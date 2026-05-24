#pragma once
#include <Arduino.h>
#include <lvgl.h>

// SysMonHistory - Background ring buffer of free-heap samples.
// Sampling runs on the LVGL task via an lv_timer so the SysMon screen
// can show the history that built up while other apps were on screen.
class SysMonHistory {
public:
    static constexpr int CAPACITY = 60;
    static constexpr uint32_t INTERVAL_MS = 1000;

    static SysMonHistory& getInstance() {
        static SysMonHistory inst;
        return inst;
    }

    // Idempotent: starts the sampling timer the first time it is called.
    void begin() {
        if (_timer) return;
        sample();
        _timer = lv_timer_create([](lv_timer_t* t) {
            ((SysMonHistory*)t->user_data)->sample();
        }, INTERVAL_MS, this);
    }

    void sample() {
        _buf[_head] = ESP.getFreeHeap();
        _head = (_head + 1) % CAPACITY;
        if (_count < CAPACITY) _count++;
    }

    // Copy samples into out[0..CAPACITY-1], oldest first.
    // Slots without a recorded sample yet are filled with 0; use
    // validCount() to know how many of the trailing entries are real.
    void snapshot(uint32_t* out) const {
        int pad = CAPACITY - _count;
        int start = (_head - _count + CAPACITY) % CAPACITY;
        for (int i = 0; i < pad; i++) out[i] = 0;
        for (int i = 0; i < _count; i++) {
            out[pad + i] = _buf[(start + i) % CAPACITY];
        }
    }

    int validCount() const { return _count; }

    uint32_t latest() const {
        if (_count == 0) return ESP.getFreeHeap();
        int idx = (_head - 1 + CAPACITY) % CAPACITY;
        return _buf[idx];
    }

private:
    uint32_t _buf[CAPACITY] = {0};
    int _head = 0;
    int _count = 0;
    lv_timer_t* _timer = nullptr;
};
