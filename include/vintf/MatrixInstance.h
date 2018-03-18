/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef ANDROID_VINTF_MATRIX_INSTANCE_H
#define ANDROID_VINTF_MATRIX_INSTANCE_H

#include <string>

#include <hidl-util/FqInstance.h>

#include "VersionRange.h"

namespace android {
namespace vintf {

class MatrixInstance {
   public:
    MatrixInstance();
    MatrixInstance(const MatrixInstance&);
    MatrixInstance(MatrixInstance&&);
    MatrixInstance& operator=(const MatrixInstance&);
    MatrixInstance& operator=(MatrixInstance&&);

    using VersionType = VersionRange;
    // fqInstance.version is ignored. Version range is provided separately.
    MatrixInstance(FqInstance&& fqInstance, VersionRange&& range, bool optional);
    MatrixInstance(const FqInstance fqInstance, const VersionRange& range, bool optional);
    const std::string& package() const;
    const VersionRange& versionRange() const;
    const std::string& interface() const;
    const std::string& instance() const;
    bool optional() const;

    bool isSatisfiedBy(const FqInstance& provided) const;

   private:
    FqInstance mFqInstance;
    VersionRange mRange;
    bool mOptional;
};

}  // namespace vintf
}  // namespace android

#endif  // ANDROID_VINTF_MATRIX_INSTANCE_H
