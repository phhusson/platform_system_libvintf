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

#include "MatrixInstance.h"

#include <utility>

namespace android {
namespace vintf {

MatrixInstance::MatrixInstance(FqInstance&& fqInstance, VersionRange&& range, bool optional)
    : mFqInstance(std::move(fqInstance)), mRange(std::move(range)), mOptional(optional) {}

MatrixInstance::MatrixInstance(const FqInstance fqInstance, const VersionRange& range,
                               bool optional)
    : mFqInstance(fqInstance), mRange(range), mOptional(optional) {}

const std::string& MatrixInstance::package() const {
    return mFqInstance.getPackage();
}

const VersionRange& MatrixInstance::versionRange() const {
    return mRange;
}

const std::string& MatrixInstance::interface() const {
    return mFqInstance.getInterface();
}

const std::string& MatrixInstance::instance() const {
    return mFqInstance.getInstance();
}

bool MatrixInstance::optional() const {
    return mOptional;
}

}  // namespace vintf
}  // namespace android
