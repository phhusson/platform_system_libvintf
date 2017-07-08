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

#include "KernelConfigParser.h"

#include <regex>

namespace android {
namespace vintf {

KernelConfigParser::KernelConfigParser(bool processComments) : mProcessComments(processComments) {}

status_t KernelConfigParser::finish() {
    return process("\n", 1 /* sizeof "\n" */);
}

std::stringbuf* KernelConfigParser::error() const {
    return mError.rdbuf();
}

std::map<std::string, std::string>& KernelConfigParser::configs() {
    return mConfigs;
}

const std::map<std::string, std::string>& KernelConfigParser::configs() const {
    return mConfigs;
}

status_t KernelConfigParser::processRemaining() {
    static std::regex sCommentPattern("^# (CONFIG[\\w_]+) is not set$");

    if (mRemaining.empty()) {
        return OK;
    }

    if (mRemaining[0] == '#') {
        if (!mProcessComments) {
            return OK;
        }
        std::smatch sm;
        if (!std::regex_match(mRemaining, sm, sCommentPattern)) {
            return OK;  // ignore this comment;
        }
        if (!mConfigs.emplace(sm[1], "n").second) {
            mError << "Key " << sm[1] << " is set but commented as not set"
                   << "\n";
            return UNKNOWN_ERROR;
        }

        return OK;
    }

    size_t equalPos = mRemaining.find('=');
    if (equalPos == std::string::npos) {
        mError << "Unrecognized line in configs: " << mRemaining << "\n";
        return UNKNOWN_ERROR;
    }
    std::string key = mRemaining.substr(0, equalPos);
    std::string value = mRemaining.substr(equalPos + 1);
    if (!mConfigs.emplace(std::move(key), std::move(value)).second) {
        mError << "Duplicated key in configs: " << mRemaining.substr(0, equalPos) << "\n";
        return UNKNOWN_ERROR;
    }

    return OK;
}

status_t KernelConfigParser::process(const char* buf, size_t len) {
    const char* begin = buf;
    const char* end = buf;
    const char* stop = buf + len;
    status_t err = OK;
    while (end < stop) {
        if (*end == '\n') {
            mRemaining.insert(mRemaining.size(), begin, end - begin);
            status_t newErr = processRemaining();
            if (newErr != OK && err == OK) {
                err = newErr;
                // but continue to get more
            }
            mRemaining.clear();
            begin = end + 1;
        }
        end++;
    }
    mRemaining.insert(mRemaining.size(), begin, end - begin);
    return err;
}

}  // namespace vintf
}  // namespace android
