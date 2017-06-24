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

#include "utils.h"

namespace android {
namespace vintf {

constexpr Version CompatibilityMatrix::kVersion;

bool CompatibilityMatrix::add(MatrixHal &&hal) {
    return HalGroup<MatrixHal>::add(std::move(hal));
}

bool CompatibilityMatrix::add(MatrixKernel &&kernel) {
    if (mType != SchemaType::FRAMEWORK) {
        return false;
    }
    framework.mKernels.push_back(std::move(kernel));
    return true;
}

const MatrixKernel *CompatibilityMatrix::findKernel(const KernelVersion &v) const {
    if (mType != SchemaType::FRAMEWORK) {
        return nullptr;
    }
    for (const MatrixKernel &matrixKernel : framework.mKernels) {
        if (matrixKernel.minLts().version == v.version &&
            matrixKernel.minLts().majorRev == v.majorRev) {
            return matrixKernel.minLts().minorRev <= v.minorRev ? &matrixKernel : nullptr;
        }
    }
    return nullptr;
}

SchemaType CompatibilityMatrix::type() const {
    return mType;
}


status_t CompatibilityMatrix::fetchAllInformation(const std::string &path) {
    return details::fetchAllInformation(path, gCompatibilityMatrixConverter, this);
}

bool operator==(const CompatibilityMatrix &lft, const CompatibilityMatrix &rgt) {
    return lft.mType == rgt.mType && lft.mHals == rgt.mHals && lft.mXmlFiles == rgt.mXmlFiles &&
           (lft.mType != SchemaType::DEVICE || (lft.device.mVndk == rgt.device.mVndk)) &&
           (lft.mType != SchemaType::FRAMEWORK ||
            (lft.framework.mKernels == rgt.framework.mKernels &&
             lft.framework.mSepolicy == rgt.framework.mSepolicy &&
             lft.framework.mAvbMetaVersion == rgt.framework.mAvbMetaVersion));
}

} // namespace vintf
} // namespace android
