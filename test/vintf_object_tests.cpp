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

#include <android-base/logging.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdio.h>
#include <unistd.h>

#ifdef LIBVINTF_TARGET
#include <android-base/properties.h>
#endif

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
    "            <instance>nfc_nci</instance>\n"
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
                      const std::string& systemManifestXml, const std::string& vendorMatrixXml,
                      const std::string& productModel) {
    MockFileFetcher* fetcher = static_cast<MockFileFetcher*>(gFetcher);

    if (!productModel.empty()) {
        ON_CALL(*fetcher, fetch(StrEq("/odm/etc/manifest_" + productModel + ".xml"), _))
            .WillByDefault(Return(::android::NAME_NOT_FOUND));
    }
    ON_CALL(*fetcher, fetch(StrEq("/odm/etc/manifest.xml"), _))
        .WillByDefault(Return(::android::NAME_NOT_FOUND));
    ON_CALL(*fetcher, fetch(StrEq("/vendor/etc/manifest.xml"), _))
        .WillByDefault(Return(::android::NAME_NOT_FOUND));
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
    // Don't list /system/etc/vintf unless otherwise specified.
    ON_CALL(*fetcher, listFiles(StrEq("/system/etc/vintf/"), _, _))
        .WillByDefault(Return(::android::OK));
}

static MockPartitionMounter &mounter() {
    return *static_cast<MockPartitionMounter *>(gPartitionMounter);
}
static MockFileFetcher &fetcher() {
    return *static_cast<MockFileFetcher*>(gFetcher);
}

class VintfObjectTestBase : public testing::Test {
   protected:
    virtual void SetUp() {
#ifdef LIBVINTF_TARGET
        productModel = android::base::GetProperty("ro.boot.product.hardware.sku", "");
#endif
    }
    virtual void TearDown() {
        Mock::VerifyAndClear(&mounter());
        Mock::VerifyAndClear(&fetcher());
    }

    void expectVendorManifest(size_t times = 1) {
        EXPECT_CALL(fetcher(), fetch(StrEq("/vendor/etc/manifest.xml"), _)).Times(times);
        if (!productModel.empty()) {
            EXPECT_CALL(fetcher(), fetch(StrEq("/odm/etc/manifest_" + productModel + ".xml"), _))
                .Times(times);
        }
        EXPECT_CALL(fetcher(), fetch(StrEq("/odm/etc/manifest.xml"), _)).Times(times);
        EXPECT_CALL(fetcher(), fetch(StrEq("/vendor/manifest.xml"), _)).Times(times);
    }

    void expectSystemManifest(size_t times = 1) {
        EXPECT_CALL(fetcher(), fetch(StrEq("/system/manifest.xml"), _)).Times(times);
    }

    void expectVendorMatrix(size_t times = 1) {
        EXPECT_CALL(fetcher(), fetch(StrEq("/vendor/compatibility_matrix.xml"), _)).Times(times);
    }

    void expectSystemMatrix(size_t times = 1) {
        EXPECT_CALL(fetcher(), fetch(StrEq("/system/compatibility_matrix.xml"), _)).Times(times);
    }

    void expectFetch(const std::string& path, const std::string& content = "", size_t times = 1) {
        EXPECT_CALL(fetcher(), fetch(StrEq(path), _))
            .Times(times)
            .WillRepeatedly(Invoke([content](const auto&, auto& out) {
                out = content;
                return content.empty() ? ::android::NAME_NOT_FOUND : ::android::OK;
            }));
    }

    std::string productModel;
};

// Test fixture that provides compatible metadata from the mock device.
class VintfObjectCompatibleTest : public VintfObjectTestBase {
   protected:
    virtual void SetUp() {
        VintfObjectTestBase::SetUp();
        mounter().reset();
        setupMockFetcher(vendorManifestXml1, systemMatrixXml1, systemManifestXml1, vendorMatrixXml1,
                         productModel);
    }
};

