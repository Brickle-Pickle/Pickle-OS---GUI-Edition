#include "toast_manager.h"
#include "../theme/theme.h"
#include <Arduino.h>

namespace {
lv_timer_t* toastHideTimer = nullptr;
}

void ToastManager::init() {
    if (toastContainer) {
        return;
    }

    // Create a hidden container for toasts on the top layer
    toastContainer = lv_obj_create(lv_layer_top());
    lv_obj_set_size(toastContainer, 180, 50);
    lv_obj_align(toastContainer, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(toastContainer, gTheme->backgroundPopup, 0);
    lv_obj_set_style_bg_opa(toastContainer, LV_OPA_80, 0);
    lv_obj_set_style_radius(toastContainer, 5, 0);
    lv_obj_clear_flag(toastContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(toastContainer, LV_OBJ_FLAG_HIDDEN);
}

// showToast displays a message for a specified duration (default 3s)
// Replaces any existing toast if one is active
void ToastManager::showToast(const char* message, ToastType toastType, uint32_t duration) {
    if (!message || message[0] == '\0') return;

    if (!toastContainer) {
        Serial.println("[ToastManager] Error: Toast container not found. Make sure init() is called.");
        return;
    }

    if (toastHideTimer) {
        lv_timer_del(toastHideTimer);
        toastHideTimer = nullptr;
    }

    // Always clean the container (whether hidden or not)
    lv_obj_clean(toastContainer);
    lv_obj_add_flag(toastContainer, LV_OBJ_FLAG_HIDDEN);

    // Create a new label for the toast message 
    // If message is too long will clip '...' at the end
    lv_obj_t* label = lv_label_create(toastContainer);
    lv_label_set_text(label, message);
    lv_obj_set_style_text_font(label, gFontNormal, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);  
    lv_obj_set_width(label, 180 - 10); // 10px padding for ellipsis

    // Add color based on toast type
    addColorByType(label, toastContainer, toastType, false, nullptr);

    // Show the toast
    lv_obj_clear_flag(toastContainer, LV_OBJ_FLAG_HIDDEN);
    // Hide the toast after the specified duration
    toastHideTimer = lv_timer_create([](lv_timer_t* t) {
        lv_obj_t* container = (lv_obj_t*)t->user_data;
        lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);
        toastHideTimer = nullptr;
        lv_timer_del(t);
    }, duration, toastContainer);
}

void ToastManager::showIconToast(const char* message, const void* icon, ToastType toastType, uint32_t duration) {
    // Toast with icon
    if (!message || message[0] == '\0') return;

    if (!toastContainer) {
        Serial.println("[ToastManager] Error: Toast container not found. Make sure init() is called.");
        return;
    }

    if (toastHideTimer) {
        lv_timer_del(toastHideTimer);
        toastHideTimer = nullptr;
    }

    // Always clean the container (whether hidden or not)
    lv_obj_clean(toastContainer);
    lv_obj_add_flag(toastContainer, LV_OBJ_FLAG_HIDDEN);

    // Create a new label for the toast message
    lv_obj_t* label = lv_label_create(toastContainer);
    lv_label_set_text(label, message);
    lv_obj_set_style_text_font(label, gFontNormal, LV_PART_MAIN);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(label, 180 - 10 - 40); // 10px padding + 40px for icon
    lv_obj_align(label, LV_ALIGN_CENTER, 20, 0); // Shift right for icon

    // Add icon (LV_SYMBOL_* are special text strings, not images)
    lv_obj_t* iconLabel = nullptr;
    if (icon) {
        iconLabel = lv_label_create(toastContainer);
        lv_label_set_text(iconLabel, (const char*)icon);
        lv_obj_set_style_text_font(iconLabel, gFontIcon, LV_PART_MAIN);
        lv_obj_align(iconLabel, LV_ALIGN_LEFT_MID, 10, 0);
    }

    // Add color based on toast type (also colors the icon label)
    addColorByType(label, toastContainer, toastType, iconLabel != nullptr, iconLabel);

    // Show the toast
    lv_obj_clear_flag(toastContainer, LV_OBJ_FLAG_HIDDEN);
    // Hide the toast after the specified duration
    toastHideTimer = lv_timer_create([](lv_timer_t* t) {
        lv_obj_t* container = (lv_obj_t*)t->user_data;
        lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);
        toastHideTimer = nullptr;
        lv_timer_del(t);
    }, duration, toastContainer);

}

// Helper function to set text color based on toast type
void ToastManager::addColorByType(lv_obj_t* label, lv_obj_t* toastContainer, ToastType toastType, bool isIconToast, lv_obj_t* icon) {
    switch (toastType) {
        case ToastType::INFO:
            lv_obj_set_style_text_color(label, gTheme->primaryLight, 0);
            lv_obj_set_style_bg_color(toastContainer, gTheme->primaryDark, 0);
            if (isIconToast) lv_obj_set_style_text_color(icon, gTheme->primaryLight, 0);
            break;
        case ToastType::SUCCESS:
            lv_obj_set_style_text_color(label, gTheme->successText, 0);
            lv_obj_set_style_bg_color(toastContainer, gTheme->successBg, 0);
            if (isIconToast) lv_obj_set_style_text_color(icon, gTheme->successText, 0);
            break;
        case ToastType::ALERT:
            lv_obj_set_style_text_color(label, gTheme->alertText, 0);
            lv_obj_set_style_bg_color(toastContainer, gTheme->alertBg, 0);
            if (isIconToast) lv_obj_set_style_text_color(icon, gTheme->alertText, 0);
            break;
        case ToastType::ERROR:
            lv_obj_set_style_text_color(label, gTheme->errorText, 0);
            lv_obj_set_style_bg_color(toastContainer, gTheme->errorBg, 0);
            if (isIconToast) lv_obj_set_style_text_color(icon, gTheme->errorText, 0);
            break;
        default:
            lv_obj_set_style_text_color(label, gTheme->primaryLight, 0);
            lv_obj_set_style_bg_color(toastContainer, gTheme->primaryDark, 0);
            if (isIconToast) lv_obj_set_style_text_color(icon, gTheme->primaryLight, 0);
    }
}