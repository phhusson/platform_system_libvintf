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

#ifndef ANDROID_VINTF_MATRIX_KERNEL_H
#define ANDROID_VINTF_MATRIX_KERNEL_H

#include <vector>

#include "KernelConfig.h"
#include "Version.h"

namespace android {
namespace vintf {

// A kernel entry to a compatibility matrix
struct MatrixKernel {

    bool operator==(const MatrixKernel &other) const;

    Version version;
    std::vector<KernelConfig> configs;
};

} // namespace vintf
} // namespace android

#endif // ANDROID_VINTF_MATRIX_KERNEL_H
