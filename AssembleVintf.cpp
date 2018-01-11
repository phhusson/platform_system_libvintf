/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>

#include <android-base/file.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>

#include <vintf/AssembleVintf.h>
#include <vintf/KernelConfigParser.h>
#include <vintf/parse_string.h>
#include <vintf/parse_xml.h>
#include "utils.h"

#define BUFFER_SIZE sysconf(_SC_PAGESIZE)

namespace android {
namespace vintf {

static const std::string gConfigPrefix = "android-base-";
static const std::string gConfigSuffix = ".cfg";
static const std::string gBaseConfig = "android-base.cfg";

// An input stream with a name.
// The input stream may be an actual file, or a stringstream for testing.
// It takes ownership on the istream.
class NamedIstream {
   public:
    NamedIstream(const std::string& name, std::unique_ptr<std::istream>&& stream)
        : mName(name), mStream(std::move(stream)) {}
    const std::string& name() const { return mName; }
    std::istream& stream() { return *mStream; }

   private:
    std::string mName;
    std::unique_ptr<std::istream> mStream;
};

/**
 * Slurps the device manifest file and add build time flag to it.
 */
class AssembleVintfImpl : public AssembleVintf {
    using Condition = std::unique_ptr<KernelConfig>;
    using ConditionedConfig = std::pair<Condition, std::vector<KernelConfig> /* configs */>;

   public:
    void setFakeEnv(const std::string& key, const std::string& value) { mFakeEnv[key] = value; }

    std::string getEnv(const std::string& key) const {
        auto it = mFakeEnv.find(key);
        if (it != mFakeEnv.end()) {
            return it->second;
        }
        const char* envValue = getenv(key.c_str());
        return envValue != nullptr ? std::string(envValue) : std::string();
    }

    template <typename T>
    bool getFlag(const std::string& key, T* value) const {
        std::string envValue = getEnv(key);
        if (envValue.empty()) {
            std::cerr << "Warning: " << key << " is missing, defaulted to " << (*value) << "."
                      << std::endl;
            return true;
        }

        if (!parse(envValue, value)) {
            std::cerr << "Cannot parse " << envValue << "." << std::endl;
            return false;
        }
        return true;
    }

    /**
     * Set *out to environment variable if *out is not a dummy value (i.e. default constructed).
     */
    template <typename T>
    bool getFlagIfUnset(const std::string& envKey, T* out) const {
        bool hasExistingValue = !(*out == T{});

        bool hasEnvValue = false;
        T envValue;
        std::string envStrValue = getEnv(envKey);
        if (!envStrValue.empty()) {
            if (!parse(envStrValue, &envValue)) {
                std::cerr << "Cannot parse " << envValue << "." << std::endl;
                return false;
            }
            hasEnvValue = true;
        }

        if (hasExistingValue) {
            if (hasEnvValue) {
                std::cerr << "Warning: cannot override existing value " << *out << " with "
                          << envKey << " (which is " << envValue << ")." << std::endl;
            }
            return false;
        }
        if (!hasEnvValue) {
            std::cerr << "Warning: " << envKey << " is not specified. Default to " << T{} << "."
                      << std::endl;
            return false;
        }
        *out = envValue;
        return true;
    }

    bool getBooleanFlag(const std::string& key) const { return getEnv(key) == std::string("true"); }

    size_t getIntegerFlag(const std::string& key, size_t defaultValue = 0) const {
        std::string envValue = getEnv(key);
        if (envValue.empty()) {
            return defaultValue;
        }
        size_t value;
        if (!base::ParseUint(envValue, &value)) {
            std::cerr << "Error: " << key << " must be a number." << std::endl;
            return defaultValue;
        }
        return value;
    }

    static std::string read(std::basic_istream<char>& is) {
        std::stringstream ss;
        ss << is.rdbuf();
        return ss.str();
    }

