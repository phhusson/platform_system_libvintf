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

#ifndef ANDROID_VINTF_SEPOLICY_H
#define ANDROID_VINTF_SEPOLICY_H


namespace android {
namespace vintf {

struct KernelSepolicyVersion {
    size_t value;
    inline KernelSepolicyVersion() : KernelSepolicyVersion(0u) {}
    inline KernelSepolicyVersion(size_t v) : value(v) {}
    inline operator size_t() const {
        return value;
    }
};

struct SepolicyVersion {
    size_t minVer;
    size_t maxVer;

    SepolicyVersion() : SepolicyVersion(0u, 0u) {}
    SepolicyVersion(size_t minV, size_t maxV)
            : minVer(minV), maxVer(maxV) {}
};

struct Sepolicy {

    Sepolicy() : Sepolicy(0u, {0u, 0u}) {}
    Sepolicy(KernelSepolicyVersion kernelSepolicyVersion,
            SepolicyVersion &&sepolicyVersion) :
            mKernelSepolicyVersion(kernelSepolicyVersion),
            mSepolicyVersion(std::move(sepolicyVersion)) {}

    inline size_t kernelSepolicyVersion() const { return mKernelSepolicyVersion.value; }
    inline const SepolicyVersion &sepolicyVersion() const {
        return mSepolicyVersion;
    }
private:
    friend struct SepolicyConverter;
    KernelSepolicyVersion mKernelSepolicyVersion;
    SepolicyVersion mSepolicyVersion;
};

} // namespace vintf
} // namespace android

#endif // ANDROID_VINTF_SEPOLICY_H
