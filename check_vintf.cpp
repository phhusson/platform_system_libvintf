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

#include <iostream>

#include <vintf/parse_xml.h>
#include "utils.h"

namespace android {
namespace vintf {

template <typename T>
std::unique_ptr<T> readObject(const std::string& path, const XmlConverter<T>& converter) {
    std::string xml;
    std::string error;
    status_t err = details::gFetcher->fetch(path, xml, &error);
    if (err != OK) {
        std::cerr << "Error: Cannot read '" << path << "' (" << strerror(-err) << "): " << error
                  << std::endl;
        return nullptr;
    }
    auto ret = std::make_unique<T>();
    if (!converter(ret.get(), xml)) {
        std::cerr << "Error: Cannot parse '" << path << "': " << converter.lastError() << std::endl;
        return nullptr;
    }
    return ret;
}

}  // namespace vintf
}  // namespace android

int main(int argc, char** argv) {
    using namespace android::vintf;
    if (argc < 3) {
        std::cerr << "usage: " << argv[0] << " <manifest.xml> <matrix.xml>" << std::endl
                  << "    Checks compatibility between a manifest and a compatibility matrix."
                  << std::endl;
        return -1;
    }

    auto manifest = readObject(argv[1], gHalManifestConverter);
    auto matrix = readObject(argv[2], gCompatibilityMatrixConverter);
    if (manifest == nullptr || matrix == nullptr) {
        return -1;
    }

    std::string error;
    if (!manifest->checkCompatibility(*matrix, &error)) {
        std::cerr << "Error: Incompatible: " << error << std::endl;
        std::cout << "false" << std::endl;
        return 1;
    }

    std::cout << "true" << std::endl;
    return 0;
}