    static bool isCommonConfig(const std::string& path) {
        return ::android::base::Basename(path) == gBaseConfig;
    }

    static Level convertFromApiLevel(size_t apiLevel) {
        if (apiLevel < 26) {
            return Level::LEGACY;
        } else if (apiLevel == 26) {
            return Level::O;
        } else if (apiLevel == 27) {
            return Level::O_MR1;
        } else {
            return Level::UNSPECIFIED;
        }
    }

    // nullptr on any error, otherwise the condition.
    static Condition generateCondition(const std::string& path) {
        std::string fname = ::android::base::Basename(path);
        if (fname.size() <= gConfigPrefix.size() + gConfigSuffix.size() ||
            !std::equal(gConfigPrefix.begin(), gConfigPrefix.end(), fname.begin()) ||
            !std::equal(gConfigSuffix.rbegin(), gConfigSuffix.rend(), fname.rbegin())) {
            return nullptr;
        }

        std::string sub = fname.substr(gConfigPrefix.size(),
                                       fname.size() - gConfigPrefix.size() - gConfigSuffix.size());
        if (sub.empty()) {
            return nullptr;  // should not happen
        }
        for (size_t i = 0; i < sub.size(); ++i) {
            if (sub[i] == '-') {
                sub[i] = '_';
                continue;
            }
            if (isalnum(sub[i])) {
                sub[i] = toupper(sub[i]);
                continue;
            }
            std::cerr << "'" << fname << "' (in " << path
                      << ") is not a valid kernel config file name. Must match regex: "
                      << "android-base(-[0-9a-zA-Z-]+)?\\.cfg" << std::endl;
            return nullptr;
        }
        sub.insert(0, "CONFIG_");
        return std::make_unique<KernelConfig>(std::move(sub), Tristate::YES);
    }

    static bool parseFileForKernelConfigs(std::basic_istream<char>& stream,
                                          std::vector<KernelConfig>* out) {
        KernelConfigParser parser(true /* processComments */, true /* relaxedFormat */);
        std::string content = read(stream);
        status_t err = parser.process(content.c_str(), content.size());
        if (err != OK) {
            std::cerr << parser.error();
            return false;
        }
        err = parser.finish();
        if (err != OK) {
            std::cerr << parser.error();
            return false;
        }

        for (auto& configPair : parser.configs()) {
            out->push_back({});
            KernelConfig& config = out->back();
            config.first = std::move(configPair.first);
            if (!parseKernelConfigTypedValue(configPair.second, &config.second)) {
                std::cerr << "Unknown value type for key = '" << config.first << "', value = '"
                          << configPair.second << "'\n";
                return false;
            }
        }
        return true;
    }

    static bool parseFilesForKernelConfigs(std::vector<NamedIstream>* streams,
                                           std::vector<ConditionedConfig>* out) {
        out->clear();
        ConditionedConfig commonConfig;
        bool foundCommonConfig = false;
        bool ret = true;

        for (auto& namedStream : *streams) {
            if (isCommonConfig(namedStream.name())) {
                ret &= parseFileForKernelConfigs(namedStream.stream(), &commonConfig.second);
                foundCommonConfig = true;
            } else {
                Condition condition = generateCondition(namedStream.name());
                ret &= (condition != nullptr);

                std::vector<KernelConfig> kernelConfigs;
                if ((ret &= parseFileForKernelConfigs(namedStream.stream(), &kernelConfigs)))
                    out->emplace_back(std::move(condition), std::move(kernelConfigs));
            }
        }

        if (!foundCommonConfig) {
            std::cerr << "No android-base.cfg is found in these paths:" << std::endl;
            for (auto& namedStream : *streams) {
                std::cerr << "    " << namedStream.name() << std::endl;
            }
        }
        ret &= foundCommonConfig;
        // first element is always common configs (no conditions).
        out->insert(out->begin(), std::move(commonConfig));
        return ret;
    }

