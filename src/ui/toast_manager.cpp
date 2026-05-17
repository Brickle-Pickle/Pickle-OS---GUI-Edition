#include "toast_manager.h"
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
    lv_obj_set_style_bg_color(toastContainer, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(toastContainer, LV_OPA_80, 0);
    lv_obj_set_style_radius(toastContainer, 5, 0);
    lv_obj_clear_flag(toastContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(toastContainer, LV_OBJ_FLAG_HIDDEN);
}

// showToast displays a message for a specified duration (default 3s)
// Replaces any existing toast if one is active
void ToastManager::showToast(const char* message, uint32_t duration) {
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
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);  
    lv_obj_set_width(label, 180 - 10); // 10px padding for ellipsis

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