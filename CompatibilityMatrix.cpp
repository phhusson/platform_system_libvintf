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

#include <utility>

#include "parse_string.h"
#include "utils.h"

namespace android {
namespace vintf {

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

SchemaType CompatibilityMatrix::type() const {
    return mType;
}

Level CompatibilityMatrix::level() const {
    return mLevel;
}

Version CompatibilityMatrix::getMinimumMetaVersion() const {
    // TODO(b/62801658): this needs to depend on whether there are 1.1 requirements
    // (e.g. required <xmlfile> entry)
    return {1, 0};
}

status_t CompatibilityMatrix::fetchAllInformation(const std::string &path) {
    return details::fetchAllInformation(path, gCompatibilityMatrixConverter, this);
}

std::string CompatibilityMatrix::getXmlSchemaPath(const std::string& xmlFileName,
                                                  const Version& version) const {
    using std::literals::string_literals::operator""s;
    auto range = getXmlFiles(xmlFileName);
    for (auto it = range.first; it != range.second; ++it) {
        const MatrixXmlFile& matrixXmlFile = it->second;
        if (matrixXmlFile.versionRange().contains(version)) {
            if (!matrixXmlFile.overriddenPath().empty()) {
                return matrixXmlFile.overriddenPath();
            }
            return "/"s + (type() == SchemaType::DEVICE ? "vendor" : "system") + "/etc/" +
                   xmlFileName + "_V" + std::to_string(matrixXmlFile.versionRange().majorVer) +
                   "_" + std::to_string(matrixXmlFile.versionRange().maxMinor) + "." +
                   to_string(matrixXmlFile.format());
        }
    }
    return "";
}

static VersionRange* findRangeWithMajorVersion(std::vector<VersionRange>& versionRanges,
                                               size_t majorVer) {
    for (VersionRange& vr : versionRanges) {
        if (vr.majorVer == majorVer) {
            return &vr;
        }
    }
    return nullptr;
}

std::pair<MatrixHal*, VersionRange*> CompatibilityMatrix::getHalWithMajorVersion(
    const std::string& name, size_t majorVer) {
    for (MatrixHal* hal : getHals(name)) {
        VersionRange* vr = findRangeWithMajorVersion(hal->versionRanges, majorVer);
        if (vr != nullptr) {
            return {hal, vr};
        }
    }
    return {nullptr, nullptr};
}

bool CompatibilityMatrix::addAllHalsAsOptional(CompatibilityMatrix* other, std::string* error) {
    if (other == nullptr || other->level() <= level()) {
        return true;
    }

    for (auto& pair : other->mHals) {
        const std::string& name = pair.first;
        MatrixHal& halToAdd = pair.second;
        for (const VersionRange& vr : halToAdd.versionRanges) {
            MatrixHal* existingHal;
            VersionRange* existingVr;
            std::tie(existingHal, existingVr) = getHalWithMajorVersion(name, vr.majorVer);

            if (existingHal == nullptr) {
                halToAdd.optional = true;
                add(std::move(halToAdd));
                continue;
            }

            if (!existingHal->optional && !existingHal->containsInstances(halToAdd)) {
                if (error != nullptr) {
                    *error = "HAL " + name + "@" + to_string(vr.minVer()) + " is a required " +
                             "HAL, but fully qualified instance names don't match (at FCM "
                             "Version " +
                             std::to_string(level()) + " and " + std::to_string(other->level()) +
                             ")";
                }
                return false;
            }

            existingVr->maxMinor = std::max(existingVr->maxMinor, vr.maxMinor);
        }
    }
    return true;
}

bool operator==(const CompatibilityMatrix &lft, const CompatibilityMatrix &rgt) {
    return lft.mType == rgt.mType && lft.mLevel == rgt.mLevel && lft.mHals == rgt.mHals &&
           lft.mXmlFiles == rgt.mXmlFiles &&
           (lft.mType != SchemaType::DEVICE || (lft.device.mVndk == rgt.device.mVndk)) &&
           (lft.mType != SchemaType::FRAMEWORK ||
            (lft.framework.mKernels == rgt.framework.mKernels &&
             lft.framework.mSepolicy == rgt.framework.mSepolicy &&
             lft.framework.mAvbMetaVersion == rgt.framework.mAvbMetaVersion));
}

} // namespace vintf
} // namespace android