    static std::string getFileNameFromPath(std::string path) {
        auto idx = path.find_last_of("\\/");
        if (idx != std::string::npos) {
            path.erase(0, idx + 1);
        }
        return path;
    }

    std::basic_ostream<char>& out() const { return mOutRef == nullptr ? std::cout : *mOutRef; }

    template <typename S>
    using Schemas = std::vector<Named<S>>;
    using HalManifests = Schemas<HalManifest>;
    using CompatibilityMatrices = Schemas<CompatibilityMatrix>;

    bool assembleHalManifest(HalManifests* halManifests) {
        std::string error;
        HalManifest* halManifest = &halManifests->front().object;
        for (auto it = halManifests->begin() + 1; it != halManifests->end(); ++it) {
            const std::string& path = it->name;
            HalManifest& halToAdd = it->object;

            if (halToAdd.level() != Level::UNSPECIFIED) {
                if (halManifest->level() == Level::UNSPECIFIED) {
                    halManifest->mLevel = halToAdd.level();
                } else if (halManifest->level() != halToAdd.level()) {
                    std::cerr << "Inconsistent FCM Version in HAL manifests:" << std::endl
                              << "    File '" << halManifests->front().name << "' has level "
                              << halManifest->level() << std::endl
                              << "    File '" << path << "' has level " << halToAdd.level()
                              << std::endl;
                    return false;
                }
            }

            if (!halManifest->addAllHals(&halToAdd, &error)) {
                std::cerr << "File \"" << path << "\" cannot be added: conflict on HAL \"" << error
                          << "\" with an existing HAL. See <hal> with the same name "
                          << "in previously parsed files or previously declared in this file."
                          << std::endl;
                return false;
            }
        }

        if (halManifest->mType == SchemaType::DEVICE) {
            if (!getFlag("BOARD_SEPOLICY_VERS", &halManifest->device.mSepolicyVersion)) {
                return false;
            }
            if (!setDeviceFcmVersion(halManifest)) {
                return false;
            }
        }

        if (mOutputMatrix) {
            CompatibilityMatrix generatedMatrix = halManifest->generateCompatibleMatrix();
            if (!halManifest->checkCompatibility(generatedMatrix, &error)) {
                std::cerr << "FATAL ERROR: cannot generate a compatible matrix: " << error
                          << std::endl;
            }
            out() << "<!-- \n"
                     "    Autogenerated skeleton compatibility matrix. \n"
                     "    Use with caution. Modify it to suit your needs.\n"
                     "    All HALs are set to optional.\n"
                     "    Many entries other than HALs are zero-filled and\n"
                     "    require human attention. \n"
                     "-->\n"
                  << gCompatibilityMatrixConverter(generatedMatrix, mSerializeFlags);
        } else {
            out() << gHalManifestConverter(*halManifest, mSerializeFlags);
        }
        out().flush();

        if (mCheckFile != nullptr) {
            CompatibilityMatrix checkMatrix;
            if (!gCompatibilityMatrixConverter(&checkMatrix, read(*mCheckFile))) {
                std::cerr << "Cannot parse check file as a compatibility matrix: "
                          << gCompatibilityMatrixConverter.lastError() << std::endl;
                return false;
            }
            if (!halManifest->checkCompatibility(checkMatrix, &error)) {
                std::cerr << "Not compatible: " << error << std::endl;
                return false;
            }
        }

        return true;
    }

    bool assembleFrameworkCompatibilityMatrixKernels(CompatibilityMatrix* matrix) {
        for (auto& pair : mKernels) {
            std::vector<ConditionedConfig> conditionedConfigs;
            if (!parseFilesForKernelConfigs(&pair.second, &conditionedConfigs)) {
                return false;
            }
            for (ConditionedConfig& conditionedConfig : conditionedConfigs) {
                MatrixKernel kernel(KernelVersion{pair.first.majorVer, pair.first.minorVer, 0u},
                                    std::move(conditionedConfig.second));
                if (conditionedConfig.first != nullptr)
                    kernel.mConditions.push_back(std::move(*conditionedConfig.first));
                matrix->framework.mKernels.push_back(std::move(kernel));
            }
        }
        return true;
    }

