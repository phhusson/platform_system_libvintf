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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdio.h>
#include <unistd.h>

#include "utils-fake.h"
#include "vintf/VintfObject.h"

using namespace ::testing;
using namespace ::android::vintf;
using namespace ::android::vintf::details;

//
// Set of Xml1 metadata compatible with each other.
//

const std::string systemMatrixXml1 =
    "<compatibility-matrix version=\"1.0\" type=\"framework\">\n"
    "    <hal format=\"hidl\" optional=\"false\">\n"
    "        <name>android.hardware.camera</name>\n"
    "        <version>2.0-5</version>\n"
    "        <version>3.4-16</version>\n"
    "    </hal>\n"
    "    <hal format=\"hidl\" optional=\"false\">\n"
    "        <name>android.hardware.nfc</name>\n"
    "        <version>1.0</version>\n"
    "        <version>2.0</version>\n"
    "    </hal>\n"
    "    <hal format=\"hidl\" optional=\"true\">\n"
    "        <name>android.hardware.foo</name>\n"
    "        <version>1.0</version>\n"
    "    </hal>\n"
    "    <kernel version=\"3.18.31\"></kernel>\n"
    "    <sepolicy>\n"
    "        <kernel-sepolicy-version>30</kernel-sepolicy-version>\n"
    "        <sepolicy-version>25.5</sepolicy-version>\n"
    "        <sepolicy-version>26.0-3</sepolicy-version>\n"
    "    </sepolicy>\n"
    "    <avb>\n"
    "        <vbmeta-version>0.0</vbmeta-version>\n"
    "    </avb>\n"
    "</compatibility-matrix>\n";

const std::string vendorManifestXml1 =
    "<manifest version=\"1.0\" type=\"device\">\n"
    "    <hal format=\"hidl\">\n"
    "        <name>android.hardware.camera</name>\n"
    "        <transport>hwbinder</transport>\n"
    "        <version>3.5</version>\n"
    "        <interface>\n"
    "            <name>IBetterCamera</name>\n"
    "            <instance>camera</instance>\n"
    "        </interface>\n"
    "        <interface>\n"
    "            <name>ICamera</name>\n"
    "            <instance>default</instance>\n"
    "            <instance>legacy/0</instance>\n"
    "        </interface>\n"
    "    </hal>\n"
    "    <hal format=\"hidl\">\n"
    "        <name>android.hardware.nfc</name>\n"
    "        <transport>hwbinder</transport>\n"
    "        <version>1.0</version>\n"
    "        <version>2.0</version>\n"
    "        <interface>\n"
    "            <name>INfc</name>\n"
    "            <instance>nfc_nci</instance>\n"
    "        </interface>\n"
    "    </hal>\n"
    "    <hal format=\"hidl\">\n"
    "        <name>android.hardware.nfc</name>\n"
    "        <transport>hwbinder</transport>\n"
    "        <version>2.0</version>\n"
    "        <interface>\n"
    "            <name>INfc</name>\n"
    "            <instance>default</instance>\n"
    "        </interface>\n"
    "    </hal>\n"
    "    <sepolicy>\n"
    "        <version>25.5</version>\n"
    "    </sepolicy>\n"
    "</manifest>\n";

const std::string systemManifestXml1 =
    "<manifest version=\"1.0\" type=\"framework\">\n"
    "    <hal format=\"hidl\">\n"
    "        <name>android.hidl.manager</name>\n"
    "        <transport>hwbinder</transport>\n"
    "        <version>1.0</version>\n"
    "        <interface>\n"
    "            <name>IServiceManager</name>\n"
    "            <instance>default</instance>\n"
    "        </interface>\n"
    "    </hal>\n"
    "    <vndk>\n"
    "        <version>25.0.5</version>\n"
    "        <library>libbase.so</library>\n"
    "        <library>libjpeg.so</library>\n"
    "    </vndk>\n"
    "</manifest>\n";

