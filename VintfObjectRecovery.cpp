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

#include "VintfObjectRecovery.h"

#include <fs_mgr.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <android-base/strings.h>

#include "utils.h"

namespace android {
namespace vintf {

namespace details {
using android::base::StartsWith;
using FstabMgr = std::unique_ptr<struct fstab, decltype(&fs_mgr_free_fstab)>;

static const char* const kSystemImageRootDir = "/mnt/system";
static const char* const kVendorImageRootDir = "/mnt/vendor";

static status_t mountAt(const FstabMgr& fstab, const char* path, const char* mountPoint) {
    mkdir(mountPoint, 0755);  // in case it doesn't already exist

    fstab_rec* rec = fs_mgr_get_entry_for_mount_point(fstab.get(), path);
    if (rec == nullptr) {
        return UNKNOWN_ERROR;
    }
    int result = mount(rec->blk_device, mountPoint, rec->fs_type, rec->flags, rec->fs_options);
    return result == 0 ? OK : -errno;
}

static FstabMgr defaultFstabMgr() {
    return FstabMgr{fs_mgr_read_fstab_default(), &fs_mgr_free_fstab};
}

class RecoveryPartitionMounter : public PartitionMounter {
   public:
    RecoveryPartitionMounter(bool systemRootImage) : mSystemRootImage(systemRootImage) {}
    status_t mountSystem() const override {
        if (mSystemRootImage) {
            return mount("/", kSystemImageRootDir);
        } else {
            return mount("/system", kSystemImageRootDir);
        }
    }

    status_t mountVendor() const override { return mount("/vendor", kVendorImageRootDir); }

    status_t umountSystem() const override { return umount(kSystemImageRootDir); }

    status_t umountVendor() const override { return umount(kVendorImageRootDir); }

   private:
    const bool mSystemRootImage = false;

    status_t mount(const char* path, const char* mountPoint) const {
        FstabMgr fstab = defaultFstabMgr();
        if (fstab == NULL) {
            return UNKNOWN_ERROR;
        }
        return mountAt(fstab, path, mountPoint);
    }
};

class RecoveryFileSystem : public FileSystem {
   public:
    RecoveryFileSystem() = default;

    status_t fetch(const std::string& path, std::string* fetched, std::string* error) const {
        return getFileSystem(path).fetch(path, fetched, error);
    }

    status_t listFiles(const std::string& path, std::vector<std::string>* out,
                       std::string* error) const {
        return getFileSystem(path).listFiles(path, out, error);
    }

   private:
    FileSystemUnderPath mSystemFileSystem{"/mnt/system"};
    FileSystemUnderPath mMntFileSystem{"/mnt"};

    const FileSystemUnderPath& getFileSystem(const std::string& path) const {
        // /system files are under /mnt/system/system because system.img contains the root dir.
        if (StartsWith(path, "/system")) {
            return mSystemFileSystem;
        }
        return mMntFileSystem;
    }
};

} // namespace details

// static
int32_t VintfObjectRecovery::CheckCompatibility(
        const std::vector<std::string> &xmls, std::string *error) {
    auto propertyFetcher = std::make_unique<details::PropertyFetcherImpl>();
    bool systemRootImage = propertyFetcher->getBoolProperty("ro.build.system_root_image", false);
    auto mounter = std::make_unique<details::RecoveryPartitionMounter>(systemRootImage);
    auto fileSystem = std::make_unique<details::RecoveryFileSystem>();
    auto vintfObject = std::make_unique<VintfObject>(std::move(fileSystem), std::move(mounter),
                                                     nullptr /* runtime info factory */,
                                                     std::move(propertyFetcher));
    return vintfObject->checkCompatibility(xmls, true /* mount */, error);
}


} // namespace vintf
} // namespace android
