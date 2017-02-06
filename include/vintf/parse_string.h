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

#ifndef ANDROID_VINTF_PARSE_STRING_H
#define ANDROID_VINTF_PARSE_STRING_H

#include <iostream>
#include <sstream>
#include <string>

#include "CompatibilityMatrix.h"
#include "KernelInfo.h"
#include "VendorManifest.h"

namespace android {
namespace vintf {

std::ostream &operator<<(std::ostream &os, HalFormat hf);
std::ostream &operator<<(std::ostream &os, ImplLevel il);
std::ostream &operator<<(std::ostream &os, Transport tr);
std::ostream &operator<<(std::ostream &os, const Version &ver);
std::ostream &operator<<(std::ostream &os, const VersionRange &vr);
std::ostream &operator<<(std::ostream &os, const ManifestHal &hal);
std::ostream &operator<<(std::ostream &os, const MatrixHal &req);

template <typename T>
std::string to_string(const T &obj) {
    std::ostringstream oss;
    oss << obj;
    return oss.str();
}

bool parse(const std::string &s, HalFormat *hf);
bool parse(const std::string &s, ImplLevel *il);
bool parse(const std::string &s, Transport *tr);
bool parse(const std::string &s, Version *ver);
bool parse(const std::string &s, VersionRange *vr);
// if return true, hal->isValid() must be true.
bool parse(const std::string &s, ManifestHal *hal);
bool parse(const std::string &s, MatrixHal *req);
bool parse(const std::string &s, KernelConfig *kc);

// A string that describes the whole object, with versions of all
// its components. For debugging and testing purposes only. This is not
// the XML string.
std::string dump(const VendorManifest &vm);

std::string dump(const KernelInfo &ki);

} // namespace vintf
} // namespace android

#endif // ANDROID_VINTF_PARSE_STRING_H