const std::string vendorMatrixXml1 =
    "<compatibility-matrix version=\"1.0\" type=\"device\">\n"
    "    <hal format=\"hidl\" optional=\"false\">\n"
    "        <name>android.hidl.manager</name>\n"
    "        <version>1.0</version>\n"
    "    </hal>\n"
    "    <vndk>\n"
    "        <version>25.0.1-5</version>\n"
    "        <library>libbase.so</library>\n"
    "        <library>libjpeg.so</library>\n"
    "    </vndk>\n"
    "</compatibility-matrix>\n";

//
// Set of Xml2 metadata compatible with each other.
//

const std::string systemMatrixXml2 =
    "<compatibility-matrix version=\"1.0\" type=\"framework\">\n"
    "    <hal format=\"hidl\">\n"
    "        <name>android.hardware.foo</name>\n"
    "        <version>1.0</version>\n"
    "    </hal>\n"
    "    <kernel version=\"3.18.31\"></kernel>\n"
    "    <sepolicy>\n"
    "        <kernel-sepolicy-version>30</kernel-sepolicy-version>\n"
    "        <sepolicy-version>25.5</sepolicy-version>\n"
    "        <sepolicy-version>26.0-3</sepolicy-version>\n"
    "    </sepolicy>\n"
    "    <avb>\n"
    "        <vbmeta-version>0.0</vbmeta-version>\n"
    "    </avb>\n"
    "</compatibility-matrix>\n";

const std::string vendorManifestXml2 =
    "<manifest version=\"1.0\" type=\"device\">"
    "    <hal>"
    "        <name>android.hardware.foo</name>"
    "        <transport>hwbinder</transport>"
    "        <version>1.0</version>"
    "    </hal>"
    "    <sepolicy>\n"
    "        <version>25.5</version>\n"
    "    </sepolicy>\n"
    "</manifest>";

// Setup the MockFileFetcher used by the fetchAllInformation template
// so it returns the given metadata info instead of fetching from device.
void setupMockFetcher(const std::string& vendorManifestXml, const std::string& systemMatrixXml,
                      const std::string& systemManifestXml, const std::string& vendorMatrixXml) {
    MockFileFetcher* fetcher = static_cast<MockFileFetcher*>(gFetcher);

    ON_CALL(*fetcher, fetch(StrEq("/vendor/manifest.xml"), _))
        .WillByDefault(Invoke([vendorManifestXml](const std::string& path, std::string& fetched) {
            (void)path;
            fetched = vendorManifestXml;
            return 0;
        }));
    ON_CALL(*fetcher, fetch(StrEq("/system/manifest.xml"), _))
        .WillByDefault(Invoke([systemManifestXml](const std::string& path, std::string& fetched) {
            (void)path;
            fetched = systemManifestXml;
            return 0;
        }));
    ON_CALL(*fetcher, fetch(StrEq("/vendor/compatibility_matrix.xml"), _))
        .WillByDefault(Invoke([vendorMatrixXml](const std::string& path, std::string& fetched) {
            (void)path;
            fetched = vendorMatrixXml;
            return 0;
        }));
    ON_CALL(*fetcher, fetch(StrEq("/system/compatibility_matrix.xml"), _))
        .WillByDefault(Invoke([systemMatrixXml](const std::string& path, std::string& fetched) {
            (void)path;
            fetched = systemMatrixXml;
            return 0;
        }));
}

// Test fixture that provides compatible metadata from the mock device.
class VintfObjectCompatibleTest : public testing::Test {
   protected:
    virtual void SetUp() {
        setupMockFetcher(vendorManifestXml1, systemMatrixXml1, systemManifestXml1,
                         vendorMatrixXml1);
    }
};

// Tests that local info is checked.
TEST_F(VintfObjectCompatibleTest, TestDeviceCompatibility) {
    std::string error;
    std::vector<std::string> packageInfo;

    int result = VintfObject::CheckCompatibility(packageInfo, &error);

    ASSERT_EQ(result, 0) << "Fail message:" << error.c_str();
    // Check that nothing was ignored.
    ASSERT_STREQ(error.c_str(), "");
}

