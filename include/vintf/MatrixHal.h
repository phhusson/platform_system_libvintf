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

#ifndef ANDROID_VINTF_MATRIX_HAL_H
#define ANDROID_VINTF_MATRIX_HAL_H

#include <map>
#include <set>
#include <string>
#include <vector>

#include "HalFormat.h"
#include "HalInterface.h"
#include "VersionRange.h"

namespace android {
namespace vintf {

// A HAL entry to a compatibility matrix
struct MatrixHal {

    bool operator==(const MatrixHal &other) const;
    // Check whether the MatrixHal contains the given version.
    bool containsVersion(const Version& version) const;
    // Get all instances of the ManifestHal with given interface name.
    std::set<std::string> getInstances(const std::string& interfaceName) const;

    HalFormat format = HalFormat::HIDL;
    std::string name;
    std::vector<VersionRange> versionRanges;
    bool optional = false;
    std::map<std::string, HalInterface> interfaces;

    inline const std::string& getName() const { return name; }
    inline bool hasInterface(const std::string& interface_name) const {
        return interfaces.find(interface_name) != interfaces.end();
    }

    // Return true if "this" contains all interface/instance instances in "other".
    bool containsInstances(const MatrixHal& other) const;
};

} // namespace vintf
} // namespace android

#endif // ANDROID_VINTF_MATRIX_HAL_H
