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

#include "VintfObject.h"

#include "CompatibilityMatrix.h"
#include "parse_string.h"
#include "parse_xml.h"
#include "utils.h"

#include <dirent.h>

#include <functional>
#include <memory>
#include <mutex>

#include <android-base/logging.h>

using std::placeholders::_1;
using std::placeholders::_2;

namespace android {
namespace vintf {

using namespace details;

#ifdef LIBVINTF_TARGET
static constexpr bool kIsTarget = true;
#else
static constexpr bool kIsTarget = false;
#endif

template <typename T, typename F>
static std::shared_ptr<const T> Get(
        LockedSharedPtr<T> *ptr,
        bool skipCache,
        const F &fetchAllInformation) {
    std::unique_lock<std::mutex> _lock(ptr->mutex);
    if (skipCache || !ptr->fetchedOnce) {
        ptr->object = std::make_unique<T>();
        std::string error;
        if (fetchAllInformation(ptr->object.get(), &error) != OK) {
            LOG(WARNING) << error;
            ptr->object = nullptr; // frees the old object
        }
        ptr->fetchedOnce = true;
    }
    return ptr->object;
}

static std::unique_ptr<FileSystem> createDefaultFileSystem() {
    std::unique_ptr<FileSystem> fileSystem;
    if (kIsTarget) {
        fileSystem = std::make_unique<details::FileSystemImpl>();
    } else {
        fileSystem = std::make_unique<details::FileSystemNoOp>();
    }
    return fileSystem;
}

static std::unique_ptr<PropertyFetcher> createDefaultPropertyFetcher() {
    std::unique_ptr<PropertyFetcher> propertyFetcher;
    if (kIsTarget) {
        propertyFetcher = std::make_unique<details::PropertyFetcherImpl>();
    } else {
        propertyFetcher = std::make_unique<details::PropertyFetcherNoOp>();
    }
    return propertyFetcher;
}

VintfObject::VintfObject(std::unique_ptr<FileSystem>&& fileSystem,
                         std::unique_ptr<details::PartitionMounter>&& partitionMounter,
                         std::unique_ptr<details::ObjectFactory<RuntimeInfo>>&& runtimeInfoFactory,
                         std::unique_ptr<details::PropertyFetcher>&& propertyFetcher)
    : mFileSystem(fileSystem ? std::move(fileSystem) : createDefaultFileSystem()),
      mPartitionMounter(partitionMounter ? std::move(partitionMounter)
                                         : std::make_unique<details::PartitionMounter>()),
      mRuntimeInfoFactory(runtimeInfoFactory
                              ? std::move(runtimeInfoFactory)
                              : std::make_unique<details::ObjectFactory<RuntimeInfo>>()),
      mPropertyFetcher(propertyFetcher ? std::move(propertyFetcher)
                                       : createDefaultPropertyFetcher()) {}

details::LockedSharedPtr<VintfObject> VintfObject::sInstance{};
std::shared_ptr<VintfObject> VintfObject::GetInstance() {
    std::unique_lock<std::mutex> lock(sInstance.mutex);
    if (sInstance.object == nullptr) {
        sInstance.object = std::make_shared<VintfObject>();
    }
    return sInstance.object;
}

std::shared_ptr<const HalManifest> VintfObject::GetDeviceHalManifest(bool skipCache) {
    return GetInstance()->getDeviceHalManifest(skipCache);
}

std::shared_ptr<const HalManifest> VintfObject::getDeviceHalManifest(bool skipCache) {
    return Get(&mDeviceManifest, skipCache,
               std::bind(&VintfObject::fetchDeviceHalManifest, this, _1, _2));
}

std::shared_ptr<const HalManifest> VintfObject::GetFrameworkHalManifest(bool skipCache) {
    return GetInstance()->getFrameworkHalManifest(skipCache);
}

std::shared_ptr<const HalManifest> VintfObject::getFrameworkHalManifest(bool skipCache) {
    return Get(&mFrameworkManifest, skipCache,
               std::bind(&VintfObject::fetchFrameworkHalManifest, this, _1, _2));
}

std::shared_ptr<const CompatibilityMatrix> VintfObject::GetDeviceCompatibilityMatrix(bool skipCache) {
    return GetInstance()->getDeviceCompatibilityMatrix(skipCache);
}

std::shared_ptr<const CompatibilityMatrix> VintfObject::getDeviceCompatibilityMatrix(
    bool skipCache) {
    return Get(&mDeviceMatrix, skipCache, std::bind(&VintfObject::fetchDeviceMatrix, this, _1, _2));
}

std::shared_ptr<const CompatibilityMatrix> VintfObject::GetFrameworkCompatibilityMatrix(bool skipCache) {
    return GetInstance()->getFrameworkCompatibilityMatrix(skipCache);
}

std::shared_ptr<const CompatibilityMatrix> VintfObject::getFrameworkCompatibilityMatrix(
    bool skipCache) {
    // To avoid deadlock, get device manifest before any locks.
    auto deviceManifest = getDeviceHalManifest();

    std::unique_lock<std::mutex> _lock(mFrameworkCompatibilityMatrixMutex);

    auto combined =
        Get(&mCombinedFrameworkMatrix, skipCache,
            std::bind(&VintfObject::getCombinedFrameworkMatrix, this, deviceManifest, _1, _2));
    if (combined != nullptr) {
        return combined;
    }

    return Get(&mFrameworkMatrix, skipCache,
               std::bind(&CompatibilityMatrix::fetchAllInformation, _1, mFileSystem.get(),
                         kSystemLegacyMatrix, _2));
}

status_t VintfObject::getCombinedFrameworkMatrix(
    const std::shared_ptr<const HalManifest>& deviceManifest, CompatibilityMatrix* out,
    std::string* error) {
    auto matrixFragments = getAllFrameworkMatrixLevels(error);
    if (matrixFragments.empty()) {
        return NAME_NOT_FOUND;
    }

    Level deviceLevel = Level::UNSPECIFIED;

    if (deviceManifest != nullptr) {
        deviceLevel = deviceManifest->level();
    }

    // TODO(b/70628538): Do not infer from Shipping API level.
    if (deviceLevel == Level::UNSPECIFIED) {
        auto shippingApi = mPropertyFetcher->getUintProperty("ro.product.first_api_level", 0u);
        if (shippingApi != 0u) {
            deviceLevel = details::convertFromApiLevel(shippingApi);
        }
    }

    if (deviceLevel == Level::UNSPECIFIED) {
        // Cannot infer FCM version. Combine all matrices by assuming
        // Shipping FCM Version == min(all supported FCM Versions in the framework)
        for (auto&& pair : matrixFragments) {
            Level fragmentLevel = pair.object.level();
            if (fragmentLevel != Level::UNSPECIFIED && deviceLevel > fragmentLevel) {
                deviceLevel = fragmentLevel;
            }
        }
    }

    if (deviceLevel == Level::UNSPECIFIED) {
        // None of the fragments specify any FCM version. Should never happen except
        // for inconsistent builds.
        if (error) {
            *error = "No framework compatibility matrix files under " + kSystemVintfDir +
                     " declare FCM version.";
        }
        return NAME_NOT_FOUND;
    }

    auto combined = CompatibilityMatrix::combine(deviceLevel, &matrixFragments, error);
    if (combined == nullptr) {
        return BAD_VALUE;
    }
    *out = std::move(*combined);
    return OK;
}

// Load and combine all of the manifests in a directory
status_t VintfObject::addDirectoryManifests(const std::string& directory, HalManifest* manifest,
                                            std::string* error) {
    std::vector<std::string> fileNames;
    status_t err = mFileSystem->listFiles(directory, &fileNames, error);
    // if the directory isn't there, that's okay
    if (err == NAME_NOT_FOUND) return OK;
    if (err != OK) return err;

    for (const std::string& file : fileNames) {
        // Only adds HALs because all other things are added by libvintf
        // itself for now.
        HalManifest fragmentManifest;
        err = fetchOneHalManifest(directory + file, &fragmentManifest, error);
        if (err != OK) return err;

        manifest->addAllHals(&fragmentManifest);
    }

    return OK;
}

// Priority for loading vendor manifest:
// 1. /vendor/etc/vintf/manifest.xml + device fragments + ODM manifest (optional) + odm fragments
// 2. /vendor/etc/vintf/manifest.xml + device fragments
// 3. ODM manifest (optional) + odm fragments
// 4. /vendor/manifest.xml (legacy, no fragments)
// where:
// A + B means unioning <hal> tags from A and B. If B declares an override, then this takes priority
// over A.
status_t VintfObject::fetchDeviceHalManifest(HalManifest* out, std::string* error) {
    status_t vendorStatus = fetchOneHalManifest(kVendorManifest, out, error);
    if (vendorStatus != OK && vendorStatus != NAME_NOT_FOUND) {
        return vendorStatus;
    }

    if (vendorStatus == OK) {
        status_t fragmentStatus = addDirectoryManifests(kVendorManifestFragmentDir, out, error);
        if (fragmentStatus != OK) {
            return fragmentStatus;
        }
    }

    HalManifest odmManifest;
    status_t odmStatus = fetchOdmHalManifest(&odmManifest, error);
    if (odmStatus != OK && odmStatus != NAME_NOT_FOUND) {
        return odmStatus;
    }

    if (vendorStatus == OK) {
        if (odmStatus == OK) {
            out->addAllHals(&odmManifest);
        }
        return addDirectoryManifests(kOdmManifestFragmentDir, out, error);
    }

    // vendorStatus != OK, "out" is not changed.
    if (odmStatus == OK) {
        *out = std::move(odmManifest);
        return addDirectoryManifests(kOdmManifestFragmentDir, out, error);
    }

    // Use legacy /vendor/manifest.xml
    return out->fetchAllInformation(mFileSystem.get(), kVendorLegacyManifest, error);
}

// "out" is written to iff return status is OK.
// Priority:
// 1. if {sku} is defined, /odm/etc/vintf/manifest_{sku}.xml
// 2. /odm/etc/vintf/manifest.xml
// 3. if {sku} is defined, /odm/etc/manifest_{sku}.xml
// 4. /odm/etc/manifest.xml
// where:
// {sku} is the value of ro.boot.product.hardware.sku
status_t VintfObject::fetchOdmHalManifest(HalManifest* out, std::string* error) {
    status_t status;

    std::string productModel;
    productModel = mPropertyFetcher->getProperty("ro.boot.product.hardware.sku", "");

    if (!productModel.empty()) {
        status =
            fetchOneHalManifest(kOdmVintfDir + "manifest_" + productModel + ".xml", out, error);
        if (status == OK || status != NAME_NOT_FOUND) {
            return status;
        }
    }

    status = fetchOneHalManifest(kOdmManifest, out, error);
    if (status == OK || status != NAME_NOT_FOUND) {
        return status;
    }

    if (!productModel.empty()) {
        status = fetchOneHalManifest(kOdmLegacyVintfDir + "manifest_" + productModel + ".xml", out,
                                     error);
        if (status == OK || status != NAME_NOT_FOUND) {
            return status;
        }
    }

    status = fetchOneHalManifest(kOdmLegacyManifest, out, error);
    if (status == OK || status != NAME_NOT_FOUND) {
        return status;
    }

    return NAME_NOT_FOUND;
}

// Fetch one manifest.xml file. "out" is written to iff return status is OK.
// Returns NAME_NOT_FOUND if file is missing.
status_t VintfObject::fetchOneHalManifest(const std::string& path, HalManifest* out,
                                          std::string* error) {
    HalManifest ret;
    status_t status = ret.fetchAllInformation(mFileSystem.get(), path, error);
    if (status == OK) {
        *out = std::move(ret);
    }
    return status;
}

status_t VintfObject::fetchDeviceMatrix(CompatibilityMatrix* out, std::string* error) {
    CompatibilityMatrix etcMatrix;
    if (etcMatrix.fetchAllInformation(mFileSystem.get(), kVendorMatrix, error) == OK) {
        *out = std::move(etcMatrix);
        return OK;
    }
    return out->fetchAllInformation(mFileSystem.get(), kVendorLegacyMatrix, error);
}

status_t VintfObject::fetchFrameworkHalManifest(HalManifest* out, std::string* error) {
    HalManifest etcManifest;
    if (etcManifest.fetchAllInformation(mFileSystem.get(), kSystemManifest, error) == OK) {
        *out = std::move(etcManifest);
        return addDirectoryManifests(kSystemManifestFragmentDir, out, error);
    }
    return out->fetchAllInformation(mFileSystem.get(), kSystemLegacyManifest, error);
}

std::vector<Named<CompatibilityMatrix>> VintfObject::getAllFrameworkMatrixLevels(
    std::string* error) {
    std::vector<std::string> fileNames;
    std::vector<Named<CompatibilityMatrix>> results;

    if (mFileSystem->listFiles(kSystemVintfDir, &fileNames, error) != OK) {
        return {};
    }
    for (const std::string& fileName : fileNames) {
        std::string path = kSystemVintfDir + fileName;

        std::string content;
        std::string fetchError;
        status_t status = mFileSystem->fetch(path, &content, &fetchError);
        if (status != OK) {
            if (error) {
                *error += "Framework Matrix: Ignore file " + path + ": " + fetchError + "\n";
            }
            continue;
        }

        auto it = results.emplace(results.end());
        if (!gCompatibilityMatrixConverter(&it->object, content, error)) {
            if (error) {
                *error += "Framework Matrix: Ignore file " + path + ": " + *error + "\n";
            }
            results.erase(it);
            continue;
        }
    }

    if (results.empty()) {
        if (error) {
            *error = "No framework matrices under " + kSystemVintfDir +
                     " can be fetched or parsed.\n" + *error;
        }
    } else {
        if (error && !error->empty()) {
            LOG(WARNING) << *error;
            *error = "";
        }
    }

    return results;
}

std::shared_ptr<const RuntimeInfo> VintfObject::GetRuntimeInfo(bool skipCache,
                                                               RuntimeInfo::FetchFlags flags) {
    return GetInstance()->getRuntimeInfo(skipCache, flags);
}
std::shared_ptr<const RuntimeInfo> VintfObject::getRuntimeInfo(bool skipCache,
                                                               RuntimeInfo::FetchFlags flags) {
    std::unique_lock<std::mutex> _lock(mDeviceRuntimeInfo.mutex);

    if (!skipCache) {
        flags &= (~mDeviceRuntimeInfo.fetchedFlags);
    }

    if (mDeviceRuntimeInfo.object == nullptr) {
        mDeviceRuntimeInfo.object = mRuntimeInfoFactory->make_shared();
    }

    status_t status = mDeviceRuntimeInfo.object->fetchAllInformation(flags);
    if (status != OK) {
        mDeviceRuntimeInfo.fetchedFlags &= (~flags);  // mark the fields as "not fetched"
        return nullptr;
    }

    mDeviceRuntimeInfo.fetchedFlags |= flags;
    return mDeviceRuntimeInfo.object;
}

namespace details {

enum class ParseStatus {
    OK,
    PARSE_ERROR,
    DUPLICATED_FWK_ENTRY,
    DUPLICATED_DEV_ENTRY,
};

static std::string toString(ParseStatus status) {
    switch(status) {
        case ParseStatus::OK:                   return "OK";
        case ParseStatus::PARSE_ERROR:          return "parse error";
        case ParseStatus::DUPLICATED_FWK_ENTRY: return "duplicated framework";
        case ParseStatus::DUPLICATED_DEV_ENTRY: return "duplicated device";
    }
    return "";
}

template<typename T>
static ParseStatus tryParse(const std::string &xml, const XmlConverter<T> &parse,
        std::shared_ptr<T> *fwk, std::shared_ptr<T> *dev) {
    std::shared_ptr<T> ret = std::make_shared<T>();
    if (!parse(ret.get(), xml, nullptr /* error */)) {
        return ParseStatus::PARSE_ERROR;
    }
    if (ret->type() == SchemaType::FRAMEWORK) {
        if (fwk->get() != nullptr) {
            return ParseStatus::DUPLICATED_FWK_ENTRY;
        }
        *fwk = std::move(ret);
    } else if (ret->type() == SchemaType::DEVICE) {
        if (dev->get() != nullptr) {
            return ParseStatus::DUPLICATED_DEV_ENTRY;
        }
        *dev = std::move(ret);
    }
    return ParseStatus::OK;
}

static void appendLine(std::string* error, const std::string& message) {
    if (error != nullptr) {
        if (!error->empty()) *error += "\n";
        *error += message;
    }
}

template <typename T, typename GetFunction>
static status_t getMissing(const std::string& msg, const std::shared_ptr<T>& pkg, bool mount,
                           std::function<status_t(void)> mountFunction,
                           std::shared_ptr<const T>* updated, GetFunction getFunction,
                           std::string* error) {
    if (pkg != nullptr) {
        *updated = pkg;
    } else {
        if (mount) {
            status_t mountStatus = mountFunction();
            if (mountStatus != OK) {
                appendLine(error, "warning: mount " + msg + " failed: " + strerror(-mountStatus));
            }
        }
        *updated = getFunction();
    }
    return OK;
}

struct PackageInfo {
    struct Pair {
        std::shared_ptr<HalManifest>         manifest;
        std::shared_ptr<CompatibilityMatrix> matrix;
    };
    Pair dev;
    Pair fwk;
};

struct UpdatedInfo {
    struct Pair {
        std::shared_ptr<const HalManifest>         manifest;
        std::shared_ptr<const CompatibilityMatrix> matrix;
    };
    Pair dev;
    Pair fwk;
    std::shared_ptr<const RuntimeInfo> runtimeInfo;
};

}  // namespace details

// Checks given compatibility info against info on the device. If no
// compatability info is given then the device info will be checked against
// itself.
int32_t VintfObject::checkCompatibility(const std::vector<std::string>& xmls, bool mount,
                                        std::string* error, CheckFlags::Type flags) {
    status_t status;
    ParseStatus parseStatus;
    PackageInfo pkg; // All information from package.
    UpdatedInfo updated; // All files and runtime info after the update.

    // parse all information from package
    for (const auto &xml : xmls) {
        parseStatus = tryParse(xml, gHalManifestConverter, &pkg.fwk.manifest, &pkg.dev.manifest);
        if (parseStatus == ParseStatus::OK) {
            continue; // work on next one
        }
        if (parseStatus != ParseStatus::PARSE_ERROR) {
            appendLine(error, toString(parseStatus) + " manifest");
            return ALREADY_EXISTS;
        }
        parseStatus = tryParse(xml, gCompatibilityMatrixConverter, &pkg.fwk.matrix, &pkg.dev.matrix);
        if (parseStatus == ParseStatus::OK) {
            continue; // work on next one
        }
        if (parseStatus != ParseStatus::PARSE_ERROR) {
            appendLine(error, toString(parseStatus) + " matrix");
            return ALREADY_EXISTS;
        }
        appendLine(error, toString(parseStatus));  // parse error
        return BAD_VALUE;
    }

    // get missing info from device
    // use functions instead of std::bind because std::bind doesn't work well with mock objects
    auto mountSystem = [this] { return this->mPartitionMounter->mountSystem(); };
    auto mountVendor = [this] { return this->mPartitionMounter->mountVendor(); };
    if ((status = getMissing(
             "system", pkg.fwk.manifest, mount, mountSystem, &updated.fwk.manifest,
             std::bind(&VintfObject::getFrameworkHalManifest, this, true /* skipCache */),
             error)) != OK) {
        return status;
    }
    if ((status =
             getMissing("vendor", pkg.dev.manifest, mount, mountVendor, &updated.dev.manifest,
                        std::bind(&VintfObject::getDeviceHalManifest, this, true /* skipCache */),
                        error)) != OK) {
        return status;
    }
    if ((status = getMissing(
             "system", pkg.fwk.matrix, mount, mountSystem, &updated.fwk.matrix,
             std::bind(&VintfObject::getFrameworkCompatibilityMatrix, this, true /* skipCache */),
             error)) != OK) {
        return status;
    }
    if ((status = getMissing(
             "vendor", pkg.dev.matrix, mount, mountVendor, &updated.dev.matrix,
             std::bind(&VintfObject::getDeviceCompatibilityMatrix, this, true /* skipCache */),
             error)) != OK) {
        return status;
    }

    if (mount) {
        status_t umountStatus = mPartitionMounter->umountSystem();
        if (umountStatus != OK) {
            appendLine(error,
                       std::string{"warning: umount system failed: "} + strerror(-umountStatus));
        }
        umountStatus = mPartitionMounter->umountVendor();
        if (umountStatus != OK) {
            appendLine(error,
                       std::string{"warning: umount vendor failed: "} + strerror(-umountStatus));
        }
    }

    if (flags.isRuntimeInfoEnabled()) {
        updated.runtimeInfo = getRuntimeInfo(true /* skipCache */);
    }

    // null checks for files and runtime info after the update
    if (updated.fwk.manifest == nullptr) {
        appendLine(error, "No framework manifest file from device or from update package");
        status = NO_INIT;
    }
    if (updated.dev.manifest == nullptr) {
        appendLine(error, "No device manifest file from device or from update package");
        status = NO_INIT;
    }
    if (updated.fwk.matrix == nullptr) {
        appendLine(error, "No framework matrix file from device or from update package");
        status = NO_INIT;
    }
    if (updated.dev.matrix == nullptr) {
        appendLine(error, "No device matrix file from device or from update package");
        status = NO_INIT;
    }

    if (flags.isRuntimeInfoEnabled()) {
        if (updated.runtimeInfo == nullptr) {
            appendLine(error, "No runtime info from device");
            status = NO_INIT;
        }
    }
    if (status != OK) return status;

    // compatiblity check.
    if (!updated.dev.manifest->checkCompatibility(*updated.fwk.matrix, error)) {
        if (error) {
            error->insert(0,
                          "Device manifest and framework compatibility matrix are incompatible: ");
        }
        return INCOMPATIBLE;
    }
    if (!updated.fwk.manifest->checkCompatibility(*updated.dev.matrix, error)) {
        if (error) {
            error->insert(0,
                          "Framework manifest and device compatibility matrix are incompatible: ");
        }
        return INCOMPATIBLE;
    }

    if (flags.isRuntimeInfoEnabled()) {
        if (!updated.runtimeInfo->checkCompatibility(*updated.fwk.matrix, error, flags)) {
            if (error) {
                error->insert(0,
                              "Runtime info and framework compatibility matrix are incompatible: ");
            }
            return INCOMPATIBLE;
        }
    }

    return COMPATIBLE;
}

namespace details {

const std::string kSystemVintfDir = "/system/etc/vintf/";
const std::string kVendorVintfDir = "/vendor/etc/vintf/";
const std::string kOdmVintfDir = "/odm/etc/vintf/";

const std::string kVendorManifest = kVendorVintfDir + "manifest.xml";
const std::string kSystemManifest = kSystemVintfDir + "manifest.xml";
const std::string kVendorMatrix = kVendorVintfDir + "compatibility_matrix.xml";
const std::string kOdmManifest = kOdmVintfDir + "manifest.xml";

const std::string kVendorManifestFragmentDir = kVendorVintfDir + "manifest/";
const std::string kSystemManifestFragmentDir = kSystemVintfDir + "manifest/";
const std::string kOdmManifestFragmentDir = kOdmVintfDir + "manifest/";

const std::string kVendorLegacyManifest = "/vendor/manifest.xml";
const std::string kVendorLegacyMatrix = "/vendor/compatibility_matrix.xml";
const std::string kSystemLegacyManifest = "/system/manifest.xml";
const std::string kSystemLegacyMatrix = "/system/compatibility_matrix.xml";
const std::string kOdmLegacyVintfDir = "/odm/etc/";
const std::string kOdmLegacyManifest = kOdmLegacyVintfDir + "manifest.xml";

std::vector<std::string> dumpFileList() {
    return {
        kSystemVintfDir,       kVendorVintfDir,     kOdmVintfDir,          kOdmLegacyVintfDir,

        kVendorLegacyManifest, kVendorLegacyMatrix, kSystemLegacyManifest, kSystemLegacyMatrix,
    };
}

}  // namespace details

int32_t VintfObject::CheckCompatibility(const std::vector<std::string>& xmls, std::string* error,
                                        CheckFlags::Type flags) {
    return GetInstance()->checkCompatibility(xmls, error, flags);
}

int32_t VintfObject::checkCompatibility(const std::vector<std::string>& xmls, std::string* error,
                                        CheckFlags::Type flags) {
    return checkCompatibility(xmls, false /* mount */, error, flags);
}

bool VintfObject::IsHalDeprecated(const MatrixHal& oldMatrixHal,
                                  const CompatibilityMatrix& targetMatrix,
                                  const ListInstances& listInstances, std::string* error) {
    bool isDeprecated = false;
    oldMatrixHal.forEachInstance([&](const MatrixInstance& oldMatrixInstance) {
        if (IsInstanceDeprecated(oldMatrixInstance, targetMatrix, listInstances, error)) {
            isDeprecated = true;
        }
        return !isDeprecated;  // continue if no deprecated instance is found.
    });
    return isDeprecated;
}

// Let oldMatrixInstance = package@x.y-w::interface with instancePattern.
// If any "servedInstance" in listInstances(package@x.y::interface) matches instancePattern, return
// true iff:
// 1. package@x.?::interface/servedInstance is not in targetMatrix; OR
// 2. package@x.z::interface/servedInstance is in targetMatrix but
//    servedInstance is not in listInstances(package@x.z::interface)
bool VintfObject::IsInstanceDeprecated(const MatrixInstance& oldMatrixInstance,
                                       const CompatibilityMatrix& targetMatrix,
                                       const ListInstances& listInstances, std::string* error) {
    const std::string& package = oldMatrixInstance.package();
    const Version& version = oldMatrixInstance.versionRange().minVer();
    const std::string& interface = oldMatrixInstance.interface();

    std::vector<std::string> instanceHint;
    if (!oldMatrixInstance.isRegex()) {
        instanceHint.push_back(oldMatrixInstance.exactInstance());
    }

    auto list = listInstances(package, version, interface, instanceHint);
    for (const auto& pair : list) {
        const std::string& servedInstance = pair.first;
        Version servedVersion = pair.second;
        if (!oldMatrixInstance.matchInstance(servedInstance)) {
            continue;
        }

        // Find any package@x.? in target matrix, and check if instance is in target matrix.
        bool foundInstance = false;
        Version targetMatrixMinVer;
        targetMatrix.forEachInstanceOfPackage(package, [&](const auto& targetMatrixInstance) {
            if (targetMatrixInstance.versionRange().majorVer == version.majorVer &&
                targetMatrixInstance.interface() == interface &&
                targetMatrixInstance.matchInstance(servedInstance)) {
                targetMatrixMinVer = targetMatrixInstance.versionRange().minVer();
                foundInstance = true;
            }
            return !foundInstance;  // continue if not found
        });
        if (!foundInstance) {
            if (error) {
                *error = toFQNameString(package, servedVersion, interface, servedInstance) +
                         " is deprecated in compatibility matrix at FCM Version " +
                         to_string(targetMatrix.level()) + "; it should not be served.";
            }
            return true;
        }

        // Assuming that targetMatrix requires @x.u-v, require that at least @x.u is served.
        bool targetVersionServed = false;
        for (const auto& newPair :
             listInstances(package, targetMatrixMinVer, interface, instanceHint)) {
            if (newPair.first == servedInstance) {
                targetVersionServed = true;
                break;
            }
        }

        if (!targetVersionServed) {
            if (error) {
                *error += toFQNameString(package, servedVersion, interface, servedInstance) +
                          " is deprecated; requires at least " + to_string(targetMatrixMinVer) +
                          "\n";
            }
            return true;
        }
    }

    return false;
}

int32_t VintfObject::CheckDeprecation(const ListInstances& listInstances, std::string* error) {
    return GetInstance()->checkDeprecation(listInstances, error);
}
int32_t VintfObject::checkDeprecation(const ListInstances& listInstances, std::string* error) {
    auto matrixFragments = getAllFrameworkMatrixLevels(error);
    if (matrixFragments.empty()) {
        if (error && error->empty())
            *error = "Cannot get framework matrix for each FCM version for unknown error.";
        return NAME_NOT_FOUND;
    }
    auto deviceManifest = getDeviceHalManifest();
    if (deviceManifest == nullptr) {
        if (error) *error = "No device manifest.";
        return NAME_NOT_FOUND;
    }
    Level deviceLevel = deviceManifest->level();
    if (deviceLevel == Level::UNSPECIFIED) {
        if (error) *error = "Device manifest does not specify Shipping FCM Version.";
        return BAD_VALUE;
    }

    const CompatibilityMatrix* targetMatrix = nullptr;
    for (const auto& namedMatrix : matrixFragments) {
        if (namedMatrix.object.level() == deviceLevel) {
            targetMatrix = &namedMatrix.object;
        }
    }
    if (targetMatrix == nullptr) {
        if (error)
            *error = "Cannot find framework matrix at FCM version " + to_string(deviceLevel) + ".";
        return NAME_NOT_FOUND;
    }

    bool hasDeprecatedHals = false;
    for (const auto& namedMatrix : matrixFragments) {
        if (namedMatrix.object.level() == Level::UNSPECIFIED) continue;
        if (namedMatrix.object.level() >= deviceLevel) continue;

        const auto& oldMatrix = namedMatrix.object;
        for (const MatrixHal& hal : oldMatrix.getHals()) {
            hasDeprecatedHals |= IsHalDeprecated(hal, *targetMatrix, listInstances, error);
        }
    }

    return hasDeprecatedHals ? DEPRECATED : NO_DEPRECATED_HALS;
}

int32_t VintfObject::CheckDeprecation(std::string* error) {
    return GetInstance()->checkDeprecation(error);
}
int32_t VintfObject::checkDeprecation(std::string* error) {
    using namespace std::placeholders;
    auto deviceManifest = getDeviceHalManifest();
    ListInstances inManifest =
        [&deviceManifest](const std::string& package, Version version, const std::string& interface,
                          const std::vector<std::string>& /* hintInstances */) {
            std::vector<std::pair<std::string, Version>> ret;
            deviceManifest->forEachInstanceOfInterface(
                package, version, interface, [&ret](const ManifestInstance& manifestInstance) {
                    ret.push_back(
                        std::make_pair(manifestInstance.instance(), manifestInstance.version()));
                    return true;
                });
            return ret;
        };
    return checkDeprecation(inManifest, error);
}

const std::unique_ptr<FileSystem>& VintfObject::getFileSystem() {
    return mFileSystem;
}

const std::unique_ptr<PartitionMounter>& VintfObject::getPartitionMounter() {
    return mPartitionMounter;
}

const std::unique_ptr<PropertyFetcher>& VintfObject::getPropertyFetcher() {
    return mPropertyFetcher;
}

const std::unique_ptr<details::ObjectFactory<RuntimeInfo>>& VintfObject::getRuntimeInfoFactory() {
    return mRuntimeInfoFactory;
}

} // namespace vintf
} // namespace android
