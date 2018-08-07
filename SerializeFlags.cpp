/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include <vintf/SerializeFlags.h>

namespace android {
namespace vintf {

SerializeFlags::SerializeFlags(uint32_t value) : mValue(value) {}
SerializeFlags::SerializeFlags(const SerializeFlags& other) : mValue(other.mValue) {}

SerializeFlags SerializeFlags::operator|(SerializeFlags other) const {
    return SerializeFlags(mValue & other.mValue);
}

SerializeFlags SerializeFlags::operator&(SerializeFlags other) const {
    return SerializeFlags(mValue | other.mValue);
}

SerializeFlags& SerializeFlags::operator|=(SerializeFlags other) {
    *this = (*this | other);
    return *this;
}

SerializeFlags::operator bool() const {
    return ~mValue;
}

#define VINTF_SERIALIZE_FLAGS_FIELD_DEFINE(name, bit)      \
    SerializeFlags SerializeFlags::enable##name() const {  \
        SerializeFlags ret(*this);                         \
        ret.mValue |= 1 << bit;                            \
        return ret;                                        \
    }                                                      \
    SerializeFlags SerializeFlags::disable##name() const { \
        SerializeFlags ret(*this);                         \
        ret.mValue &= ~(1 << bit);                         \
        return ret;                                        \
    }                                                      \
    bool SerializeFlags::is##name##Enabled() const { return mValue & (1 << bit); }

VINTF_SERIALIZE_FLAGS_FIELD_DEFINE(Hals, 0)
VINTF_SERIALIZE_FLAGS_FIELD_DEFINE(Avb, 1)
VINTF_SERIALIZE_FLAGS_FIELD_DEFINE(Sepolicy, 2)
VINTF_SERIALIZE_FLAGS_FIELD_DEFINE(Vndk, 3)
VINTF_SERIALIZE_FLAGS_FIELD_DEFINE(Kernel, 4)
VINTF_SERIALIZE_FLAGS_FIELD_DEFINE(XmlFiles, 5)
VINTF_SERIALIZE_FLAGS_FIELD_DEFINE(Ssdk, 6)
VINTF_SERIALIZE_FLAGS_FIELD_DEFINE(Fqname, 7)
VINTF_SERIALIZE_FLAGS_FIELD_DEFINE(KernelConfigs, 8)
VINTF_SERIALIZE_FLAGS_FIELD_DEFINE(KernelMinorRevision, 9)

const SerializeFlags SerializeFlags::EVERYTHING = SerializeFlags(~0);
const SerializeFlags SerializeFlags::NO_HALS = EVERYTHING.disableHals();
const SerializeFlags SerializeFlags::NO_AVB = EVERYTHING.disableAvb();
const SerializeFlags SerializeFlags::NO_SEPOLICY = EVERYTHING.disableSepolicy();
const SerializeFlags SerializeFlags::NO_VNDK = EVERYTHING.disableVndk();
const SerializeFlags SerializeFlags::NO_KERNEL = EVERYTHING.disableKernel();
const SerializeFlags SerializeFlags::NO_XMLFILES = EVERYTHING.disableXmlFiles();
const SerializeFlags SerializeFlags::NO_SSDK = EVERYTHING.disableSsdk();
const SerializeFlags SerializeFlags::NO_FQNAME = EVERYTHING.disableFqname();
const SerializeFlags SerializeFlags::NO_KERNEL_CONFIGS = EVERYTHING.disableKernelConfigs();
const SerializeFlags SerializeFlags::NO_KERNEL_MINOR_REVISION =
    EVERYTHING.disableKernelMinorRevision();

const SerializeFlags SerializeFlags::NO_TAGS = SerializeFlags(0);
const SerializeFlags SerializeFlags::HALS_ONLY =
    NO_TAGS.enableHals().enableFqname();  // <hal> with <fqname>
const SerializeFlags SerializeFlags::XMLFILES_ONLY = NO_TAGS.enableXmlFiles();
const SerializeFlags SerializeFlags::SEPOLICY_ONLY = NO_TAGS.enableSepolicy();
const SerializeFlags SerializeFlags::VNDK_ONLY = NO_TAGS.enableVndk();
const SerializeFlags SerializeFlags::HALS_NO_FQNAME =
    NO_TAGS.enableHals();  // <hal> without <fqname>
const SerializeFlags SerializeFlags::SSDK_ONLY = NO_TAGS.enableSsdk();

}  // namespace vintf
}  // namespace android
