/*
 * Copyright (C) 2021 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/light/BnLights.h>
#include <unordered_map>
#include "samsung_lights.h"

using ::aidl::android::hardware::light::HwLightState;
using ::aidl::android::hardware::light::HwLight;

static unsigned int brightness_table[256] = {
    0,   15,  28,  38,  48,  56,  61,  68,  73,  79,  84,  86,  91,  96,  99,
    104, 107, 109, 114, 117, 119, 124, 127, 130, 132, 135, 137, 140, 142, 145,
    145, 145, 145, 153, 153, 155, 158, 158, 160, 160, 163, 163, 165, 165, 168,
    168, 170, 170, 173, 173, 175, 175, 178, 178, 178, 181, 181, 181, 183, 183,
    183, 186, 186, 186, 188, 188, 188, 191, 191, 191, 191, 193, 193, 193, 193,
    196, 196, 198, 198, 198, 198, 198, 201, 201, 201, 204, 204, 204, 204, 204,
    206, 206, 206, 206, 206, 209, 209, 209, 209, 209, 211, 211, 211, 211, 211,
    214, 214, 214, 214, 214, 216, 216, 216, 216, 216, 216, 216, 216, 219, 219,
    219, 219, 221, 221, 221, 221, 221, 221, 224, 224, 224, 224, 224, 224, 224,
    224, 224, 224, 226, 226, 226, 226, 226, 226, 226, 229, 229, 229, 229, 229,
    229, 229, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 234, 234, 234,
    234, 234, 234, 234, 234, 234, 237, 237, 237, 237, 237, 237, 237, 237, 237,
    239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 242, 242, 242, 242, 242,
    242, 242, 242, 242, 242, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244,
    244, 244, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 249, 249,
    249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 252, 252, 252, 252, 252,
    252, 252, 252, 252, 252, 252, 252, 252, 255, 255, 255, 255, 255, 255, 255,
    255
};

namespace aidl {
namespace android {
namespace hardware {
namespace light {

class Lights : public BnLights {
public:
    Lights();

    ndk::ScopedAStatus setLightState(int32_t id, const HwLightState& state) override;
    ndk::ScopedAStatus getLights(std::vector<HwLight> *_aidl_return) override;

private:
    void handleBacklight(const HwLightState& state);
#ifdef BUTTON_BRIGHTNESS_NODE
    void handleButtons(const HwLightState& state);
#endif /* BUTTON_BRIGHTNESS_NODE */
#ifdef LED_BLINK_NODE
    void handleBattery(const HwLightState& state);
    void handleNotifications(const HwLightState& state);
    void handleAttention(const HwLightState& state);
    void setNotificationLED();
    uint32_t calibrateColor(uint32_t color, int32_t brightness);

    HwLightState mAttentionState;
    HwLightState mBatteryState;
    HwLightState mNotificationState;
#endif /* LED_BLINK_NODE */

    uint32_t rgbToBrightness(const HwLightState& state);

    std::mutex mLock;
    std::unordered_map<LightType, std::function<void(const HwLightState&)>> mLights;
};

} // namespace light
} // namespace hardware
} // namespace android
} // namespace aidl