// Tests that local info is checked.
TEST_F(VintfObjectCompatibleTest, TestDeviceCompatibility) {
    std::string error;
    std::vector<std::string> packageInfo;

    expectVendorManifest();
    expectSystemManifest();
    expectVendorMatrix();
    expectSystemMatrix();
    EXPECT_CALL(mounter(), mountSystem()).Times(0);
    EXPECT_CALL(mounter(), umountSystem()).Times(0);
    EXPECT_CALL(mounter(), mountVendor()).Times(0);
    EXPECT_CALL(mounter(), umountVendor()).Times(0);

    int result = VintfObject::CheckCompatibility(packageInfo, &error);

    ASSERT_EQ(result, 0) << "Fail message:" << error.c_str();
    // Check that nothing was ignored.
    ASSERT_STREQ(error.c_str(), "");
    EXPECT_FALSE(mounter().systemMounted());
    EXPECT_FALSE(mounter().vendorMounted());
}

TEST_F(VintfObjectCompatibleTest, TestDeviceCompatibilityMount) {
    std::string error;
    std::vector<std::string> packageInfo;

    EXPECT_CALL(mounter(), mountSystem()).Times(2);
    EXPECT_CALL(mounter(), umountSystem()).Times(1); // Should only umount once
    EXPECT_CALL(mounter(), mountVendor()).Times(2);
    EXPECT_CALL(mounter(), umountVendor()).Times(1);

    int result = details::checkCompatibility(packageInfo, true /* mount */, mounter(), &error);

    ASSERT_EQ(result, 0) << "Fail message:" << error.c_str();
    EXPECT_FALSE(mounter().systemMounted());
    EXPECT_FALSE(mounter().vendorMounted());
}

// Tests that input info is checked against device and passes.
TEST_F(VintfObjectCompatibleTest, TestInputVsDeviceSuccess) {
    std::string error;
    std::vector<std::string> packageInfo = {systemMatrixXml1};

    expectVendorManifest();
    expectSystemManifest();
    expectVendorMatrix();
    expectSystemMatrix(0);
    EXPECT_CALL(mounter(), mountSystem()).Times(0);
    EXPECT_CALL(mounter(), umountSystem()).Times(0);
    EXPECT_CALL(mounter(), mountVendor()).Times(0);
    EXPECT_CALL(mounter(), umountVendor()).Times(0);

    int result = VintfObject::CheckCompatibility(packageInfo, &error);

    ASSERT_EQ(result, 0) << "Fail message:" << error.c_str();
    ASSERT_STREQ(error.c_str(), "");
    EXPECT_FALSE(mounter().systemMounted());
    EXPECT_FALSE(mounter().vendorMounted());
}

