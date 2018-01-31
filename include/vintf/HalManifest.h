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


#ifndef ANDROID_VINTF_HAL_MANIFEST_H
#define ANDROID_VINTF_HAL_MANIFEST_H

#include <map>
#include <string>
#include <utils/Errors.h>
#include <vector>

#include "HalGroup.h"
#include "Level.h"
#include "ManifestHal.h"
#include "MapValueIterator.h"
#include "SchemaType.h"
#include "SystemSdk.h"
#include "VendorNdk.h"
#include "Version.h"
#include "Vndk.h"
#include "XmlFileGroup.h"

namespace android {
namespace vintf {

struct MatrixHal;
struct CompatibilityMatrix;

// A HalManifest is reported by the hardware and query-able from
// framework code. This is the API for the framework.
struct HalManifest : public HalGroup<ManifestHal>, public XmlFileGroup<ManifestXmlFile> {
   public:

    // Construct a device HAL manifest.
    HalManifest() : mType(SchemaType::DEVICE) {}

    bool add(ManifestHal&& hal) override;

    // Given a component name (e.g. "android.hardware.camera"),
    // return getHal(name)->transport if the component exist and v exactly matches
    // one of the versions in that component, else EMPTY
    Transport getTransport(const std::string &name, const Version &v,
            const std::string &interfaceName, const std::string &instanceName) const;

    // Given a component name (e.g. "android.hardware.camera"),
    // return a list of version numbers that are supported by the hardware.
    // If the component is not found, empty list is returned.
    // If multiple matches, return a concatenation of version entries
    // (dupes removed)
    std::set<Version> getSupportedVersions(const std::string &name) const;

    // Convenience method for checking if instanceName is in getInstances(halName, interfaceName)
    bool hasInstance(const std::string& halName, const Version& version,
                     const std::string& interfaceName, const std::string& instanceName) const;

    // Return a list of component names that does NOT conform to
    // the given compatibility matrix. It contains components that are optional
    // for the framework if includeOptional = true.
    // Note: only HAL entries are checked. To check other entries as well, use
    // checkCompatibility.
    std::vector<std::string> checkIncompatibility(const CompatibilityMatrix &mat,
            bool includeOptional = true) const;

    // Check compatibility against a compatibility matrix. Considered compatible if
    // - framework manifest vs. device compat-mat
    //     - checkIncompatibility for HALs returns only optional HALs
    //     - one of manifest.vndk match compat-mat.vndk
    // - device manifest vs. framework compat-mat
    //     - checkIncompatibility for HALs returns only optional HALs
    //     - manifest.sepolicy.version match one of compat-mat.sepolicy.sepolicy-version
    bool checkCompatibility(const CompatibilityMatrix &mat, std::string *error = nullptr) const;

    // Generate a compatibility matrix such that checkCompatibility will return true.
    CompatibilityMatrix generateCompatibleMatrix() const;

    // Returns all component names.
    std::set<std::string> getHalNames() const;

    // Returns all component names and versions, e.g.
    // "android.hardware.camera.device@1.0", "android.hardware.camera.device@3.2",
    // "android.hardware.nfc@1.0"]
    std::set<std::string> getHalNamesAndVersions() const;

    // Type of the manifest. FRAMEWORK or DEVICE.
    SchemaType type() const;

    // FCM version that it implements.
    Level level() const;

    // device.mSepolicyVersion. Assume type == device.
    // Abort if type != device.
    const Version &sepolicyVersion() const;

    // framework.mVendorNdks. Assume type == framework.
    // Abort if type != framework.
    const std::vector<VendorNdk>& vendorNdks() const;

    // If the corresponding <xmlfile> with the given version exists,
    // - Return the overridden <path> if it is present,
    // - otherwise the default value: /{system,vendor}/etc/<name>_V<major>_<minor>.xml
    // Otherwise if the <xmlfile> entry does not exist, "" is returned.
    std::string getXmlFilePath(const std::string& xmlFileName, const Version& version) const;

    // Get metaversion of this manifest.
    Version getMetaVersion() const;

    void forEachInstance(
        const std::function<void(const std::string&, const Version&, const std::string&,
                                 const std::string&, bool*)>& f) const;

   protected:
    // Check before add()
    bool shouldAdd(const ManifestHal& toAdd) const override;
    bool shouldAddXmlFile(const ManifestXmlFile& toAdd) const override;

   private:
    friend struct HalManifestConverter;
    friend class VintfObject;
    friend class AssembleVintfImpl;
    friend struct LibVintfTest;
    friend std::string dump(const HalManifest &vm);
    friend bool operator==(const HalManifest &lft, const HalManifest &rgt);

    status_t fetchAllInformation(const std::string& path, std::string* error = nullptr);

    // Check if all instances in matrixHal is supported in this manifest.
    bool isCompatible(const MatrixHal& matrixHal) const;

    std::vector<std::string> checkIncompatibleXmlFiles(const CompatibilityMatrix& mat,
                                                       bool includeOptional = true) const;

    void removeHals(const std::string& name, size_t majorVer);

    SchemaType mType;
    Level mLevel = Level::UNSPECIFIED;
    // version attribute. Default is 1.0 for manifests created programatically.
    Version mMetaVersion{1, 0};

    // entries for device hal manifest only
    struct {
        Version mSepolicyVersion;
    } device;

    // entries for framework hal manifest only
    struct {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        std::vector<Vndk> mVndks;
#pragma clang diagnostic pop

        std::vector<VendorNdk> mVendorNdks;
        SystemSdk mSystemSdk;
    } framework;
};


} // namespace vintf
} // namespace android

#endif // ANDROID_VINTF_HAL_MANIFEST_H
