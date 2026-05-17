#pragma once
#include <lvgl.h>

class ToastManager {
public:
    static ToastManager& getInstance() {
        static ToastManager instance;
        return instance;
    }

    // Show a toast message for a specified duration (default 3s)
    void showToast(const char* message, uint32_t duration = 3000);

    // Initialize the toast manager at startup
    void init();
private:
    ToastManager() = default;
    ToastManager(const ToastManager&) = delete;
    ToastManager& operator=(const ToastManager&) = delete;

    lv_obj_t* toastContainer = nullptr;
};
