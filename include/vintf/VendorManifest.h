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


#ifndef ANDROID_VINTF_VENDOR_MANIFEST_H
#define ANDROID_VINTF_VENDOR_MANIFEST_H

#include <map>
#include <string>
#include <utils/Errors.h>
#include <vector>

#include "ManifestHal.h"
#include "MapValueIterator.h"
#include "Version.h"

namespace android {
namespace vintf {

struct CompatibilityMatrix;

// A Vendor Interface Object is reported by the hardware and query-able from
// framework code. This is the API for the framework.
struct VendorManifest {
public:

    // manifest.version
    constexpr static Version kVersion{1, 0};

    VendorManifest() {}

    // Get an HAL entry based on the component name. Return nullptr
    // if the entry does not exist. The component name looks like:
    // android.hardware.foo
    const ManifestHal *getHal(const std::string &name) const;

    // return getHal(name)->transport if the entry exist and v exactly matches
    // one of the versions in that entry, else EMPTY
    Transport getTransport(const std::string &name, const Version &v) const;

    // Return an iterable to all ManifestHal objects. Call it as follows:
    // for (const ManifestHal &e : vm.getHals()) { }
    ConstMapValueIterable<std::string, ManifestHal> getHals() const;

    // Given a component name (e.g. "camera"), return a list of version numbers
    // that are supported by the hardware. If the entry is not found, empty list
    // is returned.
    const std::vector<Version> &getSupportedVersions(const std::string &name) const;

    // Return a list of component names that does NOT conform to
    // the given compatibility matrix. It may contain components that are optional
    // for the framework.
    std::vector<std::string> checkIncompatiblity(const CompatibilityMatrix &mat) const;

    // Gather all Vendor Manifest fragments, and encapsulate in a VendorManifest.
    // If no error, it return the same singleton object in the future, and the HAL manifest
    // file won't be touched again.
    // If any error, nullptr is returned, and Get will try to parse the HAL manifest
    // again when it is called again.
    // This operation is thread-safe.
    static const VendorManifest *Get();

private:
    friend struct VendorManifestConverter;
    friend struct LibVintfTest;

    // Add an hal to this manifest.
    bool add(ManifestHal &&hal);

    // clear this manifest.
    inline void clear() { mHals.clear(); }

    status_t fetchAllInformation();

    // sorted map from component name to the entry.
    // The component name looks like: android.hardware.foo
    std::map<std::string, ManifestHal> mHals;
};


} // namespace vintf
} // namespace android

#endif // ANDROID_VINTF_VENDOR_MANIFEST_H
