# On-Screen Keyboard — Usage Guide

## Overview

The keyboard is implemented as a reusable overlay component (`KeyboardOverlay`) backed by LVGL's built-in `lv_keyboard` widget. It supports four layouts switchable at runtime:

| Mode constant                  | Layout                |
| ------------------------------ | --------------------- |
| `LV_KEYBOARD_MODE_TEXT_LOWER`  | QWERTY lowercase      |
| `LV_KEYBOARD_MODE_TEXT_UPPER`  | QWERTY uppercase      |
| `LV_KEYBOARD_MODE_SPECIAL`     | Symbols / punctuation |
| `LV_KEYBOARD_MODE_NUMBER`      | Numeric pad           |

The user can cycle between modes with the built-in mode button on the keyboard. LVGL handles the switching automatically.

---

## Files

| File                        | Purpose                     |
| --------------------------- | --------------------------- |
| `src/ui/keyboard_overlay.h` | Reusable keyboard component |

---

## Adding the keyboard to a screen

### 1. Include the header

```cpp
#include "../keyboard_overlay.h"
```

### 2. Declare a member

```cpp
private:
    KeyboardOverlay _kb;
```

### 3. Create and link it in `onCreate()`

```cpp
void onCreate() override {
    _screen = lv_obj_create(NULL);

    // Your textarea
    _ta = lv_textarea_create(_screen);
    lv_obj_set_size(_ta, 220, 40);
    lv_obj_align(_ta, LV_ALIGN_TOP_MID, 0, 16);
    lv_textarea_set_one_line(_ta, true);

    // Keyboard setup
    _kb.create(_screen);   // attach to this screen's root object
    _kb.linkTo(_ta);       // link keyboard input to the textarea

    // Show QWERTY when the textarea gains focus
    lv_obj_add_event_cb(_ta, [](lv_event_t* e) {
        auto* self = static_cast<MyScreen*>(lv_event_get_user_data(e));
        self->_kb.show();           // QWERTY by default
        // self->_kb.showNumeric(); // or numeric pad
    }, LV_EVENT_FOCUSED, this);

    // Hide keyboard when the user presses OK (✓)
    lv_obj_add_event_cb(_kb.obj(), [](lv_event_t* e) {
        auto* self = static_cast<MyScreen*>(lv_event_get_user_data(e));
        self->_kb.hide();
    }, LV_EVENT_READY, this);
}
```

### 4. Clean up in `onDestroy()`

Nothing extra needed — the keyboard widget is a child of `_screen`, so `lv_obj_del(_screen)` destroys it automatically.

---

## KeyboardOverlay API

```cpp
// Attach keyboard to a parent lv_obj (call once in onCreate)
void create(lv_obj_t* parent);

// Link keyboard to a textarea so typed characters appear there
void linkTo(lv_obj_t* textarea);

// Show keyboard — optionally pass a mode (default: TEXT_LOWER / QWERTY)
void show(lv_keyboard_mode_t mode = LV_KEYBOARD_MODE_TEXT_LOWER);

// Shorthand: show numeric pad
void showNumeric();

// Hide keyboard without destroying it
void hide();

// Access the raw lv_obj_t* (for event registration)
lv_obj_t* obj();
```

---

## Layout notes (240×320 display)

- The keyboard is positioned at the bottom with a fixed height of **140 px**.
- That leaves **~180 px** of usable space above the keyboard.
- If content can scroll, call `lv_obj_scroll_to_view(_ta, LV_ANIM_ON)` after `show()` so the active textarea is never hidden behind the keyboard.

```text
┌──────────────────────┐  ← 0
│                      │
│   Screen content     │  ~180 px
│   [ textarea ]       │
│                      │
├──────────────────────┤  ← 180
│   Q W E R T Y ...   │
│   A S D F G H ...   │  140 px
│   Z X C V B N ...   │
│  123  space  ✓  ←   │
└──────────────────────┘  ← 320
```

---

## Switching layouts programmatically

```cpp
// Force QWERTY uppercase
_kb.show(LV_KEYBOARD_MODE_TEXT_UPPER);

// Force numeric pad
_kb.show(LV_KEYBOARD_MODE_NUMBER);

// Force symbols
_kb.show(LV_KEYBOARD_MODE_SPECIAL);
```

---

## Multiple textareas on the same screen

Call `linkTo()` again when a different textarea gains focus — the keyboard will redirect its output:

```cpp
lv_obj_add_event_cb(_ta2, [](lv_event_t* e) {
    auto* self = static_cast<MyScreen*>(lv_event_get_user_data(e));
    self->_kb.linkTo(self->_ta2);
    self->_kb.show();
}, LV_EVENT_FOCUSED, this);
```

---

## Related

- [How to make screens](how_to_make_screens.md)
- [LVGL keyboard widget docs](https://docs.lvgl.io/master/widgets/keyboard.html)
