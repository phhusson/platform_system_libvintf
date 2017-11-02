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

#ifndef ANDROID_VINTF_HAL_GROUP_H
#define ANDROID_VINTF_HAL_GROUP_H

#include <map>
#include <set>

#include "MapValueIterator.h"
#include "Version.h"

namespace android {
namespace vintf {

// A HalGroup is a wrapped multimap from name to Hal.
// Hal.getName() must return a string indicating the name.
template <typename Hal>
struct HalGroup {
   public:
    virtual ~HalGroup() {}
    // Move all hals from another HalGroup to this.
    bool addAll(HalGroup&& other, std::string* error = nullptr) {
        for (auto& pair : other.mHals) {
            if (!add(std::move(pair.second))) {
                if (error) {
                    *error = pair.first;
                }
                return false;
            }
        }
        return true;
    }

    // Add an hal to this HalGroup so that it can be constructed programatically.
    bool add(Hal&& hal) {
        if (!shouldAdd(hal)) {
            return false;
        }
        std::string name = hal.getName();
        mHals.emplace(std::move(name), std::move(hal));  // always succeed
        return true;
    }

    // Get all hals with the given name (e.g "android.hardware.camera").
    // There could be multiple hals that matches the same given name.
    std::vector<const Hal*> getHals(const std::string& name) const {
        std::vector<const Hal*> ret;
        auto range = mHals.equal_range(name);
        for (auto it = range.first; it != range.second; ++it) {
            ret.push_back(&it->second);
        }
        return ret;
    }

    // Get all hals with the given name (e.g "android.hardware.camera").
    // There could be multiple hals that matches the same given name.
    // Non-const version of the above getHals() method.
    std::vector<Hal*> getHals(const std::string& name) {
        std::vector<Hal*> ret;
        auto range = mHals.equal_range(name);
        for (auto it = range.first; it != range.second; ++it) {
            ret.push_back(&it->second);
        }
        return ret;
    }

    // Get the hal that matches the given name and version (e.g.
    // "android.hardware.camera@2.4")
    // There should be a single hal that matches the given name and version.
    const Hal* getHal(const std::string& name, const Version& version) const {
        for (const Hal* hal : getHals(name)) {
            if (hal->containsVersion(version)) return hal;
        }
        return nullptr;
    }

    // Get all instance names for hal that matches the given component name, version
    // and interface name (e.g. "android.hardware.camera@2.4::ICameraProvider").
    // * If the component ("android.hardware.camera@2.4") does not exist, return empty set.
    // * If the component ("android.hardware.camera@2.4") does exist,
    //    * If the interface (ICameraProvider) does not exist, return empty set.
    //    * Else return the list hal.interface.instance.
    std::set<std::string> getInstances(const std::string& halName, const Version& version,
                                       const std::string& interfaceName) const {
        const Hal* hal = getHal(halName, version);
        return hal->getInstances(interfaceName);
    }

   protected:
    // sorted map from component name to the component.
    // The component name looks like: android.hardware.foo
    std::multimap<std::string, Hal> mHals;

    // override this to filter for add.
    virtual bool shouldAdd(const Hal&) const { return true; }

    // Return an iterable to all ManifestHal objects. Call it as follows:
    // for (const auto& e : vm.getHals()) { }
    ConstMultiMapValueIterable<std::string, Hal> getHals() const {
        return ConstMultiMapValueIterable<std::string, Hal>(mHals);
    }

    // Get any HAL component based on the component name. Return any one
    // if multiple. Return nullptr if the component does not exist. This is only
    // for creating objects programatically.
    // The component name looks like:
    // android.hardware.foo
    Hal* getAnyHal(const std::string& name) {
        auto it = mHals.find(name);
        if (it == mHals.end()) {
            return nullptr;
        }
        return &(it->second);
    }
};

}  // namespace vintf
}  // namespace android

#endif  // ANDROID_VINTF_HAL_GROUP_H
