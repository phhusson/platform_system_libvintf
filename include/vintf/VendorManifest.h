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

#include "Version.h"
#include "ManifestHal.h"

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

    bool add(ManifestHal &&hal);
    inline void clear() { hals.clear(); }

    // Whether this manifest is a valid one. Note that an empty VendorManifest
    // (constructed via VendorManifest()) is valid.
    bool isValid() const;

    // Given a component name (e.g. "camera"), return a list of version numbers
    // that are supported by the hardware. If the entry is not found, empty list
    // is returned.
    const std::vector<Version> &getSupportedVersions(const std::string &name) const;

    // Return a list of component names that does NOT conform to
    // the given compatibility matrix.
    std::vector<std::string> checkIncompatiblity(const CompatibilityMatrix &mat) const;

    // Gather all Vendor Manifest fragments, and encapsulate in a VendorManifest.
    // If any error, nullptr is returned.
    // Note: this is not thread-safe.
    static const VendorManifest *Get();

    // sorted map from component name to the entry.
    std::map<std::string, ManifestHal> hals;

private:
    status_t fetchAllInformation();
};


} // namespace vintf
} // namespace android

#endif // ANDROID_VINTF_VENDOR_MANIFEST_H