    bool setDeviceFcmVersion(HalManifest* manifest) {
        size_t shippingApiLevel = getIntegerFlag("PRODUCT_SHIPPING_API_LEVEL");

        if (manifest->level() != Level::UNSPECIFIED) {
            return true;
        }
        if (!getBooleanFlag("PRODUCT_ENFORCE_VINTF_MANIFEST")) {
            manifest->mLevel = Level::LEGACY;
            return true;
        }
        // TODO(b/70628538): Do not infer from Shipping API level.
        if (shippingApiLevel) {
            std::cerr << "Warning: Shipping FCM Version is inferred from Shipping API level. "
                      << "Declare Shipping FCM Version in device manifest directly." << std::endl;
            manifest->mLevel = convertFromApiLevel(shippingApiLevel);
            if (manifest->mLevel == Level::UNSPECIFIED) {
                std::cerr << "Error: Shipping FCM Version cannot be inferred from Shipping API "
                          << "level " << shippingApiLevel << "."
                          << "Declare Shipping FCM Version in device manifest directly."
                          << std::endl;
                return false;
            }
            return true;
        }
        // TODO(b/69638851): should be an error if Shipping API level is not defined.
        // For now, just leave it empty; when framework compatibility matrix is built,
        // lowest FCM Version is assumed.
        std::cerr << "Warning: Shipping FCM Version cannot be inferred, because:" << std::endl
                  << "    (1) It is not explicitly declared in device manifest;" << std::endl
                  << "    (2) PRODUCT_ENFORCE_VINTF_MANIFEST is set to true;" << std::endl
                  << "    (3) PRODUCT_SHIPPING_API_LEVEL is undefined." << std::endl
                  << "Assuming 'unspecified' Shipping FCM Version. " << std::endl
                  << "To remove this warning, define 'level' attribute in device manifest."
                  << std::endl;
        return true;
    }

    Level getLowestFcmVersion(const CompatibilityMatrices& matrices) {
        Level ret = Level::UNSPECIFIED;
        for (const auto& e : matrices) {
            if (ret == Level::UNSPECIFIED || ret > e.object.level()) {
                ret = e.object.level();
            }
        }
        return ret;
    }

