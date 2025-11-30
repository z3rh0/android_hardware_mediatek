/*
 * Copyright 2018 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "android.hardware.media.c2-service"

#include <android-base/logging.h>
#include <minijail.h>

#include <util/C2InterfaceHelper.h>
#include <C2Component.h>
#include <C2Config.h>

// HIDL
#include <binder/ProcessState.h>
#include <codec2/hidl/1.2/ComponentStore.h>
#include <hidl/HidlTransportSupport.h>

// AIDL
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <codec2/aidl/ComponentStore.h>
#include <codec2/aidl/ParamTypes.h>

// MTK C2 Store
#include "MTKC2Store.h"

// This is the absolute on-device path of the prebuild_etc module
// "android.hardware.media.c2-mediatek-seccomp_policy" in Android.bp.
static constexpr char kBaseSeccompPolicyPath[] =
        "/vendor/etc/seccomp_policy/"
        "android.hardware.media.c2-mediatek-seccomp-policy";

// Additional seccomp permissions can be added in this file.
// This file does not exist by default.
static constexpr char kExtSeccompPolicyPath[] =
        "/vendor/etc/seccomp_policy/"
        "android.hardware.media.c2-extended-seccomp-policy";

// We want multiple threads to be running so that a blocking operation
// on one codec does not block the other codecs.
// For HIDL: Extra threads may be needed to handle a stacked IPC sequence that
// contains alternating binder and hwbinder calls. (See b/35283480.)
static constexpr int kThreadCount = 8;

void runAidlService() {
    ABinderProcess_setThreadPoolMaxThreadCount(kThreadCount);
    ABinderProcess_startThreadPool();

    // Create IComponentStore service.
    using namespace ::aidl::android::hardware::media::c2;
    std::shared_ptr<IComponentStore> store;

    LOG(DEBUG) << "Instantiating Codec2's IComponentStore service...";
    store = ::ndk::SharedRefBase::make<utils::ComponentStore>(
            android::GetCodec2MtkComponentStore());

    if (store == nullptr) {
        LOG(ERROR) << "Cannot create Codec2's IComponentStore service.";
    } else {
        const std::string serviceName =
            std::string(IComponentStore::descriptor) + "/default";
        binder_exception_t ex = AServiceManager_addService(
                store->asBinder().get(), serviceName.c_str());
        if (ex != EX_NONE) {
            LOG(ERROR) << "Cannot register Codec2's IComponentStore service"
                          " with instance name << \""
                       << serviceName << "\".";
        } else {
            LOG(DEBUG) << "Codec2's IComponentStore service registered. "
                          "Instance name: \"" << serviceName << "\".";
        }
    }

    ABinderProcess_joinThreadPool();
}

void runHidlService() {
    using namespace ::android;

    // Enable vndbinder to allow vendor-to-vendor binder calls.
    ProcessState::initWithDriver("/dev/vndbinder");

    ProcessState::self()->startThreadPool();
    hardware::configureRpcThreadpool(kThreadCount, true /* callerWillJoin */);

    // Create IComponentStore service.
    {
        using namespace ::android::hardware::media::c2::V1_2;
        sp<IComponentStore> store;

        LOG(DEBUG) << "Instantiating Codec2's IComponentStore service...";
        store = new utils::ComponentStore(
                android::GetCodec2MtkComponentStore());

        if (store == nullptr) {
            LOG(ERROR) << "Cannot create Codec2's IComponentStore service.";
        } else {
            constexpr char const* serviceName = "default";
            if (store->registerAsService(serviceName) != OK) {
                LOG(ERROR) << "Cannot register Codec2's IComponentStore service"
                              " with instance name << \""
                           << serviceName << "\".";
            } else {
                LOG(DEBUG) << "Codec2's IComponentStore service registered. "
                              "Instance name: \"" << serviceName << "\".";
            }
        }
    }

    hardware::joinRpcThreadpool();
}

int main(int /* argc */, char** /* argv */) {
    const bool aidlEnabled = ::aidl::android::hardware::media::c2::utils::IsSelected();
    LOG(DEBUG) << "android.hardware.media.c2" << (aidlEnabled ? "-V1" : "@1.2")
               << "-service starting...";

    // Set up minijail to limit system calls.
    signal(SIGPIPE, SIG_IGN);
    android::SetUpMinijail(kBaseSeccompPolicyPath, kExtSeccompPolicyPath);
    if (aidlEnabled) {
        runAidlService();
    } else {
        runHidlService();
    }
    return 0;
}
