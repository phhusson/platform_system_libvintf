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


#ifndef ANDROID_VINTF_VNDK_H
#define ANDROID_VINTF_VNDK_H

#include <set>
#include <string>

namespace android {
namespace vintf {

struct VndkVersionRange {

    VndkVersionRange() : VndkVersionRange(0u, 0u, 0u) {}
    VndkVersionRange(size_t s, size_t v, size_t p)
        : VndkVersionRange(s, v, p, p) {}
    VndkVersionRange(size_t s, size_t v, size_t pi, size_t pa)
        : sdk(s), vndk(v), patchMin(pi), patchMax(pa) {}

    inline bool isSingleVersion() const { return patchMin == patchMax; };

    size_t sdk;
    size_t vndk;
    size_t patchMin;
    size_t patchMax;
};

struct Vndk {

private:
    friend struct VndkConverter;
    friend struct HalManifestConverter;
    friend struct LibVintfTest;
    VndkVersionRange mVersionRange;
    std::set<std::string> mLibraries;
};

} // namespace vintf
} // namespace android

#endif // ANDROID_VINTF_VNDK_H