    bool assembleCompatibilityMatrix(CompatibilityMatrices* matrices) {
        std::string error;
        CompatibilityMatrix* matrix = nullptr;
        std::unique_ptr<HalManifest> checkManifest;
        if (matrices->front().object.mType == SchemaType::DEVICE) {
            matrix = &matrices->front().object;
        }

        if (matrices->front().object.mType == SchemaType::FRAMEWORK) {
            Level deviceLevel = Level::UNSPECIFIED;
            if (mCheckFile != nullptr) {
                checkManifest = std::make_unique<HalManifest>();
                if (!gHalManifestConverter(checkManifest.get(), read(*mCheckFile))) {
                    std::cerr << "Cannot parse check file as a HAL manifest: "
                              << gHalManifestConverter.lastError() << std::endl;
                    return false;
                }
                deviceLevel = checkManifest->level();
            }

            if (deviceLevel == Level::UNSPECIFIED) {
                // For GSI build, legacy devices that do not have a HAL manifest,
                // and devices in development, merge all compatibility matrices.
                deviceLevel = getLowestFcmVersion(*matrices);
            }

            if (deviceLevel == Level::UNSPECIFIED) {
                // building empty.xml
                matrix = &matrices->front().object;
            } else {
                matrix = CompatibilityMatrix::combine(deviceLevel, matrices, &error);
                if (matrix == nullptr) {
                    std::cerr << error << std::endl;
                    return false;
                }
            }

            if (!assembleFrameworkCompatibilityMatrixKernels(matrix)) {
                return false;
            }

            // set sepolicy.sepolicy-version to BOARD_SEPOLICY_VERS when none is specified.
            std::vector<VersionRange>* sepolicyVrs =
                &matrix->framework.mSepolicy.mSepolicyVersionRanges;
            VersionRange sepolicyVr;
            if (!sepolicyVrs->empty()) sepolicyVr = sepolicyVrs->front();
            if (getFlagIfUnset("BOARD_SEPOLICY_VERS", &sepolicyVr)) {
                *sepolicyVrs = {{sepolicyVr}};
            }

            getFlagIfUnset("POLICYVERS", &matrix->framework.mSepolicy.mKernelSepolicyVersion);
            getFlagIfUnset("FRAMEWORK_VBMETA_VERSION", &matrix->framework.mAvbMetaVersion);

            out() << "<!--" << std::endl;
            out() << "    Input:" << std::endl;
            for (const auto& e : *matrices) {
                if (!e.name.empty()) {
                    out() << "        " << getFileNameFromPath(e.name) << std::endl;
                }
            }
            out() << "-->" << std::endl;
        }
        out() << gCompatibilityMatrixConverter(*matrix, mSerializeFlags);
        out().flush();

        if (checkManifest != nullptr && getBooleanFlag("PRODUCT_ENFORCE_VINTF_MANIFEST") &&
            !checkManifest->checkCompatibility(*matrix, &error)) {
            std::cerr << "Not compatible: " << error << std::endl;
            return false;
        }

        return true;
    }

    enum AssembleStatus { SUCCESS, FAIL_AND_EXIT, TRY_NEXT };
    template <typename Schema, typename AssembleFunc>
    AssembleStatus tryAssemble(const XmlConverter<Schema>& converter, const std::string& schemaName,
                               AssembleFunc assemble) {
        Schemas<Schema> schemas;
        Schema schema;
        if (!converter(&schema, read(mInFiles.front().stream()))) {
            return TRY_NEXT;
        }
        auto firstType = schema.type();
        schemas.emplace_back(mInFiles.front().name(), std::move(schema));

        for (auto it = mInFiles.begin() + 1; it != mInFiles.end(); ++it) {
            Schema additionalSchema;
            const std::string& fileName = it->name();
            if (!converter(&additionalSchema, read(it->stream()))) {
                std::cerr << "File \"" << fileName << "\" is not a valid " << firstType << " "
                          << schemaName << " (but the first file is a valid " << firstType << " "
                          << schemaName << "). Error: " << converter.lastError() << std::endl;
                return FAIL_AND_EXIT;
            }
            if (additionalSchema.type() != firstType) {
                std::cerr << "File \"" << fileName << "\" is a " << additionalSchema.type() << " "
                          << schemaName << " (but a " << firstType << " " << schemaName
                          << " is expected)." << std::endl;
                return FAIL_AND_EXIT;
            }

            schemas.emplace_back(fileName, std::move(additionalSchema));
        }
        return assemble(&schemas) ? SUCCESS : FAIL_AND_EXIT;
    }

    bool assemble() override {
        using std::placeholders::_1;
        if (mInFiles.empty()) {
            std::cerr << "Missing input file." << std::endl;
            return false;
        }

        auto status = tryAssemble(gHalManifestConverter, "manifest",
                                  std::bind(&AssembleVintfImpl::assembleHalManifest, this, _1));
        if (status == SUCCESS) return true;
        if (status == FAIL_AND_EXIT) return false;

        resetInFiles();

        status = tryAssemble(gCompatibilityMatrixConverter, "compatibility matrix",
                             std::bind(&AssembleVintfImpl::assembleCompatibilityMatrix, this, _1));
        if (status == SUCCESS) return true;
        if (status == FAIL_AND_EXIT) return false;

        std::cerr << "Input file has unknown format." << std::endl
                  << "Error when attempting to convert to manifest: "
                  << gHalManifestConverter.lastError() << std::endl
                  << "Error when attempting to convert to compatibility matrix: "
                  << gCompatibilityMatrixConverter.lastError() << std::endl;
        return false;
    }

