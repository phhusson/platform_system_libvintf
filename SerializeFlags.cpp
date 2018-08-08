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

SerializeFlags::SerializeFlags(uint32_t legacyValue) : mValue(~legacyValue) {}

SerializeFlags::SerializeFlags(const SerializeFlags& other) : mValue(other.mValue) {}

SerializeFlags SerializeFlags::operator|(SerializeFlag other) const {
    return SerializeFlags(legacyValue() | other);
}

SerializeFlags SerializeFlags::operator&(SerializeFlag other) const {
    return SerializeFlags(legacyValue() & other);
}

SerializeFlags& SerializeFlags::operator|=(SerializeFlag other) {
    *this = (*this | SerializeFlags(other));
    return *this;
}

SerializeFlags::operator bool() const {
    return legacyValue();
}

uint32_t SerializeFlags::legacyValue() const {
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

}  // namespace vintf
}  // namespace android
