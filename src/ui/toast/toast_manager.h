#pragma once
#include <lvgl.h>
#include "toast_type.h"

class ToastManager {
public:
    static ToastManager& getInstance() {
        static ToastManager instance;
        return instance;
    }

    // Show a toast message for a specified duration (default 3s)
    void showToast(const char* message, ToastType toastType = ToastType::INFO, uint32_t duration = 3000);

    // Show a toast message with an icon
    void showIconToast(const char* message, const void* icon, ToastType toastType = ToastType::INFO, uint32_t duration = 3000);

    // Initialize the toast manager at startup
    void init();
private:
    ToastManager() = default;
    ToastManager(const ToastManager&) = delete;
    ToastManager& operator=(const ToastManager&) = delete;

    lv_obj_t* toastContainer = nullptr;

    // Helper to set toast color based on type
    void addColorByType(lv_obj_t* label, lv_obj_t* toastContainer, ToastType toastType, bool isIconToast, lv_obj_t* icon);
};