    std::ostream& setOutputStream(Ostream&& out) override {
        mOutRef = std::move(out);
        return *mOutRef;
    }

    std::istream& addInputStream(const std::string& name, Istream&& in) override {
        auto it = mInFiles.emplace(mInFiles.end(), name, std::move(in));
        return it->stream();
    }

    std::istream& setCheckInputStream(Istream&& in) override {
        mCheckFile = std::move(in);
        return *mCheckFile;
    }

    bool hasKernelVersion(const Version& kernelVer) const override {
        return mKernels.find(kernelVer) != mKernels.end();
    }

    std::istream& addKernelConfigInputStream(const Version& kernelVer, const std::string& name,
                                             Istream&& in) override {
        auto&& kernel = mKernels[kernelVer];
        auto it = kernel.emplace(kernel.end(), name, std::move(in));
        return it->stream();
    }

    void resetInFiles() {
        for (auto& inFile : mInFiles) {
            inFile.stream().clear();
            inFile.stream().seekg(0);
        }
    }

    void setOutputMatrix() override { mOutputMatrix = true; }

    bool setHalsOnly() override {
        if (mSerializeFlags) return false;
        mSerializeFlags |= SerializeFlag::HALS_ONLY;
        return true;
    }

    bool setNoHals() override {
        if (mSerializeFlags) return false;
        mSerializeFlags |= SerializeFlag::NO_HALS;
        return true;
    }

   private:
    std::vector<NamedIstream> mInFiles;
    Ostream mOutRef;
    Istream mCheckFile;
    bool mOutputMatrix = false;
    SerializeFlags mSerializeFlags = SerializeFlag::EVERYTHING;
    std::map<Version, std::vector<NamedIstream>> mKernels;
    std::map<std::string, std::string> mFakeEnv;
};

bool AssembleVintf::openOutFile(const std::string& path) {
    return static_cast<std::ofstream&>(setOutputStream(std::make_unique<std::ofstream>(path)))
        .is_open();
}

bool AssembleVintf::openInFile(const std::string& path) {
    return static_cast<std::ifstream&>(addInputStream(path, std::make_unique<std::ifstream>(path)))
        .is_open();
}

bool AssembleVintf::openCheckFile(const std::string& path) {
    return static_cast<std::ifstream&>(setCheckInputStream(std::make_unique<std::ifstream>(path)))
        .is_open();
}

bool AssembleVintf::addKernel(const std::string& kernelArg) {
    auto tokens = base::Split(kernelArg, ":");
    if (tokens.size() <= 1) {
        std::cerr << "Unrecognized --kernel option '" << kernelArg << "'" << std::endl;
        return false;
    }
    Version kernelVer;
    if (!parse(tokens.front(), &kernelVer)) {
        std::cerr << "Unrecognized kernel version '" << tokens.front() << "'" << std::endl;
        return false;
    }
    if (hasKernelVersion(kernelVer)) {
        std::cerr << "Multiple --kernel for " << kernelVer << " is specified." << std::endl;
        return false;
    }
    for (auto it = tokens.begin() + 1; it != tokens.end(); ++it) {
        bool opened =
            static_cast<std::ifstream&>(
                addKernelConfigInputStream(kernelVer, *it, std::make_unique<std::ifstream>(*it)))
                .is_open();
        if (!opened) {
            std::cerr << "Cannot open file '" << *it << "'." << std::endl;
            return false;
        }
    }
    return true;
}

std::unique_ptr<AssembleVintf> AssembleVintf::newInstance() {
    return std::make_unique<AssembleVintfImpl>();
}

}  // namespace vintf
}  // namespace android
