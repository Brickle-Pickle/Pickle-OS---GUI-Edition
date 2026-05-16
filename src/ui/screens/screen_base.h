#pragma once
#include <lvgl.h>

/**
 * ScreenBase - Base interface for all screens.
 *
 * Lifecycle:
 *   onCreate() → called when pushing the screen (create widgets)
 *   onResume() → called when returning to this screen from a pop
 *   onPause() → called when another screen is placed on top
 *   onDestroy() → called when popping (free LVGL resources)
 */
class ScreenBase {
public:
    virtual ~ScreenBase() = default;

    // Called when pushing the screen (create widgets)
    virtual void onCreate() = 0;
    
    // Called when returning to this screen from a pop
    virtual void onResume() {}
    
    // Called when another screen is placed on top
    virtual void onPause() {}
    
    // Called when popping (free LVGL resources)
    virtual void onDestroy() = 0;

    lv_obj_t* getScreen() { return _screen; }

protected:
    lv_obj_t* _screen = nullptr;
};
