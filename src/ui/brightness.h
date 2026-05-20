#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "../storage/config_store.h"

// Backlight brightness control via LEDC PWM on TFT_BL.
// Value is 0..255 and mirrors the "brightness" key in config.txt.
namespace Brightness {
    static constexpr uint8_t LEDC_CHANNEL = 0;
    static constexpr uint8_t LEDC_RESOLUTION = 8;
    static constexpr uint32_t LEDC_FREQ_HZ = 5000;
    static constexpr uint8_t DEFAULT_VALUE = 255;
    static constexpr uint8_t MIN_VALUE = 8;

    inline uint8_t gValue = DEFAULT_VALUE;

    inline void apply(uint8_t value) {
        gValue = value;
        uint8_t duty = value < MIN_VALUE ? MIN_VALUE : value;
        ledcWrite(LEDC_CHANNEL, duty);
    }

    inline void init() {
        ledcSetup(LEDC_CHANNEL, LEDC_FREQ_HZ, LEDC_RESOLUTION);
        ledcAttachPin(TFT_BL, LEDC_CHANNEL);
        apply(DEFAULT_VALUE);
    }

    inline void load() {
        String v = ConfigStore::getInstance().get("brightness", "255");
        int n = v.toInt();
        if (n < 0) n = 0;
        if (n > 255) n = 255;
        apply((uint8_t)n);
    }

    inline void save(uint8_t value) {
        apply(value);
        ConfigStore::getInstance().set("brightness", String((int)value));
    }

    inline uint8_t value() { return gValue; }
}
