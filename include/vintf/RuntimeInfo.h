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

#ifndef ANDROID_VINTF_RUNTIME_INFO_H
#define ANDROID_VINTF_RUNTIME_INFO_H

#include "Version.h"

#include <map>
#include <string>

#include <utils/Errors.h>

namespace android {
namespace vintf {

struct CompatibilityMatrix;

// Runtime Info sent to OTA server
struct RuntimeInfo {

    RuntimeInfo() {}

    // /proc/version
    // utsname.sysname
    const std::string &osName() const;
    // utsname.nodename
    const std::string &nodeName() const;
    // utsname.release
    const std::string &osRelease() const;
    // utsname.version
    const std::string &osVersion() const;
    // utsname.machine
    const std::string &hardwareId() const;

    const std::vector<std::string> &sepolicyFilePaths() const;

    // /sys/fs/selinux/policyvers
    size_t kernelSepolicyVersion() const;

    // Return whether this kernel works with the given compatibility matrix.
    bool checkCompatibility(const CompatibilityMatrix &mat,
            std::string *error = nullptr) const;

private:

    friend struct RuntimeInfoFetcher;
    friend class VintfObject;
    friend struct LibVintfTest;
    friend std::string dump(const RuntimeInfo &ki);

    status_t fetchAllInformation();

    // /proc/config.gz
    // Key: CONFIG_xxx; Value: the value after = sign.
    std::map<std::string, std::string> mKernelConfigs;
    std::string mOsName;
    std::string mNodeName;
    std::string mOsRelease;
    std::string mOsVersion;
    std::string mHardwareId;
    KernelVersion mKernelVersion;

    std::vector<std::string> mSepolicyFilePaths;

    size_t mKernelSepolicyVersion;

};

} // namespace vintf
} // namespace android

#endif // ANDROID_VINTF_RUNTIME_INFO_H
