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

#include "MatrixHal.h"

#include "MapValueIterator.h"

namespace android {
namespace vintf {

bool MatrixHal::operator==(const MatrixHal &other) const {
    if (format != other.format)
        return false;
    if (name != other.name)
        return false;
    if (versionRanges != other.versionRanges)
        return false;
    if (interfaces != other.interfaces)
        return false;
    // do not compare optional
    return true;
}

bool MatrixHal::containsVersion(const Version& version) const {
    for (VersionRange vRange : versionRanges) {
        if (vRange.contains(version)) return true;
    }
    return false;
}

std::set<std::string> MatrixHal::getInstances(const std::string& interfaceName) const {
    std::set<std::string> ret;
    auto it = interfaces.find(interfaceName);
    if (it != interfaces.end()) {
        ret.insert(it->second.instances.begin(), it->second.instances.end());
    }
    return ret;
}

bool MatrixHal::containsInstances(const MatrixHal& other) const {
    for (const auto& pair : other.interfaces) {
        const std::string& interfaceName = pair.first;
        auto thisIt = interfaces.find(interfaceName);
        if (thisIt == interfaces.end()) {
            return false;
        }

        const std::set<std::string>& thisInstances = thisIt->second.instances;
        const std::set<std::string>& otherInstances = pair.second.instances;
        if (!std::includes(thisInstances.begin(), thisInstances.end(), otherInstances.begin(),
                           otherInstances.end())) {
            return false;
        }
    }
    return true;
}

bool MatrixHal::forEachInstance(const std::function<bool(const MatrixInstance&)>& func) const {
    for (const auto& vr : versionRanges) {
        for (const auto& intf : iterateValues(interfaces)) {
            for (const auto& instance : intf.instances) {
                // TODO(b/73556059): Store MatrixInstance as well to avoid creating temps
                FqInstance fqInstance;
                if (fqInstance.setTo(getName(), vr.majorVer, vr.minMinor, intf.name, instance)) {
                    if (!func(MatrixInstance(std::move(fqInstance), VersionRange(vr), optional))) {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

} // namespace vintf
} // namespace android
