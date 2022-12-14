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

#include <sap_hidl_hal_utils.h>

bool isServiceValidForDeviceConfiguration(hidl_string& serviceName) {
    if (isSsSsEnabled()) {
        // Device is configured as SSSS.
        if (serviceName != SAP_SERVICE_SLOT1_NAME) {
            LOG(DEBUG) << "Not valid for SSSS device.";
            return false;
        }
    } else if (isDsDsEnabled()) {
        // Device is configured as DSDS.
        if (serviceName != SAP_SERVICE_SLOT1_NAME && serviceName != SAP_SERVICE_SLOT2_NAME) {
            LOG(DEBUG) << "Not valid for DSDS device.";
            return false;
        }
    } else if (isTsTsEnabled()) {
        // Device is configured as TSTS.
        if (serviceName != SAP_SERVICE_SLOT1_NAME && serviceName != SAP_SERVICE_SLOT2_NAME &&
            serviceName != SAP_SERVICE_SLOT3_NAME) {
            LOG(DEBUG) << "Not valid for TSTS device.";
            return false;
        }
    }
    return true;
}

void SapHidlTest::SetUp() {
    hidl_string serviceName = GetParam();
    if (!isServiceValidForDeviceConfiguration(serviceName)) {
        LOG(DEBUG) << "Skipped the test due to device configuration.";
        GTEST_SKIP();
    }
    sap = ISap::getService(serviceName);
    ASSERT_NE(sap, nullptr);

    sapCb = new SapCallback(*this);
    ASSERT_NE(sapCb, nullptr);

    count = 0;

    sap->setCallback(sapCb);
}

void SapHidlTest::TearDown() {}

void SapHidlTest::notify(int receivedToken) {
    std::unique_lock<std::mutex> lock(mtx);
    count++;
    if (token == receivedToken) {
        cv.notify_one();
    }
}

std::cv_status SapHidlTest::wait() {
    std::unique_lock<std::mutex> lock(mtx);

    std::cv_status status = std::cv_status::no_timeout;
    auto now = std::chrono::system_clock::now();
    while (count == 0) {
        status = cv.wait_until(lock, now + std::chrono::seconds(TIMEOUT_PERIOD));
        if (status == std::cv_status::timeout) {
            return status;
        }
    }
    count--;
    return status;
}
