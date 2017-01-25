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

#define LOG_TAG "LibHidlTest"

#include <vintf/parse_string.h>
#include <vintf/parse_xml.h>
#include <vintf/CompatibilityMatrix.h>
#include <vintf/VendorManifest.h>

#include <android-base/logging.h>
#include <gtest/gtest.h>

namespace android {
namespace vintf {

struct LibVintfTest : public ::testing::Test {
public:
    virtual void SetUp() override {
    }
    virtual void TearDown() override {
    }
    bool add(CompatibilityMatrix &cm, MatrixHal &&hal) {
        return cm.add(std::move(hal));
    }
    bool add(CompatibilityMatrix &cm, MatrixKernel &&kernel) {
        return cm.add(std::move(kernel));
    }
    bool add(VendorManifest &vm, ManifestHal &&hal) {
        return vm.add(std::move(hal));
    }
    const ManifestHal *getHal(VendorManifest &vm, const std::string &name) {
        return vm.getHal(name);
    }
    ConstMapValueIterable<std::string, ManifestHal> getHals(VendorManifest &vm) {
        return vm.getHals();
    }
    bool isEqual(const CompatibilityMatrix &cm1, const CompatibilityMatrix &cm2) {
        return cm1.hals == cm2.hals && cm1.kernels == cm2.kernels;
    }
    bool isValid(const ManifestHal &mh) {
        return mh.isValid();
    }
    VendorManifest testVendorManifest() {
        VendorManifest vm;
        vm.add(ManifestHal::hal("android.hardware.camera", ImplLevel::SOC, "msm8892",
                Version(2,0), Transport::HWBINDER));
        vm.add(ManifestHal::hal("android.hardware.nfc", ImplLevel::GENERIC, "generic",
                Version(1,0), Transport::PASSTHROUGH));

        return vm;
    }
};


TEST_F(LibVintfTest, Stringify) {
    VendorManifest vm = testVendorManifest();
    EXPECT_EQ(dump(vm), "hidl/android.hardware.camera/hwbinder/soc/msm8892/2.0:"
                        "hidl/android.hardware.nfc/passthrough/generic/generic/1.0");

    EXPECT_EQ(to_string(HalFormat::HIDL), "hidl");
    EXPECT_EQ(to_string(HalFormat::NATIVE), "native");

    VersionRange v(1, 2, 3);
    EXPECT_EQ(to_string(v), "1.2-3");
    VersionRange v2;
    EXPECT_TRUE(parse("1.2-3", &v2));
    EXPECT_EQ(v, v2);
}

TEST_F(LibVintfTest, VendorManifestConverter) {
    VendorManifest vm = testVendorManifest();
    std::string xml = gVendorManifestConverter(vm);
    EXPECT_EQ(xml,
        "<manifest version=\"1.0\">\n"
        "    <hal format=\"hidl\">\n"
        "        <name>android.hardware.camera</name>\n"
        "        <transport>hwbinder</transport>\n"
        "        <impl level=\"soc\">msm8892</impl>\n"
        "        <version>2.0</version>\n"
        "    </hal>\n"
        "    <hal format=\"hidl\">\n"
        "        <name>android.hardware.nfc</name>\n"
        "        <transport>passthrough</transport>\n"
        "        <impl level=\"generic\">generic</impl>\n"
        "        <version>1.0</version>\n"
        "    </hal>\n"
        "</manifest>\n");
}

TEST_F(LibVintfTest, VersionConverter) {
    Version v(3, 6);
    std::string xml = gVersionConverter(v);
    EXPECT_EQ(xml, "<version>3.6</version>\n");
    Version v2;
    EXPECT_TRUE(gVersionConverter(&v2, xml));
    EXPECT_EQ(v, v2);
}

TEST_F(LibVintfTest, HalImplementationConverter) {
    HalImplementation hl{ImplLevel::SOC, "msm8992"};
    std::string xml = gHalImplementationConverter(hl);
    EXPECT_EQ(xml, "<impl level=\"soc\">msm8992</impl>\n");
    HalImplementation hl2;
    EXPECT_TRUE(gHalImplementationConverter(&hl2, xml));
    EXPECT_EQ(hl.impl, hl2.impl);
    EXPECT_EQ(hl.implLevel, hl2.implLevel);
}

TEST_F(LibVintfTest, MatrixHalConverter) {
    MatrixHal mh{HalFormat::NATIVE, "android.hardware.camera",
            {{VersionRange(1,2,3), VersionRange(4,5,6)}},
            false /* optional */};
    std::string xml = gMatrixHalConverter(mh);
    EXPECT_EQ(xml,
        "<hal format=\"native\" optional=\"false\">\n"
        "    <name>android.hardware.camera</name>\n"
        "    <version>1.2-3</version>\n"
        "    <version>4.5-6</version>\n"
        "</hal>\n");
    MatrixHal mh2;
    EXPECT_TRUE(gMatrixHalConverter(&mh2, xml));
    EXPECT_EQ(mh, mh2);
}

TEST_F(LibVintfTest, CompatibilityMatrixCoverter) {
    CompatibilityMatrix cm;
    EXPECT_TRUE(add(cm, MatrixHal{HalFormat::NATIVE, "android.hardware.camera",
            {{VersionRange(1,2,3), VersionRange(4,5,6)}},
            false /* optional */}));
    EXPECT_TRUE(add(cm, MatrixHal{HalFormat::NATIVE, "android.hardware.nfc",
            {{VersionRange(4,5,6), VersionRange(10,11,12)}},
            true /* optional */}));
    EXPECT_TRUE(add(cm, MatrixKernel{Version(3, 18),
            {{{"CONFIG_FOO"}, {"CONFIG_BAR"}}}}));
    EXPECT_TRUE(add(cm, MatrixKernel{Version(4, 4),
            {{{"CONFIG_BAZ"}, {"CONFIG_BAR"}}}}));
    std::string xml = gCompatibilityMatrixConverter(cm);
    EXPECT_EQ(xml,
        "<compatibility-matrix version=\"1.0\">\n"
        "    <hal format=\"native\" optional=\"false\">\n"
        "        <name>android.hardware.camera</name>\n"
        "        <version>1.2-3</version>\n"
        "        <version>4.5-6</version>\n"
        "    </hal>\n"
        "    <hal format=\"native\" optional=\"true\">\n"
        "        <name>android.hardware.nfc</name>\n"
        "        <version>4.5-6</version>\n"
        "        <version>10.11-12</version>\n"
        "    </hal>\n"
        "    <kernel version=\"3.18\">\n"
        "        <config>CONFIG_FOO</config>\n"
        "        <config>CONFIG_BAR</config>\n"
        "    </kernel>\n"
        "    <kernel version=\"4.4\">\n"
        "        <config>CONFIG_BAZ</config>\n"
        "        <config>CONFIG_BAR</config>\n"
        "    </kernel>\n"
        "    <sepolicy/>\n"
        "</compatibility-matrix>\n");
    CompatibilityMatrix cm2;
    EXPECT_TRUE(gCompatibilityMatrixConverter(&cm2, xml));
    EXPECT_TRUE(isEqual(cm, cm2));
}

TEST_F(LibVintfTest, IsValid) {
    EXPECT_TRUE(isValid(ManifestHal()));

    ManifestHal invalidHal = ManifestHal::hal("android.hardware.camera", ImplLevel::SOC, "msm8892",
            {{Version(2,0), Version(2,1)}}, Transport::PASSTHROUGH);
    EXPECT_FALSE(isValid(invalidHal));
    VendorManifest vm2;
    EXPECT_FALSE(add(vm2, std::move(invalidHal)));
}

TEST_F(LibVintfTest, VendorManifestGetHal) {
    VendorManifest vm = testVendorManifest();
    EXPECT_NE(getHal(vm, "android.hardware.camera"), nullptr);
    EXPECT_EQ(getHal(vm, "non-existent"), nullptr);

    std::vector<std::string> arr{"android.hardware.camera", "android.hardware.nfc"};
    size_t i = 0;
    for (const auto &hal : getHals(vm)) {
        EXPECT_EQ(hal.name, arr[i++]);
    }
}

} // namespace vintf
} // namespace android

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
