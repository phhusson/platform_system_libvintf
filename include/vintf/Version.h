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


#ifndef ANDROID_VINTF_VERSION_H
#define ANDROID_VINTF_VERSION_H

#include <stdint.h>
#include <string>
#include <utility>

namespace android {
namespace vintf {

struct Version {

    constexpr Version() : Version(0u, 0u) {}
    constexpr Version(size_t mj, size_t mi) : majorVer(mj), minorVer(mi) {}

    size_t majorVer;
    size_t minorVer;

    inline bool operator==(const Version &other) const {
        return majorVer == other.majorVer && minorVer == other.minorVer;
    }
    inline bool operator!=(const Version &other) const {
        return !((*this) == other);
    }
    inline bool operator<(const Version &other) const {
        if (majorVer < other.majorVer)
            return true;
        if (majorVer > other.majorVer)
            return false;
        return minorVer < other.minorVer;
    }
    inline bool operator>(const Version &other) const {
        return other < (*this);
    }
    inline bool operator<=(const Version &other) const {
        return !((*this) > other);
    }
    inline bool operator>=(const Version &other) const {
        return !((*this) < other);
    }

    static constexpr size_t INFINITY = SIZE_MAX;
};

} // namespace vintf
} // namespace android

#endif // ANDROID_VINTF_VERSION_H
