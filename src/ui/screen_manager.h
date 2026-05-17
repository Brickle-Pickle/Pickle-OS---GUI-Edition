#pragma once
#include <vector>
#include <lvgl.h>
#include "screens/screen_base.h"

/**
 * ScreenManager - Screen manager with navigation stack.
 *
 * Singleton: access via ScreenManager::getInstance()
 *
 * Basic usage:
 *   ScreenManager::getInstance().navigateTo(new HomeScreen());
 *   ScreenManager::getInstance().goBack();
 *   ScreenManager::getInstance().goHome();
 */
class ScreenManager {
public:
    static ScreenManager& getInstance() {
        static ScreenManager instance;
        return instance;
    }

    // Destructor clears all screens from the stack
    ~ScreenManager() { clearStack(); }

    /**
     * Navigate to a new screen (push).
     * The current screen is paused. The new screen calls onCreate().
     */
    void navigateTo(ScreenBase* screen,
                    lv_scr_load_anim_t anim = LV_SCR_LOAD_ANIM_MOVE_LEFT,
                    uint32_t animTime = 200);

    /**
     * Return to the previous screen (pop).
     * Destroys the current screen and resumes the previous one.
     */
    void goBack(lv_scr_load_anim_t anim = LV_SCR_LOAD_ANIM_MOVE_RIGHT,
                uint32_t animTime = 200);

    // Return to the root screen clearing the intermediate stack.
    void goHome(lv_scr_load_anim_t anim = LV_SCR_LOAD_ANIM_FADE_ON,
                uint32_t animTime = 250);

    // Destroy the entire stack and start fresh with a new root screen.
    // Use this after theme/font changes so new widgets pick up the updated styles.
    void replaceRoot(ScreenBase* screen,
                     lv_scr_load_anim_t anim = LV_SCR_LOAD_ANIM_FADE_ON,
                     uint32_t animTime = 250);

    // Replace the current screen without stacking (without return history).
    void replaceCurrent(ScreenBase* screen,
                        lv_scr_load_anim_t anim = LV_SCR_LOAD_ANIM_FADE_ON,
                        uint32_t animTime = 200);

    // Check if we can go back
    bool canGoBack() const { return _stack.size() > 1; }
    
    // Get stack depth
    int  stackDepth() const { return (int)_stack.size(); }

private:
    ScreenManager() = default;
    ScreenManager(const ScreenManager&) = delete;
    ScreenManager& operator=(const ScreenManager&) = delete;

    void clearStack();

    std::vector<ScreenBase*> _stack;
};
