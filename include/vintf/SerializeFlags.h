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
    VINTF_SERIALIZE_FLAGS_FIELD_DECLARE(MetaVersion)
    VINTF_SERIALIZE_FLAGS_FIELD_DECLARE(SchemaType)

#undef VINTF_SERIALIZE_FLAGS_FIELD_DECLARE

    static const SerializeFlags NO_HALS;
    static const SerializeFlags NO_AVB;
    static const SerializeFlags NO_SEPOLICY;
    static const SerializeFlags NO_VNDK;
    static const SerializeFlags NO_KERNEL;
    static const SerializeFlags NO_XMLFILES;
    static const SerializeFlags NO_SSDK;
    static const SerializeFlags NO_FQNAME;
    static const SerializeFlags NO_KERNEL_CONFIGS;
    static const SerializeFlags NO_KERNEL_MINOR_REVISION;

    static const SerializeFlags EVERYTHING;
    static const SerializeFlags NO_TAGS;
    static const SerializeFlags HALS_ONLY;
    static const SerializeFlags XMLFILES_ONLY;
    static const SerializeFlags SEPOLICY_ONLY;
    static const SerializeFlags VNDK_ONLY;
    static const SerializeFlags HALS_NO_FQNAME;
    static const SerializeFlags SSDK_ONLY;

   private:
    uint32_t mValue;

    SerializeFlags(uint32_t value);
};

}  // namespace vintf
}  // namespace android

#endif  // ANDROID_VINTF_SERIALIZE_FLAGS_H
