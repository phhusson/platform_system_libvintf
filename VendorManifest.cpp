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

#include <android-base/logging.h>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <sstream>

#define MANIFEST_PATH "/vendor/manifest/"
#define MANIFEST_FILE "/vendor/manifest.xml"

namespace android {
namespace vintf {

constexpr Version VendorManifest::kVersion;

bool VendorManifest::isValid() const {
    for (const auto &pair : hals) {
        if (!pair.second.isValid()) {
            return false;
        }
    }
    return true;
}

bool VendorManifest::add(ManifestHal &&hal) {
    return hals.emplace(hal.name, std::move(hal)).second;
}

const ManifestHal *VendorManifest::getHal(const std::string &name) const {
    const auto it = hals.find(name);
    if (it == hals.end()) {
        return nullptr;
    }
    return &(it->second);
}

Transport VendorManifest::getTransport(const std::string &name) const {
    const ManifestHal *hal = getHal(name);
    if (hal == nullptr) {
        return Transport::EMPTY;
    }
    return hal->transport;
}

ConstMapValueIterable<std::string, ManifestHal> VendorManifest::getHals() const {
    return ConstMapValueIterable<std::string, ManifestHal>(hals);
}

const std::vector<Version> &VendorManifest::getSupportedVersions(const std::string &name) const {
    static const std::vector<Version> empty{};
    auto it = hals.find(name);
    if (it != hals.end()) {
        return it->second.versions;
    }
    return empty;
}

std::vector<std::string> VendorManifest::checkIncompatiblity(const CompatibilityMatrix &/*mat*/) const {
    // TODO implement this
    return std::vector<std::string>();
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
        LOG(ERROR) << "Cannot open " MANIFEST_FILE;
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
    static VendorManifest vm;
    static VendorManifest *vmp;
    if (vmp == nullptr) {
        if (vm.fetchAllInformation() == OK) {
            vmp = &vm;
        }
    }

    return vmp;
}

} // namespace vintf
} // namespace android
