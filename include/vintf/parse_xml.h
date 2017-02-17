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

#ifndef ANDROID_VINTF_PARSE_XML_H
#define ANDROID_VINTF_PARSE_XML_H

#include "CompatibilityMatrix.h"
#include "VendorManifest.h"

namespace android {
namespace vintf {

template<typename Object>
struct XmlConverter {
    XmlConverter() {}
    virtual ~XmlConverter() {}
    virtual const std::string &lastError() const = 0;
    virtual std::string serialize(const Object &o) const = 0;
    virtual bool deserialize(Object *o, const std::string &xml) const = 0;
    virtual std::string operator()(const Object &o) const = 0;
    virtual bool operator()(Object *o, const std::string &xml) const = 0;
};

extern const XmlConverter<Version> &gVersionConverter;

extern const XmlConverter<VersionRange> &gVersionRangeConverter;

extern const XmlConverter<MatrixHal> &gMatrixHalConverter;

extern const XmlConverter<KernelConfigTypedValue> &gKernelConfigTypedValueConverter;

extern const XmlConverter<KernelConfig> &gKernelConfigConverter;

extern const XmlConverter<MatrixKernel> &gMatrixKernelConverter;

extern const XmlConverter<HalImplementation> &gHalImplementationConverter;

// Convert ManifestHal from and to XML. Returned object is guaranteed to have
// .isValid() == true.
extern const XmlConverter<ManifestHal> &gManifestHalConverter;

extern const XmlConverter<Sepolicy> &gSepolicyConverter;

extern const XmlConverter<VendorManifest> &gVendorManifestConverter;

extern const XmlConverter<CompatibilityMatrix> &gCompatibilityMatrixConverter;

} // namespace vintf
} // namespace android

#endif // ANDROID_VINTF_PARSE_XML_H
