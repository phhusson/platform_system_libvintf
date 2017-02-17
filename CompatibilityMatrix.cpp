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

#include "CompatibilityMatrix.h"

namespace android {
namespace vintf {

constexpr Version CompatibilityMatrix::kVersion;

bool CompatibilityMatrix::add(MatrixHal &&hal) {
    return hals.emplace(hal.name, std::move(hal)).second;
}

bool CompatibilityMatrix::add(MatrixKernel &&kernel) {
    kernels.push_back(std::move(kernel));
    return true;
}

void CompatibilityMatrix::clear() {
    hals.clear();
    kernels.clear();
}

ConstMapValueIterable<std::string, MatrixHal> CompatibilityMatrix::getHals() const {
    return ConstMapValueIterable<std::string, MatrixHal>(hals);
}

const MatrixKernel *CompatibilityMatrix::findKernel(const KernelVersion &v) const {
    for (const MatrixKernel &matrixKernel : kernels) {
        if (matrixKernel.minLts().version == v.version &&
            matrixKernel.minLts().majorRev == v.majorRev) {
            return matrixKernel.minLts().minorRev <= v.minorRev ? &matrixKernel : nullptr;
        }
    }
    return nullptr;
}

const Sepolicy &CompatibilityMatrix::getSepolicy() const {
    return sepolicy;
}

} // namespace vintf
} // namespace android
