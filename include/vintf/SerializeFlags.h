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

#ifndef ANDROID_VINTF_SERIALIZE_FLAGS_H
#define ANDROID_VINTF_SERIALIZE_FLAGS_H

#include <stdint.h>

namespace android {
namespace vintf {

// legacy single SerializeFlag values. Use SerializeFlags class instead.
enum SerializeFlag : uint32_t {
    NO_HALS = 1 << 0,
    NO_AVB = 1 << 1,
    NO_SEPOLICY = 1 << 2,
    NO_VNDK = 1 << 3,
    NO_KERNEL = 1 << 4,
    NO_XMLFILES = 1 << 5,
    NO_SSDK = 1 << 6,
    NO_FQNAME = 1 << 7,
    NO_KERNEL_CONFIGS = 1 << 8,
    NO_KERNEL_MINOR_REVISION = 1 << 9,

    EVERYTHING = 0,
    HALS_ONLY = ~(NO_HALS | NO_FQNAME),  // <hal> with <fqname>
    XMLFILES_ONLY = ~NO_XMLFILES,
    SEPOLICY_ONLY = ~NO_SEPOLICY,
    VNDK_ONLY = ~NO_VNDK,
    HALS_NO_FQNAME = ~NO_HALS,  // <hal> without <fqname>
    SSDK_ONLY = ~NO_SSDK,
};

class SerializeFlags {
   public:
    SerializeFlags(const SerializeFlags& other);

#define VINTF_SERIALIZE_FLAGS_FIELD_DECLARE(name) \
    SerializeFlags enable##name() const;          \
    SerializeFlags disable##name() const;         \
    bool is##name##Enabled() const;

    VINTF_SERIALIZE_FLAGS_FIELD_DECLARE(Hals)
    VINTF_SERIALIZE_FLAGS_FIELD_DECLARE(Avb)
    VINTF_SERIALIZE_FLAGS_FIELD_DECLARE(Sepolicy)
    VINTF_SERIALIZE_FLAGS_FIELD_DECLARE(Vndk)
    VINTF_SERIALIZE_FLAGS_FIELD_DECLARE(Kernel)
    VINTF_SERIALIZE_FLAGS_FIELD_DECLARE(XmlFiles)
    VINTF_SERIALIZE_FLAGS_FIELD_DECLARE(Ssdk)
    VINTF_SERIALIZE_FLAGS_FIELD_DECLARE(Fqname)
    VINTF_SERIALIZE_FLAGS_FIELD_DECLARE(KernelConfigs)
    VINTF_SERIALIZE_FLAGS_FIELD_DECLARE(KernelMinorRevision)

#undef VINTF_SERIALIZE_FLAGS_FIELD_DECLARE

   private:
    uint32_t mValue;

    // Legacy APIs to be compatible with old SerializeFlag usage.
   public:
    SerializeFlags(uint32_t legacyValue);
    SerializeFlags operator|(SerializeFlag other) const;
    SerializeFlags operator&(SerializeFlag other) const;
    SerializeFlags& operator|=(SerializeFlag other);
    operator bool() const;

   private:
    uint32_t legacyValue() const;
};

}  // namespace vintf
}  // namespace android

#endif  // ANDROID_VINTF_SERIALIZE_FLAGS_H
