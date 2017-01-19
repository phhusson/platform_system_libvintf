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

#include "VendorManifest.h"

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

// TODO Need to grab all fragments here.
status_t VendorManifest::fetchAllInformation() {
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
