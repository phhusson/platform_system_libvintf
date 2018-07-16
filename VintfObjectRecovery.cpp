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

#include <sys/mount.h>
#include <fs_mgr.h>

#include "utils.h"

namespace android {
namespace vintf {

namespace details {
using FstabMgr = std::unique_ptr<struct fstab, decltype(&fs_mgr_free_fstab)>;

static status_t mountAt(const FstabMgr &fstab, const char* path, const char* mount_point) {
    fstab_rec* rec = fs_mgr_get_entry_for_mount_point(fstab.get(), path);
    if (rec == nullptr) {
        return UNKNOWN_ERROR;
    }
    int result = mount(rec->blk_device, mount_point, rec->fs_type, rec->flags, rec->fs_options);
    return result == 0 ? OK : -errno;
}

static FstabMgr defaultFstabMgr() {
    return FstabMgr{fs_mgr_read_fstab_default(), &fs_mgr_free_fstab};
}

class RecoveryPartitionMounter : public PartitionMounter {
   public:
    RecoveryPartitionMounter(PropertyFetcher* propertyFetcher)
        : mPropertyFetcher(propertyFetcher) {}
    status_t mountSystem() const override {
        FstabMgr fstab = defaultFstabMgr();
        if (fstab == NULL) {
            return UNKNOWN_ERROR;
        }
        if (mPropertyFetcher->getBoolProperty("ro.build.system_root_image", false)) {
            return mountAt(fstab, "/", "/system_root");
        } else {
            return mountAt(fstab, "/system", "/system");
        }
    }

    status_t mountVendor() const override {
        FstabMgr fstab = defaultFstabMgr();
        if (fstab == NULL) {
            return UNKNOWN_ERROR;
        }
        return mountAt(fstab, "/vendor", "/vendor");
    }

    status_t umountSystem() const override {
        if (mPropertyFetcher->getBoolProperty("ro.build.system_root_image", false)) {
            return umount("/system_root");
        } else {
            return umount("/system");
        }
    }

    status_t umountVendor() const override {
        return umount("/vendor");
    }

   private:
    PropertyFetcher* mPropertyFetcher{nullptr};
};

} // namespace details

// static
int32_t VintfObjectRecovery::CheckCompatibility(
        const std::vector<std::string> &xmls, std::string *error) {
    auto propertyFetcher = std::make_unique<details::PropertyFetcherImpl>();
    auto mounter = std::make_unique<details::RecoveryPartitionMounter>(propertyFetcher.get());
    auto vintfObject = std::make_unique<VintfObject>(nullptr /* fileSystem */, std::move(mounter),
                                                     nullptr /* runtime info factory */,
                                                     std::move(propertyFetcher));
    return vintfObject->checkCompatibility(xmls, true /* mount */, error);
}


} // namespace vintf
} // namespace android
