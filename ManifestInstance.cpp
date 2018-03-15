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

#include "ManifestInstance.h"

#include <utility>

namespace android {
namespace vintf {

ManifestInstance::ManifestInstance() = default;

ManifestInstance::ManifestInstance(const ManifestInstance&) = default;

ManifestInstance::ManifestInstance(ManifestInstance&&) = default;

ManifestInstance& ManifestInstance::operator=(const ManifestInstance&) = default;

ManifestInstance& ManifestInstance::operator=(ManifestInstance&&) = default;

ManifestInstance::ManifestInstance(FqInstance&& fqInstance, TransportArch&& ta)
    : mFqInstance(std::move(fqInstance)), mTransportArch(std::move(ta)) {}
ManifestInstance::ManifestInstance(const FqInstance& fqInstance, const TransportArch& ta)
    : mFqInstance(fqInstance), mTransportArch(ta) {}

const std::string& ManifestInstance::package() const {
    return mFqInstance.getPackage();
}

Version ManifestInstance::version() const {
    return mFqInstance.getVersion();
}

const std::string& ManifestInstance::interface() const {
    return mFqInstance.getInterface();
}

const std::string& ManifestInstance::instance() const {
    return mFqInstance.getInstance();
}

Transport ManifestInstance::transport() const {
    return mTransportArch.transport;
}

Arch ManifestInstance::arch() const {
    return mTransportArch.arch;
}

const FqInstance& ManifestInstance::getFqInstance() const {
    return mFqInstance;
}

}  // namespace vintf
}  // namespace android
