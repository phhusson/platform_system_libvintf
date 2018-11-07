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
#include <sys/stat.h>

#include <set>

#include <android-base/logging.h>
#include <android-base/strings.h>
#include <fs_mgr.h>

#include "utils.h"

namespace android {
namespace vintf {

namespace details {
using android::base::StartsWith;
using FstabMgr = std::unique_ptr<struct fstab, decltype(&fs_mgr_free_fstab)>;

static const char* const kMountImageRootDir = "/mnt";
static const char* const kSystemImageRootDir = "/mnt/system";

static status_t mountAt(const FstabMgr& fstab, const char* path, const char* mountPoint) {
    mkdir(mountPoint, 0755);  // in case it doesn't already exist

    fstab_rec* rec = fs_mgr_get_entry_for_mount_point(fstab.get(), path);
    if (rec == nullptr) {
        LOG(WARNING) << "No mount point for " << path;
        return NAME_NOT_FOUND;
    }
    int result = mount(rec->blk_device, mountPoint, rec->fs_type, rec->flags, rec->fs_options);
    if (result != 0) {
        PLOG(WARNING) << "Can't mount " << path;
    }
    return result == 0 ? OK : -errno;
}

static FstabMgr defaultFstabMgr() {
    return FstabMgr{fs_mgr_read_fstab_default(), &fs_mgr_free_fstab};
}

class RecoveryPartitionMounter {
   public:
    RecoveryPartitionMounter() : fstab_(defaultFstabMgr()) {}
    ~RecoveryPartitionMounter() {
        for (const auto& pair : mounted_) {
            umount(pair.second.c_str());
        }
        mounted_.clear();
    }

    status_t mount(const std::string& path) {
        if (path == "/system") {
            if (!fstab_) return UNKNOWN_ERROR;
            if (fs_mgr_get_entry_for_mount_point(fstab_.get(), "/system") == nullptr) {
                return mount("/", kSystemImageRootDir);
            } else {
                return mount("/system", kSystemImageRootDir);
            }
        }
        return mount(path, kMountImageRootDir + path);
    }

   private:
    FstabMgr fstab_;
    std::map<std::string, std::string> mounted_;

    status_t mount(const std::string& path, const std::string& mountPoint) {
        if (mounted_.find(path) != mounted_.end()) {
            return OK;
        }

        if (!fstab_) return UNKNOWN_ERROR;
        status_t status = mountAt(fstab_, path.c_str(), mountPoint.c_str());
        if (status == OK) {
            mounted_.emplace(path, mountPoint);
        }
        return status;
    }
};

class RecoveryFileSystem : public FileSystem {
   public:
    RecoveryFileSystem() = default;

    status_t fetch(const std::string& path, std::string* fetched, std::string* error) const {
        const FileSystem* fs = nullptr;
        status_t err = getFileSystem(path, &fs, error);
        if (err != OK) return err;
        return fs->fetch(path, fetched, error);
    }

    status_t listFiles(const std::string& path, std::vector<std::string>* out,
                       std::string* error) const {
        const FileSystem* fs = nullptr;
        status_t err = getFileSystem(path, &fs, error);
        if (err != OK) return err;
        return fs->listFiles(path, out, error);
    }

   private:
    FileSystemUnderPath mSystemFileSystem{kSystemImageRootDir};
    FileSystemUnderPath mMntFileSystem{kMountImageRootDir};
    std::unique_ptr<RecoveryPartitionMounter> mMounter =
        std::make_unique<RecoveryPartitionMounter>();

    status_t getFileSystem(const std::string& path, const FileSystem** fs,
                           std::string* error) const {
        auto partition = GetPartition(path);
        if (partition.empty()) {
            if (error) *error = "Cannot list or fetch relative path " + path;
            return NAME_NOT_FOUND;
        }
        status_t err = mMounter->mount(partition);
        if (err != OK) {
            // in recovery, ignore mount errors and assume the file does not exist.
            if (error) *error = "Cannot mount for path " + path + ": " + strerror(-err);
            return NAME_NOT_FOUND;
        }

        // /system files are under /mnt/system/system because system.img contains the root dir.
        if (partition == "/system") {
            *fs = &mSystemFileSystem;
        } else {
            *fs = &mMntFileSystem;
        }
        return OK;
    }

    // /system -> /system
    // /system/foo -> /system
    static std::string GetPartition(const std::string& path) {
        if (path.empty()) return "";
        if (path[0] != '/') return "";
        auto idx = path.find('/', 1);
        if (idx == std::string::npos) return path;
        return path.substr(0, idx);
    }
};

} // namespace details

// static
int32_t VintfObjectRecovery::CheckCompatibility(
        const std::vector<std::string> &xmls, std::string *error) {
    auto propertyFetcher = std::make_unique<details::PropertyFetcherImpl>();
    auto fileSystem = std::make_unique<details::RecoveryFileSystem>();
    auto vintfObject = std::make_unique<VintfObject>(
        std::move(fileSystem), nullptr /* runtime info factory */, std::move(propertyFetcher));
    return vintfObject->checkCompatibility(xmls, error);
}


} // namespace vintf
} // namespace android
