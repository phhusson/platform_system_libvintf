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

#include "VintfObject.h"

#include "CompatibilityMatrix.h"
#include "parse_xml.h"
#include "utils.h"

#include <dirent.h>

#include <functional>
#include <memory>
#include <mutex>

#ifdef LIBVINTF_TARGET
#include <android-base/properties.h>
#endif

#include <android-base/logging.h>

using std::placeholders::_1;
using std::placeholders::_2;

namespace android {
namespace vintf {

using namespace details;

template <typename T>
struct LockedSharedPtr {
    std::shared_ptr<T> object;
    std::mutex mutex;
    bool fetchedOnce = false;
};

struct LockedRuntimeInfoCache {
    std::shared_ptr<RuntimeInfo> object;
    std::mutex mutex;
    RuntimeInfo::FetchFlags fetchedFlags = RuntimeInfo::FetchFlag::NONE;
};

template <typename T, typename F>
static std::shared_ptr<const T> Get(
        LockedSharedPtr<T> *ptr,
        bool skipCache,
        const F &fetchAllInformation) {
    std::unique_lock<std::mutex> _lock(ptr->mutex);
    if (skipCache || !ptr->fetchedOnce) {
        ptr->object = std::make_unique<T>();
        std::string error;
        if (fetchAllInformation(ptr->object.get(), &error) != OK) {
            LOG(WARNING) << error;
            ptr->object = nullptr; // frees the old object
        }
        ptr->fetchedOnce = true;
    }
    return ptr->object;
}

// static
std::shared_ptr<const HalManifest> VintfObject::GetDeviceHalManifest(bool skipCache) {
    static LockedSharedPtr<HalManifest> gVendorManifest;
    return Get(&gVendorManifest, skipCache, &VintfObject::FetchDeviceHalManifest);
}

// static
std::shared_ptr<const HalManifest> VintfObject::GetFrameworkHalManifest(bool skipCache) {
    static LockedSharedPtr<HalManifest> gFrameworkManifest;
    return Get(&gFrameworkManifest, skipCache,
               std::bind(&HalManifest::fetchAllInformation, _1, kSystemManifest, _2));
}


// static
std::shared_ptr<const CompatibilityMatrix> VintfObject::GetDeviceCompatibilityMatrix(bool skipCache) {
    static LockedSharedPtr<CompatibilityMatrix> gDeviceMatrix;
    return Get(&gDeviceMatrix, skipCache,
               std::bind(&CompatibilityMatrix::fetchAllInformation, _1, kVendorLegacyMatrix, _2));
}

// static
std::shared_ptr<const CompatibilityMatrix> VintfObject::GetFrameworkCompatibilityMatrix(bool skipCache) {
    static LockedSharedPtr<CompatibilityMatrix> gFrameworkMatrix;
    static LockedSharedPtr<CompatibilityMatrix> gCombinedFrameworkMatrix;
    static std::mutex gFrameworkCompatibilityMatrixMutex;

    // To avoid deadlock, get device manifest before any locks.
    auto deviceManifest = GetDeviceHalManifest();

    std::unique_lock<std::mutex> _lock(gFrameworkCompatibilityMatrixMutex);

    auto combined =
        Get(&gCombinedFrameworkMatrix, skipCache,
            std::bind(&VintfObject::GetCombinedFrameworkMatrix, deviceManifest, _1, _2));
    if (combined != nullptr) {
        return combined;
    }

    return Get(&gFrameworkMatrix, skipCache,
               std::bind(&CompatibilityMatrix::fetchAllInformation, _1, kSystemLegacyMatrix, _2));
}

status_t VintfObject::GetCombinedFrameworkMatrix(
    const std::shared_ptr<const HalManifest>& deviceManifest, CompatibilityMatrix* out,
    std::string* error) {
    auto matrixFragments = GetAllFrameworkMatrixLevels(error);
    if (matrixFragments.empty()) {
        return NAME_NOT_FOUND;
    }

    Level deviceLevel = Level::UNSPECIFIED;

    if (deviceManifest != nullptr) {
        deviceLevel = deviceManifest->level();
    }

    // TODO(b/70628538): Do not infer from Shipping API level.
#ifdef LIBVINTF_TARGET
    if (deviceLevel == Level::UNSPECIFIED) {
        auto shippingApi =
            android::base::GetUintProperty<uint64_t>("ro.product.first_api_level", 0u);
        if (shippingApi != 0u) {
            deviceLevel = details::convertFromApiLevel(shippingApi);
        }
    }
#endif

    if (deviceLevel == Level::UNSPECIFIED) {
        // Cannot infer FCM version. Combine all matrices by assuming
        // Shipping FCM Version == min(all supported FCM Versions in the framework)
        for (auto&& pair : matrixFragments) {
            Level fragmentLevel = pair.object.level();
            if (fragmentLevel != Level::UNSPECIFIED && deviceLevel > fragmentLevel) {
                deviceLevel = fragmentLevel;
            }
        }
    }

    if (deviceLevel == Level::UNSPECIFIED) {
        // None of the fragments specify any FCM version. Should never happen except
        // for inconsistent builds.
        if (error) {
            *error = "No framework compatibility matrix files under " + kSystemVintfDir +
                     " declare FCM version.";
        }
        return NAME_NOT_FOUND;
    }

    CompatibilityMatrix* combined =
        CompatibilityMatrix::combine(deviceLevel, &matrixFragments, error);
    if (combined == nullptr) {
        return BAD_VALUE;
    }
    *out = std::move(*combined);
    return OK;
}

// Priority for loading vendor manifest:
// 1. If {sku} sysprop is set and both files exist,
// /vendor/etc/vintf/manifest.xml + /odm/etc/manifest_{sku}.xml
// 2. If both files exist,
// /vendor/etc/vintf/manifest.xml + /odm/etc/manifest.xml
// 3. If file exists, /vendor/etc/vintf/manifest.xml
// 4. If {sku} sysprop is set and file exists,
// /odm/etc/manifest_{sku}.xml
// 5. If file exists, /odm/etc/manifest.xml
// 6. If file exists, /vendor/manifest.xml
// where:
// {sku} is the value of ro.boot.product.hardware.sku
// A + B means adding <hal> tags from B to A (so that <hal>s from B can override A)
status_t VintfObject::FetchDeviceHalManifest(HalManifest* out, std::string* error) {
    // fetchAllInformation returns NAME_NOT_FOUND if file is missing.
    HalManifest vendorManifest;
    status_t vendorStatus = vendorManifest.fetchAllInformation(kVendorManifest, error);
    if (vendorStatus != OK && vendorStatus != NAME_NOT_FOUND) {
        return vendorStatus;
    }

    HalManifest odmManifest;
    status_t odmStatus = NAME_NOT_FOUND;

#ifdef LIBVINTF_TARGET
    std::string productModel = android::base::GetProperty("ro.boot.product.hardware.sku", "");
    if (!productModel.empty()) {
        odmStatus = odmManifest.fetchAllInformation(
            kOdmLegacyVintfDir + "manifest_" + productModel + ".xml", error);
        if (odmStatus != OK && odmStatus != NAME_NOT_FOUND) {
            return odmStatus;
        }
    }
#endif

    if (odmStatus == NAME_NOT_FOUND) {
        odmStatus = odmManifest.fetchAllInformation(kOdmLegacyManifest, error);
        if (odmStatus != OK && odmStatus != NAME_NOT_FOUND) {
            return odmStatus;
        }
    }

    // Both files exist. Use vendor manifest as base manifest and let ODM manifest override it.
    if (vendorStatus == OK && odmStatus == OK) {
        *out = std::move(vendorManifest);
        out->addAllHals(&odmManifest);
        return OK;
    }

    // Only vendor manifest exists. Use it.
    if (vendorStatus == OK) {
        *out = std::move(vendorManifest);
        return OK;
    }

    // Only ODM manifest exists. use it.
    if (odmStatus == OK) {
        *out = std::move(odmManifest);
        return OK;
    }

    // Use legacy /vendor/manifest.xml
    return out->fetchAllInformation(kVendorLegacyManifest, error);
}

std::vector<Named<CompatibilityMatrix>> VintfObject::GetAllFrameworkMatrixLevels(
    std::string* error) {
    std::vector<std::string> fileNames;
    std::vector<Named<CompatibilityMatrix>> results;

    if (details::gFetcher->listFiles(kSystemVintfDir, &fileNames, error) != OK) {
        return {};
    }
    for (const std::string& fileName : fileNames) {
        std::string path = kSystemVintfDir + fileName;

        std::string content;
        std::string fetchError;
        status_t status = details::gFetcher->fetch(path, content, &fetchError);
        if (status != OK) {
            if (error) {
                *error += "Ignore file " + path + ": " + fetchError + "\n";
            }
            continue;
        }

        auto it = results.emplace(results.end());
        if (!gCompatibilityMatrixConverter(&it->object, content)) {
            if (error) {
                // TODO(b/71874788): do not use lastError() because it is not thread-safe.
                *error +=
                    "Ignore file " + path + ": " + gCompatibilityMatrixConverter.lastError() + "\n";
            }
            results.erase(it);
            continue;
        }
    }

    if (results.empty()) {
        if (error) {
            *error = "No framework matrices under " + kSystemVintfDir +
                     " can be fetched or parsed.\n" + *error;
        }
    } else {
        if (error && !error->empty()) {
            LOG(WARNING) << *error;
            *error = "";
        }
    }

    return results;
}

// static
std::shared_ptr<const RuntimeInfo> VintfObject::GetRuntimeInfo(bool skipCache,
                                                               RuntimeInfo::FetchFlags flags) {
    static LockedRuntimeInfoCache gDeviceRuntimeInfo;
    std::unique_lock<std::mutex> _lock(gDeviceRuntimeInfo.mutex);

    if (!skipCache) {
        flags &= (~gDeviceRuntimeInfo.fetchedFlags);
    }

    if (gDeviceRuntimeInfo.object == nullptr) {
        gDeviceRuntimeInfo.object = details::gRuntimeInfoFactory->make_shared();
    }

    status_t status = gDeviceRuntimeInfo.object->fetchAllInformation(flags);
    if (status != OK) {
        gDeviceRuntimeInfo.fetchedFlags &= (~flags);  // mark the fields as "not fetched"
        return nullptr;
    }

    gDeviceRuntimeInfo.fetchedFlags |= flags;
    return gDeviceRuntimeInfo.object;
}

namespace details {

enum class ParseStatus {
    OK,
    PARSE_ERROR,
    DUPLICATED_FWK_ENTRY,
    DUPLICATED_DEV_ENTRY,
};

static std::string toString(ParseStatus status) {
    switch(status) {
        case ParseStatus::OK:                   return "OK";
        case ParseStatus::PARSE_ERROR:          return "parse error";
        case ParseStatus::DUPLICATED_FWK_ENTRY: return "duplicated framework";
        case ParseStatus::DUPLICATED_DEV_ENTRY: return "duplicated device";
    }
    return "";
}

template<typename T>
static ParseStatus tryParse(const std::string &xml, const XmlConverter<T> &parse,
        std::shared_ptr<T> *fwk, std::shared_ptr<T> *dev) {
    std::shared_ptr<T> ret = std::make_shared<T>();
    if (!parse(ret.get(), xml)) {
        return ParseStatus::PARSE_ERROR;
    }
    if (ret->type() == SchemaType::FRAMEWORK) {
        if (fwk->get() != nullptr) {
            return ParseStatus::DUPLICATED_FWK_ENTRY;
        }
        *fwk = std::move(ret);
    } else if (ret->type() == SchemaType::DEVICE) {
        if (dev->get() != nullptr) {
            return ParseStatus::DUPLICATED_DEV_ENTRY;
        }
        *dev = std::move(ret);
    }
    return ParseStatus::OK;
}

template<typename T, typename GetFunction>
static status_t getMissing(const std::shared_ptr<T>& pkg, bool mount,
        std::function<status_t(void)> mountFunction,
        std::shared_ptr<const T>* updated,
        GetFunction getFunction) {
    if (pkg != nullptr) {
        *updated = pkg;
    } else {
        if (mount) {
            (void)mountFunction(); // ignore mount errors
        }
        *updated = getFunction();
    }
    return OK;
}

#define ADD_MESSAGE(__error__)  \
    if (error != nullptr) {     \
        *error += (__error__);  \
    }                           \

struct PackageInfo {
    struct Pair {
        std::shared_ptr<HalManifest>         manifest;
        std::shared_ptr<CompatibilityMatrix> matrix;
    };
    Pair dev;
    Pair fwk;
};

struct UpdatedInfo {
    struct Pair {
        std::shared_ptr<const HalManifest>         manifest;
        std::shared_ptr<const CompatibilityMatrix> matrix;
    };
    Pair dev;
    Pair fwk;
    std::shared_ptr<const RuntimeInfo> runtimeInfo;
};

// Checks given compatibility info against info on the device. If no
// compatability info is given then the device info will be checked against
// itself.
int32_t checkCompatibility(const std::vector<std::string>& xmls, bool mount,
                           const PartitionMounter& mounter, std::string* error,
                           DisabledChecks disabledChecks) {
    status_t status;
    ParseStatus parseStatus;
    PackageInfo pkg; // All information from package.
    UpdatedInfo updated; // All files and runtime info after the update.

    // parse all information from package
    for (const auto &xml : xmls) {
        parseStatus = tryParse(xml, gHalManifestConverter, &pkg.fwk.manifest, &pkg.dev.manifest);
        if (parseStatus == ParseStatus::OK) {
            continue; // work on next one
        }
        if (parseStatus != ParseStatus::PARSE_ERROR) {
            ADD_MESSAGE(toString(parseStatus) + " manifest");
            return ALREADY_EXISTS;
        }
        parseStatus = tryParse(xml, gCompatibilityMatrixConverter, &pkg.fwk.matrix, &pkg.dev.matrix);
        if (parseStatus == ParseStatus::OK) {
            continue; // work on next one
        }
        if (parseStatus != ParseStatus::PARSE_ERROR) {
            ADD_MESSAGE(toString(parseStatus) + " matrix");
            return ALREADY_EXISTS;
        }
        ADD_MESSAGE(toString(parseStatus)); // parse error
        return BAD_VALUE;
    }

    // get missing info from device
    // use functions instead of std::bind because std::bind doesn't work well with mock objects
    auto mountSystem = [&mounter] { return mounter.mountSystem(); };
    auto mountVendor = [&mounter] { return mounter.mountVendor(); };
    if ((status = getMissing(
             pkg.fwk.manifest, mount, mountSystem, &updated.fwk.manifest,
             std::bind(VintfObject::GetFrameworkHalManifest, true /* skipCache */))) != OK) {
        return status;
    }
    if ((status = getMissing(
             pkg.dev.manifest, mount, mountVendor, &updated.dev.manifest,
             std::bind(VintfObject::GetDeviceHalManifest, true /* skipCache */))) != OK) {
        return status;
    }
    if ((status = getMissing(
             pkg.fwk.matrix, mount, mountSystem, &updated.fwk.matrix,
             std::bind(VintfObject::GetFrameworkCompatibilityMatrix, true /* skipCache */))) !=
        OK) {
        return status;
    }
    if ((status = getMissing(
             pkg.dev.matrix, mount, mountVendor, &updated.dev.matrix,
             std::bind(VintfObject::GetDeviceCompatibilityMatrix, true /* skipCache */))) != OK) {
        return status;
    }

    if (mount) {
        (void)mounter.umountSystem(); // ignore errors
        (void)mounter.umountVendor(); // ignore errors
    }

    updated.runtimeInfo = VintfObject::GetRuntimeInfo(true /* skipCache */);

    // null checks for files and runtime info after the update
    // TODO(b/37321309) if a compat mat is missing, it is not matched and considered compatible.
    if (updated.fwk.manifest == nullptr) {
        ADD_MESSAGE("No framework manifest file from device or from update package");
        return NO_INIT;
    }
    if (updated.dev.manifest == nullptr) {
        ADD_MESSAGE("No device manifest file from device or from update package");
        return NO_INIT;
    }
    if (updated.fwk.matrix == nullptr) {
        ADD_MESSAGE("No framework matrix, skipping;");
        // TODO(b/37321309) consider missing matricies as errors.
    }
    if (updated.dev.matrix == nullptr) {
        ADD_MESSAGE("No device matrix, skipping;");
        // TODO(b/37321309) consider missing matricies as errors.
    }
    if (updated.runtimeInfo == nullptr) {
        ADD_MESSAGE("No runtime info from device");
        return NO_INIT;
    }

    // compatiblity check.
    // TODO(b/37321309) outer if checks can be removed if we consider missing matrices as errors.
    if (updated.dev.manifest && updated.fwk.matrix) {
        if (!updated.dev.manifest->checkCompatibility(*updated.fwk.matrix, error)) {
            if (error)
                error->insert(0, "Device manifest and framework compatibility matrix "
                                 "are incompatible: ");
            return INCOMPATIBLE;
        }
    }
    if (updated.fwk.manifest && updated.dev.matrix) {
        if (!updated.fwk.manifest->checkCompatibility(*updated.dev.matrix, error)) {
            if (error)
                error->insert(0, "Framework manifest and device compatibility matrix "
                                 "are incompatible: ");
            return INCOMPATIBLE;
        }
    }
    if (updated.runtimeInfo && updated.fwk.matrix) {
        if (!updated.runtimeInfo->checkCompatibility(*updated.fwk.matrix, error, disabledChecks)) {
            if (error)
                error->insert(0, "Runtime info and framework compatibility matrix "
                                 "are incompatible: ");
            return INCOMPATIBLE;
        }
    }

    return COMPATIBLE;
}

const std::string kSystemVintfDir = "/system/etc/vintf/";
const std::string kVendorVintfDir = "/vendor/etc/vintf/";

const std::string kVendorManifest = kVendorVintfDir + "manifest.xml";
const std::string kSystemManifest = "/system/manifest.xml";

const std::string kVendorLegacyManifest = "/vendor/manifest.xml";
const std::string kVendorLegacyMatrix = "/vendor/compatibility_matrix.xml";
const std::string kSystemLegacyMatrix = "/system/compatibility_matrix.xml";
const std::string kOdmLegacyVintfDir = "/odm/etc/";
const std::string kOdmLegacyManifest = kOdmLegacyVintfDir + "manifest.xml";

} // namespace details

// static
int32_t VintfObject::CheckCompatibility(const std::vector<std::string>& xmls, std::string* error,
                                        DisabledChecks disabledChecks) {
    return details::checkCompatibility(xmls, false /* mount */, *details::gPartitionMounter, error,
                                       disabledChecks);
}


} // namespace vintf
} // namespace android
