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

#ifndef ANDROID_VINTF_COMPATIBILITY_MATRIX_H
#define ANDROID_VINTF_COMPATIBILITY_MATRIX_H

#include <map>
#include <string>

#include "MatrixHal.h"
#include "MatrixKernel.h"
#include "MapValueIterator.h"
#include "Sepolicy.h"

namespace android {
namespace vintf {

// Compatibility matrix defines what hardware does the framework requires.
struct CompatibilityMatrix {
    constexpr static Version kVersion{1, 0};

    // Return an iterable to all MatrixHal objects. Call it as follows:
    // for (const MatrixHal &e : cm.getHals()) { }
    ConstMapValueIterable<std::string, MatrixHal> getHals() const;

    // Find a MatrixKernel entry that has version v. nullptr if not found.
    const MatrixKernel *findKernel(const KernelVersion &v) const;

    const Sepolicy &getSepolicy() const;

private:
    bool add(MatrixHal &&hal);
    bool add(MatrixKernel &&kernel);
    void clear();

    friend struct CompatibilityMatrixConverter;
    friend struct LibVintfTest;

    // sorted map from component name to the entry.
    std::map<std::string, MatrixHal> mHals;

    std::vector<MatrixKernel> mKernels;

    Sepolicy mSepolicy;
};

} // namespace vintf
} // namespace android

#endif // ANDROID_VINTF_COMPATIBILITY_MATRIX_H
