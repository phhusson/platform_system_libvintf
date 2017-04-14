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

#include "HalManifest.h"

#include <dirent.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <mutex>

#include <android-base/logging.h>

#include "parse_string.h"
#include "parse_xml.h"
#include "CompatibilityMatrix.h"

namespace android {
namespace vintf {

constexpr Version HalManifest::kVersion;

bool HalManifest::add(ManifestHal &&hal) {
    if (!hal.isValid()) {
        return false;
    }
    mHals.emplace(hal.name, std::move(hal));
    return true;
}

std::set<std::string> HalManifest::getHalNames() const {
    std::set<std::string> names{};
    for (const auto &hal : mHals) {
        names.insert(hal.first);
    }
    return names;
}

std::set<std::string> HalManifest::getInterfaceNames(const std::string &name) const {
    std::set<std::string> interfaceNames{};
    for (const ManifestHal *hal : getHals(name)) {
        for (const auto &iface : hal->interfaces) {
            interfaceNames.insert(iface.first);
        }
    }
    return interfaceNames;
}

ManifestHal *HalManifest::getAnyHal(const std::string &name) {
    auto it = mHals.find(name);
    if (it == mHals.end()) {
        return nullptr;
    }
    return &(it->second);
}

std::vector<const ManifestHal *> HalManifest::getHals(const std::string &name) const {
    std::vector<const ManifestHal *> ret;
    auto range = mHals.equal_range(name);
    for (auto it = range.first; it != range.second; ++it) {
        ret.push_back(&it->second);
    }
    return ret;
}

Transport HalManifest::getTransport(const std::string &package, const Version &v,
            const std::string &interfaceName, const std::string &instanceName) const {

    for (const ManifestHal *hal : getHals(package)) {
        if (std::find(hal->versions.begin(), hal->versions.end(), v) == hal->versions.end()) {
            LOG(INFO) << "HalManifest::getTransport(" << to_string(mType) << "): Cannot find "
                      << to_string(v) << " in supported versions of " << package;
            continue;
        }
        auto it = hal->interfaces.find(interfaceName);
        if (it == hal->interfaces.end()) {
            LOG(INFO) << "HalManifest::getTransport(" << to_string(mType) << "): Cannot find interface '"
                      << interfaceName << "' in " << package << "@" << to_string(v);
            continue;
        }
        const auto &instances = it->second.instances;
        if (instances.find(instanceName) == instances.end()) {
            LOG(INFO) << "HalManifest::getTransport(" << to_string(mType) << "): Cannot find instance '"
                      << instanceName << "' in "
                      << package << "@" << to_string(v) << "::" << interfaceName;
            continue;
        }
        return hal->transportArch.transport;
    }
    LOG(INFO) << "HalManifest::getTransport(" << to_string(mType) << "): Cannot get transport for "
                 << package << "@" << v << "::" << interfaceName << "/" << instanceName;
    return Transport::EMPTY;

}

ConstMultiMapValueIterable<std::string, ManifestHal> HalManifest::getHals() const {
    return ConstMultiMapValueIterable<std::string, ManifestHal>(mHals);
}

std::set<Version> HalManifest::getSupportedVersions(const std::string &name) const {
    std::set<Version> ret;
    for (const ManifestHal *hal : getHals(name)) {
        ret.insert(hal->versions.begin(), hal->versions.end());
    }
    return ret;
}


std::set<std::string> HalManifest::getInstances(
        const std::string &halName, const std::string &interfaceName) const {
    std::set<std::string> ret;
    for (const ManifestHal *hal : getHals(halName)) {
        auto it = hal->interfaces.find(interfaceName);
        if (it != hal->interfaces.end()) {
            ret.insert(it->second.instances.begin(), it->second.instances.end());
        }
    }
    return ret;
}

bool HalManifest::hasInstance(const std::string &halName,
        const std::string &interfaceName, const std::string &instanceName) const {
    const auto &instances = getInstances(halName, interfaceName);
    return instances.find(instanceName) != instances.end();
}

static bool isCompatible(const MatrixHal &matrixHal, const ManifestHal &manifestHal) {
    if (matrixHal.format != manifestHal.format) {
        return false;
    }
    for (const Version &manifestVersion : manifestHal.versions) {
        for (const VersionRange &matrixVersionRange : matrixHal.versionRanges) {
            // If Compatibility Matrix says 2.5-2.7, the "2.7" is purely informational;
            // the framework can work with all 2.5-2.infinity.
            if (matrixVersionRange.supportedBy(manifestVersion)) {
                return true;
            }
        }
    }
    return false;
}

// For each hal in mat, there must be a hal in manifest that supports this.
std::vector<std::string> HalManifest::checkIncompatibility(const CompatibilityMatrix &mat,
        bool includeOptional) const {
    std::vector<std::string> incompatible;
    for (const MatrixHal &matrixHal : mat.getHals()) {
        if (!includeOptional && matrixHal.optional) {
            continue;
        }
        // don't check optional; put it in the incompatibility list as well.
        const std::string &name = matrixHal.name;
        bool matrixHalSupported = false;
        for (const ManifestHal *manifestHal : getHals(name)) {
            matrixHalSupported = isCompatible(matrixHal, *manifestHal);
            if (matrixHalSupported) {
                break;
            }
        }
        if (!matrixHalSupported) {
            incompatible.push_back(name);
        }
    }
    return incompatible;
}

bool HalManifest::checkCompatibility(const CompatibilityMatrix &mat, std::string *error) const {
    if (mType == mat.mType) {
        if (error != nullptr) {
            *error = "Wrong type; checking " + to_string(mType) + " manifest against "
                    + to_string(mat.mType) + " compatibility matrix";
        }
        return false;
    }
    std::vector<std::string> incompatibleHals =
            checkIncompatibility(mat, false /* includeOptional */);
    if (!incompatibleHals.empty()) {
        if (error != nullptr) {
            *error = "HALs incompatible.";
            for (const auto &name : incompatibleHals) {
                *error += " " + name;
            }
        }
        return false;
    }
    if (mType == SchemaType::FRAMEWORK) {
    // TODO(b/36400653) enable this. It is disabled since vndk is not yet defined.
#ifdef VINTF_CHECK_VNDK
        bool match = false;
        const auto &matVndk = mat.device.mVndk;
        for (const auto &vndk : framework.mVndks) {
            if (!vndk.mVersionRange.in(matVndk.mVersionRange)) {
                continue;
            }
            // version matches, check libaries
            std::vector<std::string> diff;
            std::set_difference(matVndk.mLibraries.begin(), matVndk.mLibraries.end(),
                    vndk.mLibraries.begin(), vndk.mLibraries.end(),
                    std::inserter(diff, diff.begin()))
            if (!diff.empty()) {
                if (error != nullptr) {
                    *error = "Vndk libs incompatible.";
                    for (const auto &name : diff) {
                        *error += " " + name;
                    }
                }
                return false;
            }
            match = true;
            break;
        }
        if (!match) {
            if (error != nullptr) {
                *error = "Vndk version " + to_string(matVndk.mVersionRange) + " is not supported.";
            }
        }
#endif
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

status_t HalManifest::fetchAllInformation(const std::string &path) {
    std::ifstream in;
    in.open(path);
    if (!in.is_open()) {
        LOG(WARNING) << "Cannot open " << path;
        return INVALID_OPERATION;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    bool success = gHalManifestConverter(this, ss.str());
    if (!success) {
        LOG(ERROR) << "Illformed vendor manifest: " << path << ": "
                   << gHalManifestConverter.lastError();
        return BAD_VALUE;
    }
    return OK;
}

SchemaType HalManifest::type() const {
    return mType;
}

bool operator==(const HalManifest &lft, const HalManifest &rgt) {
    return lft.mType == rgt.mType &&
           lft.mHals == rgt.mHals &&
           (lft.mType != SchemaType::DEVICE || (
                lft.device.mSepolicyVersion == rgt.device.mSepolicyVersion)) &&
           (lft.mType != SchemaType::FRAMEWORK || (
                lft.framework.mVndks == rgt.framework.mVndks));
}

} // namespace vintf
} // namespace android
