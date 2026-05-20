#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include <functional>
#include <vector>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

// LuaEngine - one-shot Lua VM bound to a single game session.
// Built with a host LVGL parent and HUD callbacks. run(script) opens the
// VM, registers bindings and executes the script top-level code, which
// typically registers callbacks and schedules timers. While the screen
// lives, LVGL events and timers re-enter the VM via the stored callback
// references. The destructor closes the VM and frees tracked timers;
// LVGL widgets are released when the host screen is destroyed.
// The bindings stay minimal: every primitive a game needs is exposed
// under three globals (lvgl, canvas, game).
class LuaEngine {
public:
    using HudCb = std::function<void(const char* leftText, const char* rightText)>;
    using GameOverCb = std::function<void(int score)>;

    LuaEngine(lv_obj_t* host, HudCb hud, GameOverCb gameOver);
    ~LuaEngine();

    // Loads and executes the given script. Returns true on success. On
    // failure the engine writes to Serial and shows an alert toast.
    bool run(const String& script);

    // Manually invoke the game-over flow with the current score.
    void triggerGameOver();

    // Invoke the HUD callback supplied at construction time. Public so the
    // C-style Lua bindings can reach it without friend declarations.
    void invokeHud(const char* left, const char* right) {
        if (_hud) _hud(left ? left : "", right ? right : "");
    }

    int score() const { return _score; }
    void setScore(int s) { _score = s; }
    void addScore(int d) { _score += d; }
    lv_obj_t* host() const { return _host; }

    // Track timer bookkeeping records created from Lua so they can be cleaned
    // safely at shutdown. The opaque pointer is a TimerEntry defined in the
    // implementation file.
    void trackTimerEntry(void* entry) { _timers.push_back(entry); }

    static LuaEngine* fromState(lua_State* L);

private:
    lua_State* _state = nullptr;
    lv_obj_t* _host = nullptr;
    HudCb _hud;
    GameOverCb _gameOver;
    int _score = 0;
    bool _gameOverFired = false;
    std::vector<void*> _timers;

    void _registerApi();
    void _stopTimers();
};
