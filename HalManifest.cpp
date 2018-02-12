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

#define LOG_TAG "libvintf"
#include <android-base/logging.h>

#include "HalManifest.h"

#include <dirent.h>

#include <mutex>
#include <set>

#include <android-base/strings.h>

#include "parse_string.h"
#include "parse_xml.h"
#include "utils.h"
#include "CompatibilityMatrix.h"

namespace android {
namespace vintf {

using details::Instances;
using details::InstancesOfVersion;

// Check <version> tag for all <hal> with the same name.
bool HalManifest::shouldAdd(const ManifestHal& hal) const {
    if (!hal.isValid()) {
        return false;
    }
    if (hal.isOverride) {
        return true;
    }
    auto existingHals = mHals.equal_range(hal.name);
    std::set<size_t> existingMajorVersions;
    for (auto it = existingHals.first; it != existingHals.second; ++it) {
        for (const auto& v : it->second.versions) {
            // Assume integrity on existingHals, so no check on emplace().second
            existingMajorVersions.insert(v.majorVer);
        }
    }
    for (const auto& v : hal.versions) {
        if (!existingMajorVersions.emplace(v.majorVer).second /* no insertion */) {
            return false;
        }
    }
    return true;
}

// Remove elements from "list" if p(element) returns true.
template <typename List, typename Predicate>
static void removeIf(List& list, Predicate predicate) {
    for (auto it = list.begin(); it != list.end();) {
        if (predicate(*it)) {
            it = list.erase(it);
        } else {
            ++it;
        }
    }
}

void HalManifest::removeHals(const std::string& name, size_t majorVer) {
    removeIf(mHals, [&name, majorVer](auto& existingHalPair) {
        auto& existingHal = existingHalPair.second;
        if (existingHal.name != name) {
            return false;
        }
        auto& existingVersions = existingHal.versions;
        removeIf(existingVersions, [majorVer](const auto& existingVersion) {
            return existingVersion.majorVer == majorVer;
        });
        return existingVersions.empty();
    });
}

bool HalManifest::add(ManifestHal&& halToAdd) {
    if (halToAdd.isOverride) {
        if (halToAdd.versions.empty()) {
            // Special syntax when there are no <version> tags at all. Remove all existing HALs
            // with the given name.
            mHals.erase(halToAdd.name);
        }
        // If there are <version> tags, remove all existing major versions that causes a conflict.
        for (const Version& versionToAdd : halToAdd.versions) {
            removeHals(halToAdd.name, versionToAdd.majorVer);
        }
    }

    return HalGroup::add(std::move(halToAdd));
}

bool HalManifest::shouldAddXmlFile(const ManifestXmlFile& xmlFile) const {
    auto existingXmlFiles = getXmlFiles(xmlFile.name());
    for (auto it = existingXmlFiles.first; it != existingXmlFiles.second; ++it) {
        if (xmlFile.version() == it->second.version()) {
            return false;
        }
    }
    return true;
}

std::set<std::string> HalManifest::getHalNames() const {
    std::set<std::string> names{};
    for (const auto &hal : mHals) {
        names.insert(hal.first);
    }
    return names;
}

std::set<std::string> HalManifest::getHalNamesAndVersions() const {
    std::set<std::string> names{};
    for (const auto &hal : getHals()) {
        for (const auto &version : hal.versions) {
            names.insert(hal.name + "@" + to_string(version));
        }
    }
    return names;
}

Transport HalManifest::getTransport(const std::string &package, const Version &v,
            const std::string &interfaceName, const std::string &instanceName) const {

    for (const ManifestHal *hal : getHals(package)) {
        bool found = false;
        for (auto& ver : hal->versions) {
            if (ver.majorVer == v.majorVer && ver.minorVer >= v.minorVer) {
                found = true;
                break;
            }
        }
        if (!found) {
            LOG(DEBUG) << "HalManifest::getTransport(" << to_string(mType) << "): Cannot find "
                      << to_string(v) << " in supported versions of " << package;
            continue;
        }
        auto it = hal->interfaces.find(interfaceName);
        if (it == hal->interfaces.end()) {
            LOG(DEBUG) << "HalManifest::getTransport(" << to_string(mType) << "): Cannot find interface '"
                      << interfaceName << "' in " << package << "@" << to_string(v);
            continue;
        }
        const auto &instances = it->second.instances;
        if (instances.find(instanceName) == instances.end()) {
            LOG(DEBUG) << "HalManifest::getTransport(" << to_string(mType) << "): Cannot find instance '"
                      << instanceName << "' in "
                      << package << "@" << to_string(v) << "::" << interfaceName;
            continue;
        }
        return hal->transportArch.transport;
    }
    LOG(DEBUG) << "HalManifest::getTransport(" << to_string(mType) << "): Cannot get transport for "
                 << package << "@" << v << "::" << interfaceName << "/" << instanceName;
    return Transport::EMPTY;

}

std::set<Version> HalManifest::getSupportedVersions(const std::string &name) const {
    std::set<Version> ret;
    for (const ManifestHal *hal : getHals(name)) {
        ret.insert(hal->versions.begin(), hal->versions.end());
    }
    return ret;
}

bool HalManifest::hasInstance(const std::string& halName, const Version& version,
                              const std::string& interfaceName,
                              const std::string& instanceName) const {
    const auto& instances = getInstances(halName, version, interfaceName);
    return instances.find(instanceName) != instances.end();
}

void HalManifest::forEachInstance(
    const std::function<void(const std::string&, const Version&, const std::string&,
                             const std::string&, bool*)>& f) const {
    bool stop = false;
    for (const auto& hal : getHals()) {
        for (const auto& v : hal.versions) {
            for (const auto& intf : iterateValues(hal.interfaces)) {
                for (const auto& instance : intf.instances) {
                    f(hal.name, v, intf.name, instance, &stop);
                    if (stop) break;
                }
            }
        }
    }
}

static bool satisfyVersion(const MatrixHal& matrixHal, const Version& manifestHalVersion) {
    for (const VersionRange &matrixVersionRange : matrixHal.versionRanges) {
        // If Compatibility Matrix says 2.5-2.7, the "2.7" is purely informational;
        // the framework can work with all 2.5-2.infinity.
        if (matrixVersionRange.supportedBy(manifestHalVersion)) {
            return true;
        }
    }
    return false;
}

// Check if matrixHal.interfaces is a subset of instancesOfVersion
static bool satisfyAllInstances(const MatrixHal& matrixHal,
        const InstancesOfVersion &instancesOfVersion) {
    for (const auto& matrixHalInterfacePair : matrixHal.interfaces) {
        const std::string& interface = matrixHalInterfacePair.first;
        auto it = instancesOfVersion.find(interface);
        if (it == instancesOfVersion.end()) {
            return false;
        }
        const std::set<std::string>& manifestInterfaceInstances = it->second;
        const std::set<std::string>& matrixInterfaceInstances =
                matrixHalInterfacePair.second.instances;
        if (!std::includes(manifestInterfaceInstances.begin(), manifestInterfaceInstances.end(),
                           matrixInterfaceInstances.begin(), matrixInterfaceInstances.end())) {
            return false;
        }
    }
    return true;
}

Instances HalManifest::expandInstances(const std::string& name) const {
    Instances instances;
    // Do the cross product version x interface x instance and sort them,
    // because interfaces / instances can span in multiple HALs.
    // This is efficient for small <hal> entries.
    for (const ManifestHal* manifestHal : getHals(name)) {
        for (const Version& manifestHalVersion : manifestHal->versions) {
            instances[manifestHalVersion] = {};
            for (const auto& halInterfacePair : manifestHal->interfaces) {
                const std::string& interface = halInterfacePair.first;
                const auto& toAdd = halInterfacePair.second.instances;
                instances[manifestHalVersion][interface].insert(toAdd.begin(), toAdd.end());
            }
        }
    }
    return instances;
}

bool HalManifest::isCompatible(const Instances& instances, const MatrixHal& matrixHal) const {
    for (const auto& instanceMapPair : instances) {
        const Version& manifestHalVersion = instanceMapPair.first;
        const InstancesOfVersion& instancesOfVersion = instanceMapPair.second;
        if (!satisfyVersion(matrixHal, manifestHalVersion)) {
            continue;
        }
        if (!satisfyAllInstances(matrixHal, instancesOfVersion)) {
            continue;
        }
        return true; // match!
    }
    return false;
}

static std::vector<std::string> toLines(const Instances& allInstances) {
    std::vector<std::string> lines;
    for (const auto& pair : allInstances) {
        const auto& version = pair.first;
        for (const auto& ifacePair : pair.second) {
            const auto& interface = ifacePair.first;
            for (const auto& instance : ifacePair.second) {
                lines.push_back("@" + to_string(version) + "::" + interface + "/" + instance);
            }
        }
    }
    return lines;
}

// indent = 2, {"foo"} => "foo"
// indent = 2, {"foo", "bar"} => "\n  foo\n  bar";
void multilineIndent(std::ostream& os, size_t indent, const std::vector<std::string>& lines) {
    if (lines.size() == 1) {
        os << lines.front();
        return;
    }
    for (const auto& line : lines) {
        os << "\n";
        for (size_t i = 0; i < indent; ++i) os << " ";
        os << line;
    }
}

// For each hal in mat, there must be a hal in manifest that supports this.
std::vector<std::string> HalManifest::checkIncompatibleHals(const CompatibilityMatrix& mat) const {
    std::vector<std::string> ret;
    for (const MatrixHal &matrixHal : mat.getHals()) {
        if (matrixHal.optional) {
            continue;
        }
        auto manifestInstances = expandInstances(matrixHal.name);
        if (!isCompatible(manifestInstances, matrixHal)) {
            std::ostringstream oss;
            oss << matrixHal.name << ":\n    required: ";
            multilineIndent(oss, 8, android::vintf::expandInstances(matrixHal));
            oss << "\n    provided: ";
            multilineIndent(oss, 8, toLines(manifestInstances));

            ret.insert(ret.end(), oss.str());
        }
    }
    return ret;
}

static bool checkVendorNdkCompatibility(const VendorNdk& matVendorNdk,
                                        const std::vector<VendorNdk>& manifestVendorNdk,
                                        std::string* error) {
    // For pre-P vendor images, device compatibility matrix does not specify <vendor-ndk>
    // tag. Ignore the check for these devices.
    if (matVendorNdk.version().empty()) {
        return true;
    }
    for (const auto& vndk : manifestVendorNdk) {
        if (vndk.version() != matVendorNdk.version()) {
            continue;
        }
        // version matches, check libraries
        std::vector<std::string> diff;
        std::set_difference(matVendorNdk.libraries().begin(), matVendorNdk.libraries().end(),
                            vndk.libraries().begin(), vndk.libraries().end(),
                            std::inserter(diff, diff.begin()));
        if (!diff.empty()) {
            if (error != nullptr) {
                *error = "Vndk libs incompatible for version " + matVendorNdk.version() +
                         ". These libs are not in framework manifest:";
                for (const auto& name : diff) {
                    *error += " " + name;
                }
            }
            return false;
        }
        return true;
    }

    // no match is found.
    if (error != nullptr) {
        *error = "Vndk version " + matVendorNdk.version() + " is not supported. " +
                 "Supported versions in framework manifest are:";
        for (const auto& vndk : manifestVendorNdk) {
            *error += " " + vndk.version();
        }
    }
    return false;
}

static bool checkSystemSdkCompatibility(const SystemSdk& matSystemSdk,
                                        const SystemSdk& manifestSystemSdk, std::string* error) {
    SystemSdk notSupported = matSystemSdk.removeVersions(manifestSystemSdk);
    if (!notSupported.empty()) {
        if (error) {
            *error =
                "The following System SDK versions are required by device "
                "compatibility matrix but not supported by the framework manifest: [" +
                base::Join(notSupported.versions(), ", ") + "]. Supported versions are: [" +
                base::Join(manifestSystemSdk.versions(), ", ") + "].";
        }
        return false;
    }
    return true;
}

bool HalManifest::checkCompatibility(const CompatibilityMatrix &mat, std::string *error) const {
    if (mType == mat.mType) {
        if (error != nullptr) {
            *error = "Wrong type; checking " + to_string(mType) + " manifest against "
                    + to_string(mat.mType) + " compatibility matrix";
        }
        return false;
    }
    auto incompatibleHals = checkIncompatibleHals(mat);
    if (!incompatibleHals.empty()) {
        if (error != nullptr) {
            *error = "HALs incompatible. The following requirements are not met:\n";
            for (const auto& e : incompatibleHals) {
                *error += e + "\n";
            }
        }
        return false;
    }
    if (mType == SchemaType::FRAMEWORK) {
        if (!checkVendorNdkCompatibility(mat.device.mVendorNdk, framework.mVendorNdks, error)) {
            return false;
        }

        if (!checkSystemSdkCompatibility(mat.device.mSystemSdk, framework.mSystemSdk, error)) {
            return false;
        }
    } else if (mType == SchemaType::DEVICE) {
        bool match = false;
        for (const auto &range : mat.framework.mSepolicy.sepolicyVersions()) {
            if (range.supportedBy(device.mSepolicyVersion)) {
                match = true;
                break;
            }
        }
        if (!match) {
            if (error != nullptr) {
                *error = "Sepolicy version " + to_string(device.mSepolicyVersion)
                        + " doesn't satisify the requirements.";
            }
            return false;
        }
    }

    return true;
}

CompatibilityMatrix HalManifest::generateCompatibleMatrix() const {
    CompatibilityMatrix matrix;

    for (const ManifestHal &manifestHal : getHals()) {
        MatrixHal matrixHal{
            .format = manifestHal.format,
            .name = manifestHal.name,
            .optional = true,
            .interfaces = manifestHal.interfaces
        };
        for (const Version &manifestVersion : manifestHal.versions) {
            matrixHal.versionRanges.push_back({manifestVersion.majorVer, manifestVersion.minorVer});
        }
        matrix.add(std::move(matrixHal));
    }
    if (mType == SchemaType::FRAMEWORK) {
        matrix.mType = SchemaType::DEVICE;
        // VNDK does not need to be added for compatibility
    } else if (mType == SchemaType::DEVICE) {
        matrix.mType = SchemaType::FRAMEWORK;
        matrix.framework.mSepolicy = Sepolicy(0u /* kernelSepolicyVersion */,
                {{device.mSepolicyVersion.majorVer, device.mSepolicyVersion.minorVer}});
    }

    return matrix;
}

status_t HalManifest::fetchAllInformation(const std::string& path, std::string* error) {
    return details::fetchAllInformation(path, gHalManifestConverter, this, error);
}

SchemaType HalManifest::type() const {
    return mType;
}

void HalManifest::setType(SchemaType type) {
    mType = type;
}

Level HalManifest::level() const {
    return mLevel;
}

Version HalManifest::getMetaVersion() const {
    return mMetaVersion;
}

const Version &HalManifest::sepolicyVersion() const {
    CHECK(mType == SchemaType::DEVICE);
    return device.mSepolicyVersion;
}

const std::vector<VendorNdk>& HalManifest::vendorNdks() const {
    CHECK(mType == SchemaType::FRAMEWORK);
    return framework.mVendorNdks;
}

std::string HalManifest::getXmlFilePath(const std::string& xmlFileName,
                                        const Version& version) const {
    using std::literals::string_literals::operator""s;
    auto range = getXmlFiles(xmlFileName);
    for (auto it = range.first; it != range.second; ++it) {
        const ManifestXmlFile& manifestXmlFile = it->second;
        if (manifestXmlFile.version() == version) {
            if (!manifestXmlFile.overriddenPath().empty()) {
                return manifestXmlFile.overriddenPath();
            }
            return "/"s + (type() == SchemaType::DEVICE ? "vendor" : "system") + "/etc/" +
                   xmlFileName + "_V" + std::to_string(version.majorVer) + "_" +
                   std::to_string(version.minorVer) + ".xml";
        }
    }
    return "";
}

bool operator==(const HalManifest &lft, const HalManifest &rgt) {
    return lft.mType == rgt.mType && lft.mLevel == rgt.mLevel && lft.mHals == rgt.mHals &&
           lft.mXmlFiles == rgt.mXmlFiles &&
           (lft.mType != SchemaType::DEVICE ||
            (lft.device.mSepolicyVersion == rgt.device.mSepolicyVersion)) &&
           (lft.mType != SchemaType::FRAMEWORK ||
            (
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
                lft.framework.mVndks == rgt.framework.mVndks &&
#pragma clang diagnostic pop
                lft.framework.mVendorNdks == rgt.framework.mVendorNdks &&
                lft.framework.mSystemSdk == rgt.framework.mSystemSdk));
}

} // namespace vintf
} // namespace android
