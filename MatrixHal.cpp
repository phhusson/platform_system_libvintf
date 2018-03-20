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

#include <algorithm>

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

bool MatrixHal::forEachInstance(const std::function<bool(const MatrixInstance&)>& func) const {
    for (const auto& vr : versionRanges) {
        if (!forEachInstance(vr, func)) {
            return false;
        }
    }
    return true;
}

bool MatrixHal::forEachInstance(const VersionRange& vr,
                                const std::function<bool(const MatrixInstance&)>& func) const {
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
    return true;
}

bool MatrixHal::forEachInstance(
    const std::function<bool(const std::vector<VersionRange>&, const std::string&,
                             const std::string&)>& func) const {
    for (const auto& intf : iterateValues(interfaces)) {
        for (const auto& instance : intf.instances) {
            if (!func(versionRanges, intf.name, instance)) {
                return false;
            }
        }
    }
    return true;
}

bool MatrixHal::isCompatible(const std::set<FqInstance>& providedInstances,
                             const std::set<Version>& providedVersions) const {
    // <version>'s are related by OR.
    return std::any_of(versionRanges.begin(), versionRanges.end(), [&](const VersionRange& vr) {
        return isCompatible(vr, providedInstances, providedVersions);
    });
}

bool MatrixHal::isCompatible(const VersionRange& vr, const std::set<FqInstance>& providedInstances,
                             const std::set<Version>& providedVersions) const {
    bool hasAnyInstance = false;
    bool versionUnsatisfied = false;

    // Look at each interface/instance, and ensure that they are in providedInstances.
    forEachInstance(vr, [&](const MatrixInstance& matrixInstance) {
        hasAnyInstance = true;

        versionUnsatisfied |=
            !std::any_of(providedInstances.begin(), providedInstances.end(),
                         [&](const FqInstance& providedInstance) {
                             return matrixInstance.isSatisfiedBy(providedInstance);
                         });

        return !versionUnsatisfied;  // if any interface/instance is unsatisfied, break
    });

    if (hasAnyInstance) {
        return !versionUnsatisfied;
    }

    // In some cases (e.g. tests and native HALs), compatibility matrix doesn't specify
    // any instances. Check versions only.
    return std::any_of(
        providedVersions.begin(), providedVersions.end(),
        [&](const auto& providedVersion) { return vr.supportedBy(providedVersion); });
}

void MatrixHal::setOptional(bool o) {
    this->optional = o;
}

void MatrixHal::insertVersionRanges(const std::vector<VersionRange>& other) {
    for (const VersionRange& otherVr : other) {
        auto existingVr = std::find_if(this->versionRanges.begin(), this->versionRanges.end(),
                                       [&](const auto& e) { return e.overlaps(otherVr); });

        if (existingVr == this->versionRanges.end()) {
            this->versionRanges.push_back(otherVr);
        } else {
            existingVr->minMinor = std::min(existingVr->minMinor, otherVr.minMinor);
            existingVr->maxMinor = std::max(existingVr->maxMinor, otherVr.maxMinor);
        }
    }
}

void MatrixHal::insertInstance(const std::string& interface, const std::string& instance) {
    auto it = interfaces.find(interface);
    if (it == interfaces.end())
        it = interfaces.emplace(interface, HalInterface{interface, {}}).first;
    it->second.instances.insert(instance);
}

bool MatrixHal::hasAnyInstance() const {
    bool found = false;
    forEachInstance([&](const auto&) {
        found = true;
        return false;  // break if any instance
    });
    return found;
}

bool MatrixHal::hasInstance(const std::string& interface, const std::string& instance) const {
    bool found = false;
    forEachInstance([&](const auto& matrixInstance) {
        found |= matrixInstance.interface() == interface && matrixInstance.instance() == instance;
        return !found;  // continue if not match
    });
    return found;
}

bool MatrixHal::hasOnlyInstance(const std::string& interface, const std::string& instance) const {
    bool found = false;
    bool foundOthers = false;

    forEachInstance([&](const auto& matrixInstance) {
        bool match =
            matrixInstance.interface() == interface && matrixInstance.instance() == instance;

        found |= match;
        foundOthers |= (!match);

        return !foundOthers;
    });

    return found && !foundOthers;
}

bool MatrixHal::removeInstance(const std::string& interface, const std::string& instance) {
    auto it = interfaces.find(interface);
    if (it == interfaces.end()) return false;
    it->second.instances.erase(instance);
    if (it->second.instances.empty()) interfaces.erase(it);
    return true;
}

void MatrixHal::clearInstances() {
    this->interfaces.clear();
}

} // namespace vintf
} // namespace android
