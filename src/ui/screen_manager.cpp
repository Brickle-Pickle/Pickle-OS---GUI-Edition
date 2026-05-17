#include "screen_manager.h"
#include <Arduino.h>

// Deferred deletion: LVGL needs the old screen's object alive for the full
// animation duration. Destroying it before lv_scr_load_anim causes a
// use-after-free crash that resets the ESP32.
static void _deferredDeleteCb(lv_timer_t* timer) {
    ScreenBase* screen = static_cast<ScreenBase*>(timer->user_data);
    screen->onDestroy();
    delete screen;
}

static void _deferDelete(ScreenBase* screen, uint32_t animTime) {
    lv_timer_t* t = lv_timer_create(_deferredDeleteCb, animTime + 50, screen);
    lv_timer_set_repeat_count(t, 1);
}

// navigateTo (push)
void ScreenManager::navigateTo(
    ScreenBase* screen,
    lv_scr_load_anim_t anim,
    uint32_t animTime
) {
    if (!screen) return;

    if (!_stack.empty()) {
        _stack.back()->onPause();
    }

    screen->onCreate();
    lv_scr_load_anim(screen->getScreen(), anim, animTime, 0, false);
    _stack.push_back(screen);

    Serial.printf("[ScreenManager] navigateTo → stack depth: %d\n", (int)_stack.size());
}

// goBack (pop)
void ScreenManager::goBack(lv_scr_load_anim_t anim, uint32_t animTime)
{
    if (_stack.size() <= 1) {
        Serial.println("[ScreenManager] goBack → already at home, ignored");
        return;
    }

    ScreenBase* current = _stack.back();
    _stack.pop_back();

    // Resume previous screen and start animation BEFORE destroying current.
    // The old screen object must stay alive until the animation finishes.
    ScreenBase* previous = _stack.back();
    previous->onResume();
    lv_scr_load_anim(previous->getScreen(), anim, animTime, 0, false);

    _deferDelete(current, animTime);

    Serial.printf("[ScreenManager] goBack → stack depth: %d\n", (int)_stack.size());
}

// goHome (pop until root)
void ScreenManager::goHome(lv_scr_load_anim_t anim, uint32_t animTime)
{
    if (_stack.size() <= 1) {
        if (!_stack.empty()) _stack.back()->onResume();
        Serial.println("[ScreenManager] goHome → already at root");
        return;
    }

    // Save the currently displayed screen — it must outlive the animation.
    ScreenBase* current = _stack.back();
    _stack.pop_back();

    // Intermediate screens are not visible: destroy them immediately.
    while (_stack.size() > 1) {
        ScreenBase* s = _stack.back();
        _stack.pop_back();
        s->onDestroy();
        delete s;
    }

    _stack.back()->onResume();
    lv_scr_load_anim(_stack.back()->getScreen(), anim, animTime, 0, false);

    _deferDelete(current, animTime);

    Serial.println("[ScreenManager] goHome → back to root");
}

// replaceCurrent (without history)
void ScreenManager::replaceCurrent(
    ScreenBase* screen,
    lv_scr_load_anim_t anim,
    uint32_t animTime
)
{
    if (!screen) return;

    ScreenBase* old = nullptr;
    if (!_stack.empty()) {
        old = _stack.back();
        _stack.pop_back();
    }

    screen->onCreate();
    lv_scr_load_anim(screen->getScreen(), anim, animTime, 0, false);
    _stack.push_back(screen);

    if (old) _deferDelete(old, animTime);

    Serial.printf("[ScreenManager] replaceCurrent → stack depth: %d\n", (int)_stack.size());
}

// replaceRoot — destroy everything and start fresh
void ScreenManager::replaceRoot(ScreenBase* screen, lv_scr_load_anim_t anim, uint32_t animTime)
{
    if (!screen) return;

    // Save the currently displayed screen — it must outlive the animation.
    ScreenBase* current = !_stack.empty() ? _stack.back() : nullptr;
    if (current) _stack.pop_back();

    // Remaining screens in the stack are not visible: destroy immediately.
    clearStack();

    screen->onCreate();
    lv_scr_load_anim(screen->getScreen(), anim, animTime, 0, false);
    _stack.push_back(screen);

    if (current) _deferDelete(current, animTime);

    Serial.println("[ScreenManager] replaceRoot → fresh root");
}

// clearStack (immediate, no animation)
void ScreenManager::clearStack()
{
    while (!_stack.empty()) {
        ScreenBase* s = _stack.back();
        _stack.pop_back();
        s->onDestroy();
        delete s;
    }
}
