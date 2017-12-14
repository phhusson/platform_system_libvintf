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

#include <utils/Errors.h>

#include "HalGroup.h"
#include "Level.h"
#include "MapValueIterator.h"
#include "MatrixHal.h"
#include "MatrixKernel.h"
#include "SchemaType.h"
#include "Sepolicy.h"
#include "Vndk.h"
#include "XmlFileGroup.h"

namespace android {
namespace vintf {

// Compatibility matrix defines what hardware does the framework requires.
struct CompatibilityMatrix : public HalGroup<MatrixHal>, public XmlFileGroup<MatrixXmlFile> {
    // Create a framework compatibility matrix.
    CompatibilityMatrix() : mType(SchemaType::FRAMEWORK) {};

    SchemaType type() const;
    Level level() const;
    Version getMinimumMetaVersion() const;

    // If the corresponding <xmlfile> with the given version exists, for the first match,
    // - Return the overridden <path> if it is present,
    // - otherwise the default value: /{system,vendor}/etc/<name>_V<major>_<minor-max>.<format>
    // Otherwise if the <xmlfile> entry does not exist, "" is returned.
    // For example, if the matrix says ["audio@1.0-5" -> "foo.xml", "audio@1.3-7" -> bar.xml]
    // getXmlSchemaPath("audio", 1.0) -> foo.xml
    // getXmlSchemaPath("audio", 1.5) -> foo.xml
    // getXmlSchemaPath("audio", 1.7) -> bar.xml
    // (Normally, version ranges do not overlap, and the only match is returned.)
    std::string getXmlSchemaPath(const std::string& xmlFileName, const Version& version) const;

   private:
    bool add(MatrixHal &&hal);
    bool add(MatrixKernel &&kernel);

    // Add all HALs as optional HALs from "other". This function moves MatrixHal objects
    // from "other".
    // Require other->level() > this->level(), otherwise do nothing.
    bool addAllHalsAsOptional(CompatibilityMatrix* other, std::string* error);
    // Return the MatrixHal object with the given name and major version. Since all major
    // version are guaranteed distinct when add()-ed, there should be at most 1 match.
    // Return nullptr if none is found.
    std::pair<MatrixHal*, VersionRange*> getHalWithMajorVersion(const std::string& name,
                                                                size_t majorVer);

    status_t fetchAllInformation(const std::string &path);

    friend struct HalManifest;
    friend struct RuntimeInfo;
    friend struct CompatibilityMatrixConverter;
    friend struct LibVintfTest;
    friend class VintfObject;
    friend class AssembleVintf;
    friend bool operator==(const CompatibilityMatrix &, const CompatibilityMatrix &);

    SchemaType mType;
    Level mLevel = Level::UNSPECIFIED;

    // entries only for framework compatibility matrix.
    struct {
        std::vector<MatrixKernel> mKernels;
        Sepolicy mSepolicy;
        Version mAvbMetaVersion;
    } framework;

    // entries only for device compatibility matrix.
    struct {
        Vndk mVndk;
    } device;
};

} // namespace vintf
} // namespace android

#endif // ANDROID_VINTF_COMPATIBILITY_MATRIX_H
