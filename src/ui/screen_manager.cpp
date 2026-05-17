#include "screen_manager.h"
#include <Arduino.h>

// navigateTo (push)
void ScreenManager::navigateTo(
    ScreenBase* screen,
    lv_scr_load_anim_t anim,
    uint32_t animTime
) {
    if (!screen) return;

    // Pause current screen
    if (!_stack.empty()) {
        _stack.back()->onPause();
    }

    // Initialize new screen and show it
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

    // Destroy current screen
    ScreenBase* current = _stack.back();
    _stack.pop_back();
    current->onDestroy();
    delete current;

    // Resume previous screen
    ScreenBase* previous = _stack.back();
    previous->onResume();
    lv_scr_load_anim(previous->getScreen(), anim, animTime, 0, false);

    Serial.printf("[ScreenManager] goBack → stack depth: %d\n", (int)_stack.size());
}

// goHome (pop until root)
void ScreenManager::goHome(lv_scr_load_anim_t anim, uint32_t animTime)
{
    while (_stack.size() > 1) {
        ScreenBase* s = _stack.back();
        _stack.pop_back();
        s->onDestroy();
        delete s;
    }

    if (!_stack.empty()) {
        _stack.back()->onResume();
        lv_scr_load_anim(_stack.back()->getScreen(), anim, animTime, 0, false);
    }

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

    if (!_stack.empty()) {
        ScreenBase* old = _stack.back();
        _stack.pop_back();
        old->onDestroy();
        delete old;
    }

    screen->onCreate();
    lv_scr_load_anim(screen->getScreen(), anim, animTime, 0, false);
    _stack.push_back(screen);

    Serial.printf("[ScreenManager] replaceCurrent → stack depth: %d\n", (int)_stack.size());
}

// replaceRoot — destroy everything and start fresh
void ScreenManager::replaceRoot(ScreenBase* screen, lv_scr_load_anim_t anim, uint32_t animTime)
{
    if (!screen) return;
    clearStack();
    screen->onCreate();
    lv_scr_load_anim(screen->getScreen(), anim, animTime, 0, false);
    _stack.push_back(screen);
    Serial.println("[ScreenManager] replaceRoot → fresh root");
}

// clearStack (reset)
void ScreenManager::clearStack()
{
    while (!_stack.empty()) {
        ScreenBase* s = _stack.back();
        _stack.pop_back();
        s->onDestroy();
        delete s;
    }
}
