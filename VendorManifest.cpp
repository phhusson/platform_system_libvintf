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

#include "VendorManifest.h"

#include "parse_xml.h"
#include "CompatibilityMatrix.h"

#include <dirent.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <mutex>

#include <android-base/logging.h>

#define MANIFEST_PATH "/vendor/manifest/"
#define MANIFEST_FILE "/vendor/manifest.xml"

namespace android {
namespace vintf {

constexpr Version VendorManifest::kVersion;

bool VendorManifest::add(ManifestHal &&hal) {
    return hal.isValid() && hals.emplace(hal.name, std::move(hal)).second;
}

const ManifestHal *VendorManifest::getHal(const std::string &name) const {
    const auto it = hals.find(name);
    if (it == hals.end()) {
        return nullptr;
    }
    return &(it->second);
}

Transport VendorManifest::getTransport(const std::string &name, const Version &v) const {
    const ManifestHal *hal = getHal(name);
    if (hal == nullptr) {
        return Transport::EMPTY;
    }
    if (std::find(hal->versions.begin(), hal->versions.end(), v) == hal->versions.end()) {
        LOG(WARNING) << "VendorManifest::getTransport: Cannot find "
                     << v.majorVer << "." << v.minorVer << " in supported versions of " << name;
        return Transport::EMPTY;
    }
    return hal->transport;
}

ConstMapValueIterable<std::string, ManifestHal> VendorManifest::getHals() const {
    return ConstMapValueIterable<std::string, ManifestHal>(hals);
}

const std::vector<Version> &VendorManifest::getSupportedVersions(const std::string &name) const {
    static const std::vector<Version> empty{};
    const ManifestHal *hal = getHal(name);
    if (hal == nullptr) {
        return empty;
    }
    return hal->versions;
}

static bool isCompatible(const MatrixHal &matrixHal, const ManifestHal &manifestHal) {
    if (matrixHal.format != manifestHal.format) {
        return false;
    }
    // don't check optional

    for (const Version &manifestVersion : manifestHal.versions) {
        for (const VersionRange &matrixVersionRange : matrixHal.versionRanges) {
            // If Compatibility Matrix says 2.5-2.7, the "2.7" is purely informational;
            // the framework can work with all 2.5-2.infinity.
            if (manifestVersion.majorVer == matrixVersionRange.majorVer &&
                manifestVersion.minorVer >= matrixVersionRange.minMinor) {
                return true;
            }
        }
    }
    return false;
}

std::vector<std::string> VendorManifest::checkIncompatiblity(const CompatibilityMatrix &mat) const {
    std::vector<std::string> incompatible;
    for (const MatrixHal &matrixHal : mat.getHals()) {
        // don't check optional; put it in the incompatibility list as well.
        const std::string &name = matrixHal.name;
        auto it = hals.find(name);
        if (it == hals.end()) {
            incompatible.push_back(name);
            continue;
        }
        const ManifestHal &manifestHal = it->second;
        if (!isCompatible(matrixHal, manifestHal)) {
            incompatible.push_back(name);
            continue;
        }
    }
    return incompatible;
}

status_t VendorManifest::fetchAllInformation() {
#if 0
    // TODO: b/33755377 Uncomment this if we use a directory of fragments instead.
    status_t err = OK;
    DIR *dir = NULL;
    struct dirent *e;
    std::ifstream in;
    std::string buf;
    if ((dir = opendir(MANIFEST_PATH))) {
        while ((e = readdir(dir))) {
            if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) {
                continue;
            }
            in.open(e->d_name);
            in >> buf;

            err = handleFragment(this, buf); // And we will need to define this
            if (err != OK) {
                break;
            }
        }
    }
    if (dir != NULL) {
        closedir(dir);
    }
    return err;
#endif

    std::ifstream in;
    in.open(MANIFEST_FILE);
    if (!in.is_open()) {
        LOG(WARNING) << "Cannot open " MANIFEST_FILE;
        return INVALID_OPERATION;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    bool success = gVendorManifestConverter(this, ss.str());
    if (!success) {
        LOG(ERROR) << "Illformed vendor manifest: " MANIFEST_FILE << ": "
                   << gVendorManifestConverter.lastError();
        return BAD_VALUE;
    }
    return OK;
}

// static
const VendorManifest *VendorManifest::Get() {
    static VendorManifest vm{};
    static VendorManifest *vmp = nullptr;
    static std::mutex mutex{};

    std::lock_guard<std::mutex> lock(mutex);
    if (vmp == nullptr) {
        if (vm.fetchAllInformation() == OK) {
            vmp = &vm;
        } else {
            vm.clear();
            return nullptr;
        }
    }

    return vmp;
}


} // namespace vintf
} // namespace android
