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

#include "VintfObject.h"

#include <functional>
#include <memory>
#include <mutex>

namespace android {
namespace vintf {

template <typename T>
struct LockedUniquePtr {
    std::unique_ptr<T> object;
    std::mutex mutex;
};

static LockedUniquePtr<HalManifest> gDeviceManifest;
static LockedUniquePtr<HalManifest> gFrameworkManifest;
static LockedUniquePtr<RuntimeInfo> gDeviceRuntimeInfo;

template <typename T, typename F>
static const T *Get(
        LockedUniquePtr<T> *ptr,
        const F &fetchAllInformation) {
    std::unique_lock<std::mutex> _lock(ptr->mutex);
    if (ptr->object == nullptr) {
        ptr->object = std::make_unique<T>();
        if (fetchAllInformation(ptr->object.get()) != OK) {
            ptr->object = nullptr; // frees the old object
        }
    }
    return ptr->object.get();
}

// static
const HalManifest *VintfObject::GetDeviceHalManifest() {
    return Get(&gDeviceManifest,
            std::bind(&HalManifest::fetchAllInformation, std::placeholders::_1,
                "/vendor/manifest.xml"));
}

// static
const HalManifest *VintfObject::GetFrameworkHalManifest() {
    return Get(&gFrameworkManifest,
            std::bind(&HalManifest::fetchAllInformation, std::placeholders::_1,
                "/system/manifest.xml"));
}

// static
const RuntimeInfo *VintfObject::GetRuntimeInfo() {
    return Get(&gDeviceRuntimeInfo,
            std::bind(&RuntimeInfo::fetchAllInformation, std::placeholders::_1));
}

// static
int32_t VintfObject::CheckCompatibility(
        const std::vector<std::string> &,
        bool) {
    // TODO(b/36814503): actually do the verification.
    return -1;
}


} // namespace vintf
} // namespace android
