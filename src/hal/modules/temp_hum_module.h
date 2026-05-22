#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "../module.h"

// TempHumModule - AHT10 temperature and humidity sensor over I2C.
class TempHumModule : public Module {
public:
    static constexpr uint8_t SDA_PIN = 22;
    static constexpr uint8_t SCL_PIN = 27;
    static constexpr uint8_t I2C_ADDR = 0x38;
    static constexpr uint32_t MIN_INTERVAL_MS = 2000;
    static constexpr uint32_t CONVERSION_MS = 80;

    void init() override {
        Wire.begin(SDA_PIN, SCL_PIN);
        Wire.setClock(100000);

        delay(40);

        // Soft reset.
        Wire.beginTransmission(I2C_ADDR);
        Wire.write(0xBA);
        Wire.endTransmission();
        delay(20);

        // Calibration command: 0xE1, 0x08, 0x00.
        Wire.beginTransmission(I2C_ADDR);
        Wire.write(0xE1);
        Wire.write(0x08);
        Wire.write(0x00);
        uint8_t err = Wire.endTransmission();
        delay(10);

        _connected = (err == 0) && _isCalibrated();
        _state = State::IDLE;
        _stateSinceMs = millis();
        _lastTriggerMs = 0;
        _temperatureC = NAN;
        _humidity = NAN;
    }

    void update() override {
        uint32_t now = millis();
        switch (_state) {
            case State::IDLE:
                if (_lastTriggerMs == 0 || (now - _lastTriggerMs) >= MIN_INTERVAL_MS) {
                    _triggerMeasurement(now);
                }
                break;
            case State::WAITING:
                if ((now - _stateSinceMs) >= CONVERSION_MS) {
                    _readResult();
                    _state = State::IDLE;
                    _stateSinceMs = now;
                }
                break;
        }
    }

    const char* name() const override { return "Temperature & Humidity"; }
    ModuleType type() const override { return ModuleType::TEMP_HUMIDITY; }
    ModuleConnector connector() const override { return ModuleConnector::CN1; }
    bool isConnected() const override { return _connected; }

    float temperatureC() const { return _temperatureC; }
    float humidity() const { return _humidity; }

private:
    enum class State : uint8_t { IDLE, WAITING };

    State _state = State::IDLE;
    uint32_t _stateSinceMs = 0;
    uint32_t _lastTriggerMs = 0;
    bool _connected = false;
    float    _temperatureC = NAN;
    float _humidity = NAN;

    bool _isCalibrated() {
        Wire.requestFrom((uint8_t)I2C_ADDR, (uint8_t)1);
        if (Wire.available() < 1) return false;
        uint8_t status = Wire.read();
        return (status & 0x08) != 0;
    }

    void _triggerMeasurement(uint32_t now) {
        Wire.beginTransmission(I2C_ADDR);
        Wire.write(0xAC);
        Wire.write(0x33);
        Wire.write(0x00);
        uint8_t err = Wire.endTransmission();
        if (err != 0) {
            _connected = false;
            _lastTriggerMs = now;
            return;
        }
        _lastTriggerMs = now;
        _stateSinceMs = now;
        _state = State::WAITING;
    }

    void _readResult() {
        uint8_t n = Wire.requestFrom((uint8_t)I2C_ADDR, (uint8_t)6);
        if (n < 6) {
            _connected = false;
            return;
        }
        uint8_t b[6];
        for (int i = 0; i < 6; i++) b[i] = Wire.read();

        // b[0] is status. Bit 7 = busy; should be 0 by now.
        if (b[0] & 0x80) {
            _connected = false;
            return;
        }

        uint32_t rawHum = ((uint32_t)b[1] << 12) | ((uint32_t)b[2] << 4) | (b[3] >> 4);
        uint32_t rawTemp = (((uint32_t)b[3] & 0x0F) << 16) | ((uint32_t)b[4] << 8) | b[5];

        _humidity = (rawHum * 100.0f) / 1048576.0f;
        _temperatureC = (rawTemp * 200.0f) / 1048576.0f - 50.0f;
        _connected = true;
    }
};