// Tests that input info is checked against device and passes.
TEST_F(VintfObjectCompatibleTest, TestInputVsDeviceSuccess) {
    std::string error;
    std::vector<std::string> packageInfo = {systemMatrixXml1};

    int result = VintfObject::CheckCompatibility(packageInfo, &error);

    ASSERT_EQ(result, 0) << "Fail message:" << error.c_str();
    ASSERT_STREQ(error.c_str(), "");
}

// Tests that input info is checked against device and fails.
TEST_F(VintfObjectCompatibleTest, TestInputVsDeviceFail) {
    std::string error;
    std::vector<std::string> packageInfo = {systemMatrixXml2};

    int result = VintfObject::CheckCompatibility(packageInfo, &error);

    ASSERT_EQ(result, 1) << "Should have failed:" << error.c_str();
    ASSERT_STREQ(error.c_str(),
                 "Device manifest and framework compatibility matrix are incompatible: HALs "
                 "incompatible. android.hardware.foo");
}

// Tests that complementary info is checked against itself.
TEST_F(VintfObjectCompatibleTest, TestInputSuccess) {
    std::string error;
    std::vector<std::string> packageInfo = {systemMatrixXml2, vendorManifestXml2};

    int result = VintfObject::CheckCompatibility(packageInfo, &error);

    ASSERT_EQ(result, 0) << "Failed message:" << error.c_str();
    ASSERT_STREQ(error.c_str(), "");
}

// Test fixture that provides incompatible metadata from the mock device.
class VintfObjectIncompatibleTest : public testing::Test {
   protected:
    virtual void SetUp() {
        setupMockFetcher(vendorManifestXml1, systemMatrixXml2, systemManifestXml1,
                         vendorMatrixXml1);
    }
};

// Fetch all metadata from device and ensure that it fails.
TEST_F(VintfObjectIncompatibleTest, TestDeviceCompatibility) {
    std::string error;
    std::vector<std::string> packageInfo;

    MockFileFetcher* fetcher = static_cast<MockFileFetcher*>(gFetcher);
    EXPECT_CALL(*fetcher, fetch(StrEq("/vendor/manifest.xml"), _));
    EXPECT_CALL(*fetcher, fetch(StrEq("/system/manifest.xml"), _));
    EXPECT_CALL(*fetcher, fetch(StrEq("/vendor/compatibility_matrix.xml"), _));
    EXPECT_CALL(*fetcher, fetch(StrEq("/system/compatibility_matrix.xml"), _));

    int result = VintfObject::CheckCompatibility(packageInfo, &error);

    ASSERT_EQ(result, 1) << "Should have failed:" << error.c_str();
}

// Pass in new metadata that fixes the incompatibility.
TEST_F(VintfObjectIncompatibleTest, TestInputVsDeviceSuccess) {
    std::string error;
    std::vector<std::string> packageInfo = {systemMatrixXml1};

    MockFileFetcher* fetcher = static_cast<MockFileFetcher*>(gFetcher);
    EXPECT_CALL(*fetcher, fetch(StrEq("/vendor/manifest.xml"), _));
    EXPECT_CALL(*fetcher, fetch(StrEq("/system/manifest.xml"), _));
    EXPECT_CALL(*fetcher, fetch(StrEq("/vendor/compatibility_matrix.xml"), _));
    EXPECT_CALL(*fetcher, fetch(StrEq("/system/compatibility_matrix.xml"), _)).Times(0);

    int result = VintfObject::CheckCompatibility(packageInfo, &error);

    ASSERT_EQ(result, 0) << "Failed message:" << error.c_str();
    ASSERT_STREQ(error.c_str(), "");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleMock(&argc, argv);

    NiceMock<MockFileFetcher> fetcher;
    gFetcher = static_cast<FileFetcher*>(&fetcher);

    return RUN_ALL_TESTS();
}
