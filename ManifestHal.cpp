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

#include "ManifestHal.h"
#include <unordered_set>

#include "MapValueIterator.h"

namespace android {
namespace vintf {

bool ManifestHal::isValid() const {
    std::unordered_set<size_t> existing;
    for (const auto &v : versions) {
        if (existing.find(v.majorVer) != existing.end()) {
            return false;
        }
        existing.insert(v.majorVer);
    }
    return transportArch.isValid();
}

bool ManifestHal::operator==(const ManifestHal &other) const {
    if (format != other.format)
        return false;
    if (name != other.name)
        return false;
    if (versions != other.versions)
        return false;
    // do not compare impl
    return true;
}

std::set<std::string> ManifestHal::getInstances(const std::string& interfaceName) const {
    std::set<std::string> ret;
    auto it = interfaces.find(interfaceName);
    if (it != interfaces.end()) {
        ret.insert(it->second.instances.begin(), it->second.instances.end());
    }
    return ret;
}

bool ManifestHal::forEachInstance(const std::function<bool(const ManifestInstance&)>& func) const {
    // TODO(b/73556059): support <fqname> as well.
    for (const auto& v : versions) {
        for (const auto& intf : iterateValues(interfaces)) {
            for (const auto& instance : intf.instances) {
                // TODO(b/73556059): Store ManifestInstance as well to avoid creating temps
                FqInstance fqInstance;
                if (fqInstance.setTo(getName(), v.majorVer, v.minorVer, intf.name, instance)) {
                    if (!func(ManifestInstance(std::move(fqInstance),
                                               TransportArch{transportArch}))) {
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