TEST_F(VintfObjectCompatibleTest, TestInputVsDeviceSuccessMount) {
    std::string error;
    std::vector<std::string> packageInfo = {systemMatrixXml1};

    EXPECT_CALL(mounter(), mountSystem()).Times(1); // Should only mount once for manifest
    EXPECT_CALL(mounter(), umountSystem()).Times(1);
    EXPECT_CALL(mounter(), mountVendor()).Times(2);
    EXPECT_CALL(mounter(), umountVendor()).Times(1);

    int result = details::checkCompatibility(packageInfo, true /* mount */, mounter(), &error);

    ASSERT_EQ(result, 0) << "Fail message:" << error.c_str();
    EXPECT_FALSE(mounter().systemMounted());
    EXPECT_FALSE(mounter().vendorMounted());
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

TEST_F(VintfObjectCompatibleTest, TestFrameworkOnlyOta) {
    std::string error;
    std::vector<std::string> packageInfo = {systemMatrixXml1, systemManifestXml1};

    expectVendorManifest();
    expectSystemManifest(0);
    expectVendorMatrix();
    expectSystemMatrix(0);
    EXPECT_CALL(mounter(), mountSystem()).Times(0);
    EXPECT_CALL(mounter(), umountSystem()).Times(0);
    EXPECT_CALL(mounter(), mountVendor()).Times(0);
    EXPECT_CALL(mounter(), umountVendor()).Times(0);

    int result = VintfObject::CheckCompatibility(packageInfo, &error);

    ASSERT_EQ(result, 0) << "Fail message:" << error.c_str();
    ASSERT_STREQ(error.c_str(), "");
    EXPECT_FALSE(mounter().systemMounted());
    EXPECT_FALSE(mounter().vendorMounted());
}

TEST_F(VintfObjectCompatibleTest, TestFrameworkOnlyOtaMount) {
    std::string error;
    std::vector<std::string> packageInfo = {systemMatrixXml1, systemManifestXml1};

    EXPECT_CALL(mounter(), mountSystem()).Times(0);
    EXPECT_CALL(mounter(), umountSystem()).Times(1);
    EXPECT_CALL(mounter(), mountVendor()).Times(2);
    EXPECT_CALL(mounter(), umountVendor()).Times(1);

    int result = details::checkCompatibility(packageInfo, true /* mount */, mounter(), &error);

    ASSERT_EQ(result, 0) << "Fail message:" << error.c_str();
    EXPECT_FALSE(mounter().systemMounted());
    EXPECT_FALSE(mounter().vendorMounted());
}

TEST_F(VintfObjectCompatibleTest, TestFullOta) {
    std::string error;
    std::vector<std::string> packageInfo = {systemMatrixXml1, systemManifestXml1,
            vendorMatrixXml1, vendorManifestXml1};

    expectVendorManifest(0);
    expectSystemManifest(0);
    expectVendorMatrix(0);
    expectSystemMatrix(0);
    EXPECT_CALL(mounter(), mountSystem()).Times(0);
    EXPECT_CALL(mounter(), umountSystem()).Times(0);
    EXPECT_CALL(mounter(), mountVendor()).Times(0);
    EXPECT_CALL(mounter(), umountVendor()).Times(0);

    int result = VintfObject::CheckCompatibility(packageInfo, &error);

    ASSERT_EQ(result, 0) << "Fail message:" << error.c_str();
    ASSERT_STREQ(error.c_str(), "");
    EXPECT_FALSE(mounter().systemMounted());
    EXPECT_FALSE(mounter().vendorMounted());
}

TEST_F(VintfObjectCompatibleTest, TestFullOnlyOtaMount) {
    std::string error;
    std::vector<std::string> packageInfo = {systemMatrixXml1, systemManifestXml1,
            vendorMatrixXml1, vendorManifestXml1};

    EXPECT_CALL(mounter(), mountSystem()).Times(0);
    EXPECT_CALL(mounter(), umountSystem()).Times(1);
    EXPECT_CALL(mounter(), mountVendor()).Times(0);
    EXPECT_CALL(mounter(), umountVendor()).Times(1);

    int result = details::checkCompatibility(packageInfo, true /* mount */, mounter(), &error);

    ASSERT_EQ(result, 0) << "Fail message:" << error.c_str();
    EXPECT_FALSE(mounter().systemMounted());
    EXPECT_FALSE(mounter().vendorMounted());
}

// Test fixture that provides incompatible metadata from the mock device.
class VintfObjectIncompatibleTest : public VintfObjectTestBase {
   protected:
    virtual void SetUp() {
        VintfObjectTestBase::SetUp();
        mounter().reset();
        setupMockFetcher(vendorManifestXml1, systemMatrixXml2, systemManifestXml1, vendorMatrixXml1,
                         productModel);
    }
};

// Fetch all metadata from device and ensure that it fails.
TEST_F(VintfObjectIncompatibleTest, TestDeviceCompatibility) {
    std::string error;
    std::vector<std::string> packageInfo;

    expectVendorManifest();
    expectSystemManifest();
    expectVendorMatrix();
    expectSystemMatrix();

    int result = VintfObject::CheckCompatibility(packageInfo, &error);

    ASSERT_EQ(result, 1) << "Should have failed:" << error.c_str();
}

// Pass in new metadata that fixes the incompatibility.
TEST_F(VintfObjectIncompatibleTest, TestInputVsDeviceSuccess) {
    std::string error;
    std::vector<std::string> packageInfo = {systemMatrixXml1};

    expectVendorManifest();
    expectSystemManifest();
    expectVendorMatrix();
    expectSystemMatrix(0);

    int result = VintfObject::CheckCompatibility(packageInfo, &error);

    ASSERT_EQ(result, 0) << "Failed message:" << error.c_str();
    ASSERT_STREQ(error.c_str(), "");
}

static MockRuntimeInfoFactory& runtimeInfoFactory() {
    return *static_cast<MockRuntimeInfoFactory*>(gRuntimeInfoFactory);
}

// Test fixture that provides compatible metadata from the mock device.
class VintfObjectRuntimeInfoTest : public testing::Test {
   protected:
    virtual void SetUp() {
        // clear fetch flags
        runtimeInfoFactory().getInfo()->failNextFetch();
        VintfObject::GetRuntimeInfo(true /* skipCache */, RuntimeInfo::FetchFlag::ALL);
    }
    virtual void TearDown() {
        Mock::VerifyAndClear(&runtimeInfoFactory());
        Mock::VerifyAndClear(runtimeInfoFactory().getInfo().get());
    }
};

TEST_F(VintfObjectRuntimeInfoTest, GetRuntimeInfo) {
    InSequence s;

    EXPECT_CALL(*runtimeInfoFactory().getInfo(),
                fetchAllInformation(RuntimeInfo::FetchFlag::CPU_VERSION));
    EXPECT_CALL(*runtimeInfoFactory().getInfo(), fetchAllInformation(RuntimeInfo::FetchFlag::NONE));
    EXPECT_CALL(*runtimeInfoFactory().getInfo(),
                fetchAllInformation(RuntimeInfo::FetchFlag::CPU_VERSION));
    EXPECT_CALL(
        *runtimeInfoFactory().getInfo(),
        fetchAllInformation(RuntimeInfo::FetchFlag::ALL & ~RuntimeInfo::FetchFlag::CPU_VERSION));
    EXPECT_CALL(*runtimeInfoFactory().getInfo(), fetchAllInformation(RuntimeInfo::FetchFlag::ALL));
    EXPECT_CALL(*runtimeInfoFactory().getInfo(), fetchAllInformation(RuntimeInfo::FetchFlag::NONE));

    VintfObject::GetRuntimeInfo(false /* skipCache */, RuntimeInfo::FetchFlag::CPU_VERSION);
    VintfObject::GetRuntimeInfo(false /* skipCache */, RuntimeInfo::FetchFlag::CPU_VERSION);
    VintfObject::GetRuntimeInfo(true /* skipCache */, RuntimeInfo::FetchFlag::CPU_VERSION);
    VintfObject::GetRuntimeInfo(false /* skipCache */, RuntimeInfo::FetchFlag::ALL);
    VintfObject::GetRuntimeInfo(true /* skipCache */, RuntimeInfo::FetchFlag::ALL);
    VintfObject::GetRuntimeInfo(false /* skipCache */, RuntimeInfo::FetchFlag::ALL);
}

// Test fixture that provides incompatible metadata from the mock device.
class VintfObjectTest : public VintfObjectTestBase {
   protected:
    virtual void SetUp() {}
};

// Test framework compatibility matrix is combined at runtime
TEST_F(VintfObjectTest, FrameworkCompatibilityMatrixCombine) {
    EXPECT_CALL(fetcher(), listFiles(StrEq("/system/etc/vintf/"), _, _))
        .WillOnce(Invoke([](const auto&, auto* out, auto*) {
            *out = {
                "compatibility_matrix.1.xml",
                "compatibility_matrix.empty.xml",
            };
            return ::android::OK;
        }));
    expectFetch("/system/etc/vintf/compatibility_matrix.1.xml",
                "<compatibility-matrix version=\"1.0\" type=\"framework\" level=\"1\"/>");
    expectFetch("/system/etc/vintf/compatibility_matrix.empty.xml",
                "<compatibility-matrix version=\"1.0\" type=\"framework\"/>");
    expectSystemMatrix(0);

    EXPECT_NE(nullptr, VintfObject::GetFrameworkCompatibilityMatrix(true /* skipCache */));
}

const std::string vendorEtcManifest =
    "<manifest version=\"1.0\" type=\"device\">\n"
    "    <hal format=\"hidl\">\n"
    "        <name>android.hardware.foo</name>\n"
    "        <transport>hwbinder</transport>\n"
    "        <version>1.0</version>\n"
    "        <version>2.0</version>\n"
    "        <interface>\n"
    "            <name>IVendorEtc</name>\n"
    "            <instance>default</instance>\n"
    "        </interface>\n"
    "    </hal>\n"
    "</manifest>\n";

const std::string vendorManifest =
    "<manifest version=\"1.0\" type=\"device\">\n"
    "    <hal format=\"hidl\">\n"
    "        <name>android.hardware.foo</name>\n"
    "        <transport>hwbinder</transport>\n"
    "        <version>1.0</version>\n"
    "        <interface>\n"
    "            <name>IVendor</name>\n"
    "            <instance>default</instance>\n"
    "        </interface>\n"
    "    </hal>\n"
    "</manifest>\n";

const std::string odmProductManifest =
    "<manifest version=\"1.0\" type=\"device\">\n"
    "    <hal format=\"hidl\" override=\"true\">\n"
    "        <name>android.hardware.foo</name>\n"
    "        <transport>hwbinder</transport>\n"
    "        <version>1.1</version>\n"
    "        <interface>\n"
    "            <name>IOdmProduct</name>\n"
    "            <instance>default</instance>\n"
    "        </interface>\n"
    "    </hal>\n"
    "</manifest>\n";

const std::string odmManifest =
    "<manifest version=\"1.0\" type=\"device\">\n"
    "    <hal format=\"hidl\" override=\"true\">\n"
    "        <name>android.hardware.foo</name>\n"
    "        <transport>hwbinder</transport>\n"
    "        <version>1.1</version>\n"
    "        <interface>\n"
    "            <name>IOdm</name>\n"
    "            <instance>default</instance>\n"
    "        </interface>\n"
    "    </hal>\n"
    "</manifest>\n";

bool containsVendorManifest(const std::shared_ptr<const HalManifest>& p) {
    return !p->getInstances("android.hardware.foo", {1, 0}, "IVendor").empty();
}

bool containsVendorEtcManifest(const std::shared_ptr<const HalManifest>& p) {
    return !p->getInstances("android.hardware.foo", {2, 0}, "IVendorEtc").empty();
}

bool vendorEtcManifestOverridden(const std::shared_ptr<const HalManifest>& p) {
    return p->getInstances("android.hardware.foo", {1, 0}, "IVendorEtc").empty();
}

bool containsOdmManifest(const std::shared_ptr<const HalManifest>& p) {
    return !p->getInstances("android.hardware.foo", {1, 1}, "IOdm").empty();
}

bool containsOdmProductManifest(const std::shared_ptr<const HalManifest>& p) {
    return !p->getInstances("android.hardware.foo", {1, 1}, "IOdmProduct").empty();
}

// Test /vendor/etc/manifest.xml + /odm/etc/manifest_{sku}.xml
TEST_F(VintfObjectTest, DeviceManifestCombine1) {
    if (productModel.empty()) return;

    expectFetch("/vendor/etc/manifest.xml", vendorEtcManifest);
    expectFetch("/odm/etc/manifest_" + productModel + ".xml", odmProductManifest);

    auto p = VintfObject::GetDeviceHalManifest(true /* skipCache */);
    ASSERT_NE(nullptr, p);
    EXPECT_FALSE(containsVendorManifest(p));
    EXPECT_TRUE(vendorEtcManifestOverridden(p));
    EXPECT_TRUE(containsVendorEtcManifest(p));
    EXPECT_FALSE(containsOdmManifest(p));
    EXPECT_TRUE(containsOdmProductManifest(p));
}

// Test /vendor/etc/manifest.xml + /odm/etc/manifest.xml
TEST_F(VintfObjectTest, DeviceManifestCombine2) {
    expectFetch("/vendor/etc/manifest.xml", vendorEtcManifest);
    if (!productModel.empty()) {
        expectFetch("/odm/etc/manifest_" + productModel + ".xml");
    }
    expectFetch("/odm/etc/manifest.xml", odmManifest);

    auto p = VintfObject::GetDeviceHalManifest(true /* skipCache */);
    ASSERT_NE(nullptr, p);
    EXPECT_FALSE(containsVendorManifest(p));
    EXPECT_TRUE(vendorEtcManifestOverridden(p));
    EXPECT_TRUE(containsVendorEtcManifest(p));
    EXPECT_TRUE(containsOdmManifest(p));
    EXPECT_FALSE(containsOdmProductManifest(p));
}

// Test /vendor/etc/manifest.xml
TEST_F(VintfObjectTest, DeviceManifestCombine3) {
    expectFetch("/vendor/etc/manifest.xml", vendorEtcManifest);
    if (!productModel.empty()) {
        expectFetch("/odm/etc/manifest_" + productModel + ".xml");
    }
    expectFetch("/odm/etc/manifest.xml");

    auto p = VintfObject::GetDeviceHalManifest(true /* skipCache */);
    ASSERT_NE(nullptr, p);
    EXPECT_FALSE(containsVendorManifest(p));
    EXPECT_FALSE(vendorEtcManifestOverridden(p));
    EXPECT_TRUE(containsVendorEtcManifest(p));
    EXPECT_FALSE(containsOdmManifest(p));
    EXPECT_FALSE(containsOdmProductManifest(p));
}

// Test /odm/etc/manifest_{sku}.xml
TEST_F(VintfObjectTest, DeviceManifestCombine4) {
    if (productModel.empty()) return;

    expectFetch("/vendor/etc/manifest.xml");
    expectFetch("/odm/etc/manifest_" + productModel + ".xml", odmProductManifest);

    auto p = VintfObject::GetDeviceHalManifest(true /* skipCache */);
    ASSERT_NE(nullptr, p);
    EXPECT_FALSE(containsVendorManifest(p));
    EXPECT_TRUE(vendorEtcManifestOverridden(p));
    EXPECT_FALSE(containsVendorEtcManifest(p));
    EXPECT_FALSE(containsOdmManifest(p));
    EXPECT_TRUE(containsOdmProductManifest(p));
}

// Test /odm/etc/manifest.xml
TEST_F(VintfObjectTest, DeviceManifestCombine5) {
    expectFetch("/vendor/etc/manifest.xml");
    if (!productModel.empty()) {
        expectFetch("/odm/etc/manifest_" + productModel + ".xml");
    }
    expectFetch("/odm/etc/manifest.xml", odmManifest);

    auto p = VintfObject::GetDeviceHalManifest(true /* skipCache */);
    ASSERT_NE(nullptr, p);
    EXPECT_FALSE(containsVendorManifest(p));
    EXPECT_TRUE(vendorEtcManifestOverridden(p));
    EXPECT_FALSE(containsVendorEtcManifest(p));
    EXPECT_TRUE(containsOdmManifest(p));
    EXPECT_FALSE(containsOdmProductManifest(p));
}

// Test /vendor/manifest.xml
TEST_F(VintfObjectTest, DeviceManifestCombine6) {
    expectFetch("/vendor/etc/manifest.xml");
    if (!productModel.empty()) {
        expectFetch("/odm/etc/manifest_" + productModel + ".xml");
    }
    expectFetch("/odm/etc/manifest.xml");
    expectFetch("/vendor/manifest.xml", vendorManifest);

    auto p = VintfObject::GetDeviceHalManifest(true /* skipCache */);
    ASSERT_NE(nullptr, p);
    EXPECT_TRUE(containsVendorManifest(p));
    EXPECT_TRUE(vendorEtcManifestOverridden(p));
    EXPECT_FALSE(containsVendorEtcManifest(p));
    EXPECT_FALSE(containsOdmManifest(p));
    EXPECT_FALSE(containsOdmProductManifest(p));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleMock(&argc, argv);

    NiceMock<MockFileFetcher> fetcher;
    gFetcher = &fetcher;

    NiceMock<MockPartitionMounter> mounter;
    gPartitionMounter = &mounter;

    NiceMock<MockRuntimeInfoFactory> runtimeInfoFactory(
        std::make_shared<NiceMock<MockRuntimeInfo>>());
    gRuntimeInfoFactory = &runtimeInfoFactory;

    return RUN_ALL_TESTS();
}
