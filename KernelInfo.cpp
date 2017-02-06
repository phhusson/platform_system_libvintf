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


#define LOG_TAG "libvintf"

#include "KernelInfo.h"

#include <errno.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <android-base/logging.h>
#include <selinux/selinux.h>
#include <zlib.h>

#define PROC_CONFIG "/proc/config.gz"
#define BUFFER_SIZE sysconf(_SC_PAGESIZE)

namespace android {
namespace vintf {

static void removeTrailingComments(std::string *s) {
    size_t sharpPos = s->find('#');
    if (sharpPos != std::string::npos) {
        s->erase(sharpPos);
    }
}
static void trim(std::string *s) {
    auto l = s->begin();
    for (; l != s->end() && std::isspace(*l); ++l);
    s->erase(s->begin(), l);
    auto r = s->rbegin();
    for (; r != s->rend() && std::isspace(*r); ++r);
    s->erase(r.base(), s->end());
}

struct KernelInfoFetcher {
    KernelInfoFetcher(KernelInfo *ki) : mKernelInfo(ki) { }
    status_t fetchAllInformation();
private:
    void streamConfig(const char *buf, size_t len);
    void parseConfig(std::string *s);
    status_t fetchVersion();
    status_t fetchKernelConfigs();
    status_t fetchCpuInfo();
    status_t fetchKernelSepolicyVers();
    status_t fetchSepolicyFiles();
    KernelInfo *mKernelInfo;
    std::string mRemaining;
};

// decompress /proc/config.gz and read its contents.
status_t KernelInfoFetcher::fetchKernelConfigs() {
    gzFile f = gzopen(PROC_CONFIG, "rb");
    if (f == NULL) {
        LOG(ERROR) << "Could not open /proc/config.gz: " << errno;
        return -errno;
    }

    char buf[BUFFER_SIZE];
    int len;
    while ((len = gzread(f, buf, sizeof buf)) > 0) {
        streamConfig(buf, len);
    }
    status_t err = OK;
    if (len < 0) {
        int errnum;
        const char *errmsg = gzerror(f, &errnum);
        LOG(ERROR) << "Could not read /proc/config.gz: " << errmsg;
        err = (errnum == Z_ERRNO ? -errno : errnum);
    }

    // stream a "\n" to end the stream to finish the last line.
    streamConfig("\n", 1 /* sizeof "\n" */);

    gzclose(f);
    return err;
}

void KernelInfoFetcher::parseConfig(std::string *s) {
    removeTrailingComments(s);
    trim(s);
    if (s->empty()) {
        return;
    }
    size_t equalPos = s->find('=');
    if (equalPos == std::string::npos) {
        LOG(WARNING) << "Unrecognized line in /proc/config.gz: " << *s;
        return;
    }
    std::string key = s->substr(0, equalPos);
    std::string value = s->substr(equalPos + 1);
    if (!mKernelInfo->kernelConfigs.emplace(std::move(key), std::move(value)).second) {
        LOG(WARNING) << "Duplicated key in /proc/config.gz: " << s->substr(0, equalPos);
        return;
    }
}

void KernelInfoFetcher::streamConfig(const char *buf, size_t len) {
    const char *begin = buf;
    const char *end = buf;
    const char *stop = buf + len;
    while (end < stop) {
        if (*end == '\n') {
            mRemaining.insert(mRemaining.size(), begin, end - begin);
            parseConfig(&mRemaining);
            mRemaining.clear();
            begin = end + 1;
        }
        end++;
    }
    mRemaining.insert(mRemaining.size(), begin, end - begin);
}

status_t KernelInfoFetcher::fetchCpuInfo() {
    // TODO implement this; 32-bit and 64-bit has different format.
    return OK;
}

status_t KernelInfoFetcher::fetchKernelSepolicyVers() {
    int pv = security_policyvers();
    if (pv < 0) {
        return pv;
    }
    mKernelInfo->mKernelSepolicyVersion = pv;
    return OK;
}

status_t KernelInfoFetcher::fetchVersion() {
    struct utsname buf;
    if (uname(&buf)) {
        return -errno;
    }
    mKernelInfo->mOsName = buf.sysname;
    mKernelInfo->mNodeName = buf.nodename;
    mKernelInfo->mOsRelease = buf.release;
    mKernelInfo->mOsVersion = buf.version;
    mKernelInfo->mHardwareId = buf.machine;
    return OK;
}

// Grab sepolicy files.
status_t KernelInfoFetcher::fetchSepolicyFiles() {
    // TODO implement this
    return OK;
}

status_t KernelInfoFetcher::fetchAllInformation() {
    status_t err;
    if ((err = fetchVersion()) != OK) {
        return err;
    }
    if ((err = fetchKernelConfigs()) != OK) {
        return err;
    }
    if ((err = fetchCpuInfo()) != OK) {
        return err;
    }
    if ((err = fetchKernelSepolicyVers()) != OK) {
        return err;
    }
    if ((err = fetchSepolicyFiles()) != OK) {
        return err;
    }
    return OK;
}


const std::string &KernelInfo::osName() const {
    return mOsName;
}

const std::string &KernelInfo::nodeName() const {
    return mNodeName;
}

const std::string &KernelInfo::osRelease() const {
    return mOsRelease;
}

const std::string &KernelInfo::osVersion() const {
    return mOsVersion;
}

const std::string &KernelInfo::hardwareId() const {
    return mHardwareId;
}

size_t KernelInfo::kernelSepolicyVersion() const {
    return mKernelSepolicyVersion;
}

void KernelInfo::clear() {
    kernelConfigs.clear();
    mOsName.clear();
    mNodeName.clear();
    mOsRelease.clear();
    mOsVersion.clear();
    mHardwareId.clear();
}

const KernelInfo *KernelInfo::Get() {
    static KernelInfo ki{};
    static KernelInfo *kip = nullptr;
    static std::mutex mutex{};

    std::lock_guard<std::mutex> lock(mutex);
    if (kip == nullptr) {
        if (KernelInfoFetcher(&ki).fetchAllInformation() == OK) {
            kip = &ki;
        } else {
            ki.clear();
            return nullptr;
        }
    }

    return kip;
}

} // namespace vintf
} // namespace android
