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

#ifndef ANDROID_VINTF_VINTF_OBJECT_H_
#define ANDROID_VINTF_VINTF_OBJECT_H_

#include "HalManifest.h"
#include "RuntimeInfo.h"

namespace android {
namespace vintf {
/*
 * The top level class for libvintf.
 * An overall diagram of the public API:
 * VintfObject
 *   + GetDeviceHalManfiest
 *   |   + getTransport
 *   |   + getSupportedVersions
 *   |   + checkIncompatibility
 *   + GetFrameworkHalManifest
 *   |   + getTransport
 *   |   + getSupportedVersions
 *   |   + checkIncompatibility
 *   + GetRuntimeInfo
 *       + checkCompatibility
 *
 * Each of the function gathers all information and encapsulate it into the object.
 * If no error, it return the same singleton object in the future, and the HAL manifest
 * file won't be touched again.
 * If any error, nullptr is returned, and Get will try to parse the HAL manifest
 * again when it is called again.
 * This operation is thread-safe.
 */
class VintfObject {
public:
    /*
     * Return the API that access the device-side HAL manifest stored
     * in /vendor/manifest.xml.
     */
    static const HalManifest *GetDeviceHalManifest();

    /*
     * Return the API that access the framework-side HAL manifest stored
     * in /system/manfiest.xml.
     */
    static const HalManifest *GetFrameworkHalManifest();

    /*
     * Return the API that access device runtime info.
     */
    static const RuntimeInfo *GetRuntimeInfo();

    /**
     * Check compatibility, given a set of manifests / matrices in packageInfo.
     * They will be checked against the manifests / matrices on the device.
     * Partitions (/system, /vendor) are mounted if necessary.
     *
     * @param packageInfo a list of XMLs of HalManifest /
     * CompatibilityMatrix objects.
     * @param mount whether partitions should be mounted to do the check.
     *
     * @return = 0 if success (compatible)
     *         > 0 if incompatible
     *         < 0 if any error (mount partition fails, illformed XML, etc.)
     */
    static int32_t CheckCompatibility(
            const std::vector<std::string> &packageInfo,
            bool mount = false);
};


} // namespace vintf
} // namespace android

#endif // ANDROID_VINTF_VINTF_OBJECT_H_
