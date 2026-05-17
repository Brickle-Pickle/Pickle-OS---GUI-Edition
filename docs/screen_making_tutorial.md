# Screen Making Tutorial - Pickle OS GUI Edition

This document explains the complete process of creating a new screen, wiring it into
the navigation system, and registering it in the home screen app grid. Every concept is
illustrated with a full Settings screen that contains one of each widget type available
in the project.

---

## Table of Contents

1. [Project structure](#1-project-structure)
2. [How the screen system works](#2-how-the-screen-system-works)
3. [Screen lifecycle](#3-screen-lifecycle)
4. [Creating a new screen file](#4-creating-a-new-screen-file)
5. [Building the header](#5-building-the-header)
6. [Building a scrollable content area](#6-building-a-scrollable-content-area)
7. [Widgets](#7-widgets)
   - [Label](#71-label)
   - [Button](#72-button)
   - [Switch](#73-switch)
   - [Slider](#74-slider)
   - [Dropdown](#75-dropdown)
   - [Checkbox](#76-checkbox)
   - [Text area](#77-text-area)
   - [Bar (progress bar)](#78-bar-progress-bar)
   - [Separator line](#79-separator-line)
   - [Section card](#710-section-card)
   - [Row with key and value](#711-row-with-key-and-value)
8. [Layout systems](#8-layout-systems)
   - [Manual positioning](#81-manual-positioning)
   - [Alignment helpers](#82-alignment-helpers)
   - [Flex layout](#83-flex-layout)
9. [Styling reference](#9-styling-reference)
   - [Colors](#91-colors)
   - [Size and position](#92-size-and-position)
   - [Fonts](#93-fonts)
   - [Border, radius and padding](#94-border-radius-and-padding)
   - [Shadow](#95-shadow)
   - [Opacity](#96-opacity)
   - [States](#97-states)
10. [Events and callbacks](#10-events-and-callbacks)
11. [Navigation between screens](#11-navigation-between-screens)
12. [Registering the screen in main.cpp](#12-registering-the-screen-in-maincpp)
13. [Adding the app icon in home_screen.h](#13-adding-the-app-icon-in-home_screenh)
14. [Available LVGL symbols](#14-available-lvgl-symbols)
15. [Complete example - settings_screen.h](#15-complete-example---settings_screenh)

---

## 1. Project structure

All screen files live inside `src/ui/screens/`. One header file per screen.

```
src/
  main.cpp                      <- include + launcher function for each screen
  ui/
    screen_manager.h            <- navigation stack API
    screen_manager.cpp
    screens/
      screen_base.h             <- base class, do not modify
      home_screen.h             <- root screen, contains the app grid
      about_screen.h            <- example of a simple info screen
      settings_screen.h         <- the screen you will create
```

---

## 2. How the screen system works

`ScreenManager` is a singleton that owns a stack of `ScreenBase*` pointers.
When you navigate to a new screen it is pushed on top. When you go back the top
screen is destroyed and the one below it resumes.

```
Stack at boot:              [ HomeScreen ]
After tapping Settings:     [ HomeScreen | SettingsScreen ]
After pressing back:        [ HomeScreen ]
```

You never call `lv_scr_load()` directly. Always go through `ScreenManager`.

The launcher pattern used in this project works as follows:

- `main.cpp` declares a plain `void launchX()` function for each screen.
- The home screen icon callback calls that function via `extern void launchX()`.
- The launcher calls `ScreenManager::getInstance().navigateTo(new XScreen(), anim)`.

This avoids circular includes between `home_screen.h` and the target screen header.

---

## 3. Screen lifecycle

Every screen class inherits `ScreenBase` and must implement `onCreate()` and
`onDestroy()`. The other two methods are optional.

| Method | When it runs | What to do inside |
|---|---|---|
| `onCreate()` | Screen is pushed onto the stack | Create `_screen` with `lv_obj_create(NULL)`, build all widgets |
| `onResume()` | Returned to after a pop (back navigation) | Refresh dynamic data: clock, sensor readings, slider values |
| `onPause()` | Another screen is pushed on top | Stop periodic timers if any |
| `onDestroy()` | Screen is popped off the stack | Call `lv_obj_del(_screen)` and set `_screen = nullptr` |

`_screen` is the root LVGL object declared in `ScreenBase`. Every widget you create
must be a child of `_screen` or a child of a child.

---

## 4. Creating a new screen file

Create `src/ui/screens/settings_screen.h`. The bare minimum looks like this:

```cpp
#pragma once
#include "screen_base.h"
#include "../screen_manager.h"

// SettingsScreen - System settings
class SettingsScreen : public ScreenBase {
public:
    void onCreate() override {
        _screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(_screen, lv_color_hex(0x121212), LV_PART_MAIN);

        _buildHeader();
        _buildContent();
    }

    void onResume() override {
        // Refresh any dynamic values here when the user comes back
    }

    void onDestroy() override {
        if (_screen) {
            lv_obj_del(_screen);
            _screen = nullptr;
        }
    }

private:
    void _buildHeader() { /* see section 5 */ }
    void _buildContent() { /* see section 6 and 7 */ }
};
```

Rules:

- `_screen` must be created with `lv_obj_create(NULL)`. The `NULL` parent makes it a
  standalone LVGL screen, not a child widget.
- `lv_obj_del(_screen)` in `onDestroy()` recursively destroys all child widgets.
  You do not need to delete them individually.
- Never call `lv_obj_del` on any widget pointer while the screen is still active.
- Member widget pointers that need to be updated later (like a label showing a sensor
  value) must be declared as private member variables, not local variables inside a
  build method.

---

## 5. Building the header

The header is a fixed container pinned to the top of the screen. On a 320px tall
display, 40px for the header leaves 280px for content.

It always contains:

- A back button on the left (not present on HomeScreen which is the root).
- A centered title label.

```cpp
void _buildHeader() {
    // Container pinned to the top, full width, no scroll, no radius
    lv_obj_t* header = lv_obj_create(_screen);
    lv_obj_set_size(header, 240, 40);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x1E1E2E), LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(header, 0, LV_PART_MAIN);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    // Back button - navigates to the previous screen on tap
    lv_obj_t* backBtn = lv_btn_create(header);
    lv_obj_set_size(backBtn, 36, 28);
    lv_obj_align(backBtn, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_set_style_bg_color(backBtn, lv_color_hex(0x313244), LV_PART_MAIN);
    lv_obj_set_style_radius(backBtn, 8, LV_PART_MAIN);
    lv_obj_add_event_cb(backBtn, [](lv_event_t* e) {
        ScreenManager::getInstance().goBack();
    }, LV_EVENT_CLICKED, NULL);

    // Arrow icon inside the back button
    lv_obj_t* backIco = lv_label_create(backBtn);
    lv_label_set_text(backIco, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(backIco, lv_color_hex(0x89B4FA), LV_PART_MAIN);
    lv_obj_center(backIco);

    // Screen title centered in the header
    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_color(title, lv_color_hex(0xCDD6F4), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
}
```

`lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE)` prevents the header from
intercepting scroll gestures meant for the content below it.

---

## 6. Building a scrollable content area

Content taller than 280px needs a scrollable container. Create a plain `lv_obj_t`
below the header, enable flex column layout, and add widgets as children. LVGL will
scroll it automatically when its children overflow the height.

```cpp
void _buildContent() {
    lv_obj_t* scroll = lv_obj_create(_screen);
    lv_obj_set_size(scroll, 240, 280);   // full width, all vertical space below header
    lv_obj_set_pos(scroll, 0, 40);       // starts just below the 40px header
    lv_obj_set_style_bg_color(scroll, lv_color_hex(0x121212), LV_PART_MAIN);
    lv_obj_set_style_border_width(scroll, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(scroll, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scroll, 8, LV_PART_MAIN);   // inner padding around edges
    lv_obj_set_style_pad_row(scroll, 8, LV_PART_MAIN);   // gap between flex children
    lv_obj_set_flex_flow(scroll, LV_FLEX_FLOW_COLUMN);

    // All section cards are children of scroll from here
    _buildDisplaySection(scroll);
    _buildConnectivitySection(scroll);
    _buildSystemSection(scroll);
    _buildDangerSection(scroll);
}
```

`lv_obj_set_style_pad_row` sets the vertical gap between flex children.
`lv_obj_set_style_pad_all` sets the inner padding around all four edges.

---

## 7. Widgets

Every widget is created with `lv_WIDGETNAME_create(parent)`. The parent can be
`_screen`, the scroll container, a card, or any other `lv_obj_t*`.

The project color palette used in all examples:

| Role | Hex | Use |
|---|---|---|
| Background | `0x121212` | Screen background |
| Surface | `0x1E1E2E` | Cards, header |
| Elevated | `0x313244` | Buttons, inputs |
| Hover | `0x45475A` | Pressed state, separators |
| Muted | `0x585B70` | Secondary text |
| Text | `0xCDD6F4` | Primary text |
| Accent | `0x89B4FA` | Icons, active controls |
| Danger | `0xF38BA8` | Destructive actions |

### 7.1 Label

A read-only text widget. Use it for titles, descriptions, and dynamic values.

```cpp
lv_obj_t* lbl = lv_label_create(parent);
lv_label_set_text(lbl, "Display brightness");
lv_obj_set_style_text_color(lbl, lv_color_hex(0xCDD6F4), LV_PART_MAIN);
lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
```

To update a label at runtime, keep the pointer as a private member and call:

```cpp
lv_label_set_text(_myLabel, "New text");

// For formatted values
lv_label_set_text_fmt(_myLabel, "Free RAM: %d KB", freeHeap / 1024);
```

Long text that overflows the widget width can be handled with long mode:

```cpp
lv_label_set_long_mode(lbl, LV_LABEL_LONG_SCROLL_CIRCULAR); // scrolls automatically
lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);            // wraps to next line
lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);             // clips with "..."
lv_obj_set_width(lbl, 180);                                  // required for long mode
```

### 7.2 Button

A pressable container. The visual element is just the background. Add a label child
for text or a label with an `LV_SYMBOL_*` for an icon.

```cpp
lv_obj_t* btn = lv_btn_create(parent);
lv_obj_set_size(btn, 200, 36);
lv_obj_set_style_bg_color(btn, lv_color_hex(0x313244), LV_PART_MAIN);
lv_obj_set_style_bg_color(btn, lv_color_hex(0x45475A), LV_STATE_PRESSED);
lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
lv_obj_add_event_cb(btn, [](lv_event_t* e) {
    // action when tapped
}, LV_EVENT_CLICKED, NULL);

lv_obj_t* btnLbl = lv_label_create(btn);
lv_label_set_text(btnLbl, "Save settings");
lv_obj_set_style_text_color(btnLbl, lv_color_hex(0xCDD6F4), LV_PART_MAIN);
lv_obj_center(btnLbl);
```

To pass data to the callback, use the `user_data` parameter of `lv_obj_add_event_cb`
and read it with `lv_event_get_user_data(e)`:

```cpp
static int myValue = 42;
lv_obj_add_event_cb(btn, myCallback, LV_EVENT_CLICKED, &myValue);

// Inside the callback
void myCallback(lv_event_t* e) {
    int* val = (int*)lv_event_get_user_data(e);
}
```

### 7.3 Switch

An on/off toggle. Useful for enabling and disabling features. It has three styleable
parts: `LV_PART_MAIN` (track background), `LV_PART_KNOB` (the sliding circle), and
indicator color is applied through `LV_STATE_CHECKED` on `LV_PART_MAIN`.

```cpp
lv_obj_t* sw = lv_switch_create(parent);
// Track color when off
lv_obj_set_style_bg_color(sw, lv_color_hex(0x313244), LV_PART_MAIN);
// Track color when on (checked state)
lv_obj_set_style_bg_color(sw, lv_color_hex(0x89B4FA), LV_PART_MAIN | LV_STATE_CHECKED);
// Knob color
lv_obj_set_style_bg_color(sw, lv_color_hex(0xCDD6F4), LV_PART_KNOB);

// Set initial state
lv_obj_add_state(sw, LV_STATE_CHECKED);     // on by default
// lv_obj_clear_state(sw, LV_STATE_CHECKED); // off by default

lv_obj_add_event_cb(sw, [](lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_target(e);
    bool enabled = lv_obj_has_state(obj, LV_STATE_CHECKED);
    // use enabled
}, LV_EVENT_VALUE_CHANGED, NULL);
```

In the Settings screen this widget is placed on the right side of a row container
with a label on the left. See section 7.11 for the row pattern.

### 7.4 Slider

A draggable control for numeric values like brightness or volume. It has three
styleable parts: `LV_PART_MAIN` (track), `LV_PART_INDICATOR` (filled portion),
`LV_PART_KNOB` (the draggable handle).

```cpp
lv_obj_t* slider = lv_slider_create(parent);
lv_obj_set_width(slider, 200);
lv_slider_set_range(slider, 0, 255);            // min, max
lv_slider_set_value(slider, 128, LV_ANIM_OFF);  // initial value, no animation

// Track (unfilled background)
lv_obj_set_style_bg_color(slider, lv_color_hex(0x313244), LV_PART_MAIN);
// Indicator (filled portion from min to current value)
lv_obj_set_style_bg_color(slider, lv_color_hex(0x89B4FA), LV_PART_INDICATOR);
// Knob (the draggable circle)
lv_obj_set_style_bg_color(slider, lv_color_hex(0xCDD6F4), LV_PART_KNOB);

lv_obj_add_event_cb(slider, [](lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_target(e);
    int32_t val = lv_slider_get_value(obj); // val is between the set range
}, LV_EVENT_VALUE_CHANGED, NULL);
```

To display the current value next to the slider, store a label pointer as a member and
update it inside the callback by passing `this` as user data:

```cpp
// In the callback registered with lv_obj_add_event_cb(..., this)
SettingsScreen* self = (SettingsScreen*)lv_event_get_user_data(e);
lv_label_set_text_fmt(self->_brightnessLabel, "%d", lv_slider_get_value(obj));
```

### 7.5 Dropdown

A compact selector for a list of options. Options are separated by `\n` in a single
string. Tapping the widget opens a list below (or above if configured).

```cpp
lv_obj_t* dd = lv_dropdown_create(parent);
lv_dropdown_set_options(dd, "English\nEspanol\nFrancais\nDeutsch");
lv_dropdown_set_selected(dd, 0);     // select index 0 by default
lv_obj_set_width(dd, 160);
lv_obj_set_style_bg_color(dd, lv_color_hex(0x313244), LV_PART_MAIN);
lv_obj_set_style_text_color(dd, lv_color_hex(0xCDD6F4), LV_PART_MAIN);
lv_obj_set_style_border_width(dd, 0, LV_PART_MAIN);
lv_obj_set_style_radius(dd, 8, LV_PART_MAIN);

lv_obj_add_event_cb(dd, [](lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_target(e);
    uint16_t idx = lv_dropdown_get_selected(obj); // zero-based index
    char buf[32];
    lv_dropdown_get_selected_str(obj, buf, sizeof(buf)); // selected string
}, LV_EVENT_VALUE_CHANGED, NULL);
```

If the dropdown is near the bottom of the screen, open it upward:

```cpp
lv_dropdown_set_dir(dd, LV_DIR_TOP);
```

### 7.6 Checkbox

A boolean option with a built-in text label. It has two styleable parts:
`LV_PART_MAIN` (the text) and `LV_PART_INDICATOR` (the box that shows the check mark).

```cpp
lv_obj_t* cb = lv_checkbox_create(parent);
lv_checkbox_set_text(cb, "Enable haptic feedback");
lv_obj_set_style_text_color(cb, lv_color_hex(0xCDD6F4), LV_PART_MAIN);
lv_obj_set_style_text_font(cb, &lv_font_montserrat_14, LV_PART_MAIN);
// Box background when unchecked
lv_obj_set_style_bg_color(cb, lv_color_hex(0x313244), LV_PART_INDICATOR);
// Box background when checked
lv_obj_set_style_bg_color(cb, lv_color_hex(0x89B4FA), LV_PART_INDICATOR | LV_STATE_CHECKED);
// Box border
lv_obj_set_style_border_color(cb, lv_color_hex(0x45475A), LV_PART_INDICATOR);
lv_obj_set_style_border_width(cb, 1, LV_PART_INDICATOR);
lv_obj_add_state(cb, LV_STATE_CHECKED); // checked by default

lv_obj_add_event_cb(cb, [](lv_event_t* e) {
    bool checked = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    // use checked
}, LV_EVENT_VALUE_CHANGED, NULL);
```

### 7.7 Text area

A single-line or multi-line text input. It accepts keyboard input and can be wired
to an on-screen keyboard widget (to be added in a future phase). The cursor appears
when the widget is focused.

```cpp
lv_obj_t* ta = lv_textarea_create(parent);
lv_obj_set_size(ta, 200, 38);
lv_textarea_set_one_line(ta, true);               // single-line mode
lv_textarea_set_placeholder_text(ta, "Enter hostname...");
lv_textarea_set_max_length(ta, 64);               // character limit
lv_obj_set_style_bg_color(ta, lv_color_hex(0x313244), LV_PART_MAIN);
lv_obj_set_style_text_color(ta, lv_color_hex(0xCDD6F4), LV_PART_MAIN);
lv_obj_set_style_border_color(ta, lv_color_hex(0x45475A), LV_PART_MAIN);
lv_obj_set_style_border_width(ta, 1, LV_PART_MAIN);
// Highlight border when the field is focused
lv_obj_set_style_border_color(ta, lv_color_hex(0x89B4FA), LV_PART_MAIN | LV_STATE_FOCUSED);
lv_obj_set_style_radius(ta, 8, LV_PART_MAIN);

// Read the current text at any time
const char* text = lv_textarea_get_text(ta);

// Set text programmatically
lv_textarea_set_text(ta, "pickle-os.local");
```

For password fields, enable masking so typed characters are hidden after a short delay:

```cpp
lv_textarea_set_password_mode(ta, true);
lv_textarea_set_password_show_time(ta, 1500); // ms to show char before masking
```

### 7.8 Bar (progress bar)

A read-only progress indicator. Use it for RAM usage, battery level, download
progress, or any metric with a known range. It has two styleable parts:
`LV_PART_MAIN` (track background) and `LV_PART_INDICATOR` (filled portion).

```cpp
lv_obj_t* bar = lv_bar_create(parent);
lv_obj_set_size(bar, 200, 12);
lv_bar_set_range(bar, 0, 100);
lv_bar_set_value(bar, 60, LV_ANIM_ON);  // 60% with a smooth animation
// Track (empty background)
lv_obj_set_style_bg_color(bar, lv_color_hex(0x313244), LV_PART_MAIN);
// Indicator (filled portion)
lv_obj_set_style_bg_color(bar, lv_color_hex(0x89B4FA), LV_PART_INDICATOR);
lv_obj_set_style_radius(bar, 6, LV_PART_MAIN);
lv_obj_set_style_radius(bar, 6, LV_PART_INDICATOR);

// Update the value at runtime (e.g. from onResume)
lv_bar_set_value(bar, newValue, LV_ANIM_ON);
```

### 7.9 Separator line

A thin horizontal rule used to visually divide sections inside a card. It is a plain
`lv_obj_t` with fixed height of 1px, no border, and no radius.

```cpp
lv_obj_t* sep = lv_obj_create(parent);
lv_obj_set_size(sep, lv_pct(100), 1);  // full parent width, 1px tall
lv_obj_set_style_bg_color(sep, lv_color_hex(0x45475A), LV_PART_MAIN);
lv_obj_set_style_border_width(sep, 0, LV_PART_MAIN);
lv_obj_set_style_radius(sep, 0, LV_PART_MAIN);
lv_obj_set_style_pad_all(sep, 0, LV_PART_MAIN);
```

`lv_pct(100)` sets the width to 100% of the parent's inner width. This adapts
automatically if the parent is resized.

### 7.10 Section card

A rounded container that groups related settings together. Place a card inside the
scrollable content area and put widgets inside the card. The card uses flex column
layout so children stack vertically and the card grows to fit them.

```cpp
lv_obj_t* card = lv_obj_create(parent);
lv_obj_set_width(card, lv_pct(100));        // full scroll area width
lv_obj_set_height(card, LV_SIZE_CONTENT);   // grows automatically with children
lv_obj_set_style_bg_color(card, lv_color_hex(0x1E1E2E), LV_PART_MAIN);
lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
lv_obj_set_style_radius(card, 12, LV_PART_MAIN);
lv_obj_set_style_pad_all(card, 12, LV_PART_MAIN);
lv_obj_set_style_pad_row(card, 10, LV_PART_MAIN);
lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

// Section title inside the card (icon + text)
lv_obj_t* sectionTitle = lv_label_create(card);
lv_label_set_text(sectionTitle, LV_SYMBOL_SETTINGS " Display");
lv_obj_set_style_text_color(sectionTitle, lv_color_hex(0x89B4FA), LV_PART_MAIN);
lv_obj_set_style_text_font(sectionTitle, &lv_font_montserrat_14, LV_PART_MAIN);
```

`LV_SIZE_CONTENT` makes the height expand to fit all children automatically. Always
prefer this over a fixed pixel height so the card adapts when you add or remove rows.

### 7.11 Row with key and value

A common pattern for a settings row: label on the left, control on the right.
The row is a transparent container with no border. Children are pinned to each side
with `lv_obj_align`.

```cpp
// Transparent row container - holds label on left and a control on right
lv_obj_t* row = lv_obj_create(card);
lv_obj_set_size(row, lv_pct(100), 32);
lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

// Left side: descriptive label
lv_obj_t* lbl = lv_label_create(row);
lv_label_set_text(lbl, "Dark mode");
lv_obj_set_style_text_color(lbl, lv_color_hex(0xCDD6F4), LV_PART_MAIN);
lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

// Right side: the interactive control (switch in this example)
lv_obj_t* sw = lv_switch_create(row);
lv_obj_set_style_bg_color(sw, lv_color_hex(0x313244), LV_PART_MAIN);
lv_obj_set_style_bg_color(sw, lv_color_hex(0x89B4FA), LV_PART_MAIN | LV_STATE_CHECKED);
lv_obj_set_style_bg_color(sw, lv_color_hex(0xCDD6F4), LV_PART_KNOB);
lv_obj_add_state(sw, LV_STATE_CHECKED);
lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);

lv_obj_add_event_cb(sw, [](lv_event_t* e) {
    bool on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    (void)on;
}, LV_EVENT_VALUE_CHANGED, NULL);
```

---

## 8. Layout systems

### 8.1 Manual positioning

`lv_obj_set_pos(obj, x, y)` places the object at pixel coordinates relative to its
parent's top-left corner, excluding the parent's padding.

```cpp
lv_obj_set_pos(btn, 10, 50);
```

Use this when you need exact positions, for example when building the home screen grid
where every icon position is calculated with arithmetic.

### 8.2 Alignment helpers

`lv_obj_align` positions the object relative to its parent without explicit
coordinates. Use this inside containers and cards where you want objects anchored to
edges or the center.

```cpp
lv_obj_align(obj, LV_ALIGN_CENTER, 0, 0);       // centered, no offset
lv_obj_align(obj, LV_ALIGN_LEFT_MID, 8, 0);     // left-center, 8px from left edge
lv_obj_align(obj, LV_ALIGN_RIGHT_MID, -8, 0);   // right-center, 8px from right edge
lv_obj_align(obj, LV_ALIGN_TOP_MID, 0, 8);      // top-center, 8px from top edge
lv_obj_align(obj, LV_ALIGN_BOTTOM_MID, 0, -4);  // bottom-center, 4px from bottom edge
lv_obj_align(obj, LV_ALIGN_TOP_LEFT, 8, 8);     // top-left corner with offset
lv_obj_align(obj, LV_ALIGN_TOP_RIGHT, -8, 8);   // top-right corner with offset
```

To align relative to a sibling instead of the parent:

```cpp
lv_obj_align_to(valueLabel, keyLabel, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
```

`OUT_RIGHT_MID` means the left edge of `valueLabel` is placed to the right of the
right edge of `keyLabel` with an 8px gap.

Important: if either object is repositioned after calling `lv_obj_align_to`, the
alignment is not automatically updated. Call it again after the move.

### 8.3 Flex layout

Flex arranges children automatically without calculating positions. Set the layout on
the container and it applies to all direct children.

```cpp
lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);      // stack vertically
lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);         // arrange horizontally
lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW_WRAP);    // horizontal with wrap
lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN_WRAP); // vertical with wrap
```

Control how children are distributed along the main axis and aligned on the cross axis:

```cpp
lv_obj_set_flex_align(
    container,
    LV_FLEX_ALIGN_START,    // main axis: pack to start
    LV_FLEX_ALIGN_CENTER,   // cross axis: center each item
    LV_FLEX_ALIGN_CENTER    // track alignment when wrapping
);
```

Flex align values: `LV_FLEX_ALIGN_START`, `LV_FLEX_ALIGN_END`, `LV_FLEX_ALIGN_CENTER`,
`LV_FLEX_ALIGN_SPACE_EVENLY`, `LV_FLEX_ALIGN_SPACE_AROUND`, `LV_FLEX_ALIGN_SPACE_BETWEEN`.

To make a specific child grow and fill the remaining space on the main axis:

```cpp
lv_obj_set_flex_grow(child, 1);
```

Flex gaps are controlled through `pad_row` (vertical gap in column flow) and
`pad_column` (horizontal gap in row flow) set on the container:

```cpp
lv_obj_set_style_pad_row(container, 8, LV_PART_MAIN);
lv_obj_set_style_pad_column(container, 8, LV_PART_MAIN);
```

---

## 9. Styling reference

### 9.1 Colors

All colors are 24-bit hex values passed through `lv_color_hex()`.

```cpp
lv_obj_set_style_bg_color(obj, lv_color_hex(0x1E1E2E), LV_PART_MAIN);
lv_obj_set_style_text_color(obj, lv_color_hex(0xCDD6F4), LV_PART_MAIN);
lv_obj_set_style_border_color(obj, lv_color_hex(0x45475A), LV_PART_MAIN);
lv_obj_set_style_shadow_color(obj, lv_color_hex(0x000000), LV_PART_MAIN);
lv_obj_set_style_outline_color(obj, lv_color_hex(0x89B4FA), LV_PART_MAIN);
```

### 9.2 Size and position

```cpp
lv_obj_set_size(obj, 200, 40);            // width and height in pixels
lv_obj_set_width(obj, lv_pct(100));       // 100% of parent inner width
lv_obj_set_height(obj, LV_SIZE_CONTENT);  // grow to fit children
lv_obj_set_pos(obj, 10, 50);              // x, y from parent top-left
```

### 9.3 Fonts

Only fonts enabled in `lv_conf.h` can be used. The project enables:
`lv_font_montserrat_10`, `lv_font_montserrat_14`, `lv_font_montserrat_18`.

```cpp
lv_obj_set_style_text_font(obj, &lv_font_montserrat_10, LV_PART_MAIN); // small
lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, LV_PART_MAIN); // default
lv_obj_set_style_text_font(obj, &lv_font_montserrat_18, LV_PART_MAIN); // large
```

To add more font sizes, enable them in `lv_conf.h` and rebuild:

```c
#define LV_FONT_MONTSERRAT_12 1
```

Each enabled font size adds to flash usage, so only enable what is needed.

### 9.4 Border, radius and padding

```cpp
lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);           // no border
lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN);           // 1px border
lv_obj_set_style_border_color(obj, lv_color_hex(0x45475A), LV_PART_MAIN);
lv_obj_set_style_radius(obj, 0, LV_PART_MAIN);                 // sharp corners
lv_obj_set_style_radius(obj, 8, LV_PART_MAIN);                 // rounded corners
lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_MAIN);  // pill or circle

lv_obj_set_style_pad_all(obj, 12, LV_PART_MAIN);    // all four sides equally
lv_obj_set_style_pad_top(obj, 8, LV_PART_MAIN);     // top only
lv_obj_set_style_pad_bottom(obj, 8, LV_PART_MAIN);  // bottom only
lv_obj_set_style_pad_left(obj, 12, LV_PART_MAIN);   // left only
lv_obj_set_style_pad_right(obj, 12, LV_PART_MAIN);  // right only
lv_obj_set_style_pad_row(obj, 8, LV_PART_MAIN);     // gap between flex rows
lv_obj_set_style_pad_column(obj, 8, LV_PART_MAIN);  // gap between flex columns
```

### 9.5 Shadow

```cpp
lv_obj_set_style_shadow_width(obj, 12, LV_PART_MAIN);          // spread in pixels
lv_obj_set_style_shadow_color(obj, lv_color_hex(0x000000), LV_PART_MAIN);
lv_obj_set_style_shadow_opa(obj, LV_OPA_30, LV_PART_MAIN);     // 30% opacity
lv_obj_set_style_shadow_ofs_x(obj, 0, LV_PART_MAIN);           // horizontal offset
lv_obj_set_style_shadow_ofs_y(obj, 4, LV_PART_MAIN);           // vertical offset
```

### 9.6 Opacity

To make an entire object semi-transparent:

```cpp
lv_obj_set_style_opa(obj, LV_OPA_50, LV_PART_MAIN);        // 50% for everything
lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN); // transparent background only
```

Opacity constants: `LV_OPA_TRANSP` (0%), `LV_OPA_10`, `LV_OPA_20`, `LV_OPA_30`,
`LV_OPA_40`, `LV_OPA_50`, `LV_OPA_60`, `LV_OPA_70`, `LV_OPA_80`, `LV_OPA_90`,
`LV_OPA_COVER` (100%).

### 9.7 States

Styles can target specific interactive states by OR-ing the part with a state constant.

```cpp
// Normal state (default, no state constant needed)
lv_obj_set_style_bg_color(obj, lv_color_hex(0x313244), LV_PART_MAIN);

// Pressed state (applied while the user holds the finger down)
lv_obj_set_style_bg_color(obj, lv_color_hex(0x45475A), LV_STATE_PRESSED);

// Checked state (switches, checkboxes)
lv_obj_set_style_bg_color(obj, lv_color_hex(0x89B4FA), LV_STATE_CHECKED);

// Focused state (encoder or keyboard navigation)
lv_obj_set_style_border_color(obj, lv_color_hex(0x89B4FA), LV_STATE_FOCUSED);

// Disabled state (grayed out, non-interactive)
lv_obj_set_style_opa(obj, LV_OPA_50, LV_STATE_DISABLED);
```

To combine a part and a state:

```cpp
lv_obj_set_style_bg_color(sw, lv_color_hex(0x89B4FA), LV_PART_MAIN | LV_STATE_CHECKED);
lv_obj_set_style_bg_color(cb, lv_color_hex(0x89B4FA), LV_PART_INDICATOR | LV_STATE_CHECKED);
```

To set and query states programmatically:

```cpp
lv_obj_add_state(obj, LV_STATE_CHECKED);
lv_obj_clear_state(obj, LV_STATE_CHECKED);
lv_obj_add_state(obj, LV_STATE_DISABLED);
bool isChecked = lv_obj_has_state(obj, LV_STATE_CHECKED);
```

---

## 10. Events and callbacks

LVGL delivers interactions through the event system. You register a callback function
for a specific event code on an object.

```cpp
lv_obj_add_event_cb(obj, callbackFunction, LV_EVENT_CLICKED, userDataPtr);
```

Common event codes:

| Event | When it fires |
|---|---|
| `LV_EVENT_CLICKED` | Touch released on the object |
| `LV_EVENT_PRESSED` | Touch started on the object |
| `LV_EVENT_RELEASED` | Touch lifted after pressing the object |
| `LV_EVENT_VALUE_CHANGED` | Value changed (switch, slider, dropdown, checkbox) |
| `LV_EVENT_FOCUSED` | Object gained focus |
| `LV_EVENT_DEFOCUSED` | Object lost focus |
| `LV_EVENT_LONG_PRESSED` | Held for approximately 400ms |

Callback signatures must match `void name(lv_event_t* e)`. Lambda functions are fine
for short callbacks. For callbacks that need access to screen member variables, pass
`this` as user data and cast it back inside the callback:

```cpp
// When registering (inside a method of the class)
lv_obj_add_event_cb(slider, [](lv_event_t* e) {
    SettingsScreen* self = (SettingsScreen*)lv_event_get_user_data(e);
    int32_t val = lv_slider_get_value(lv_event_get_target(e));
    lv_label_set_text_fmt(self->_brightnessLabel, "%d", val);
}, LV_EVENT_VALUE_CHANGED, this);
```

Static lambdas (using `static auto cb = [](lv_event_t* e) {...}`) are used when the
callback does not need access to instance data, as seen in `home_screen.h`. A static
lambda works because `lv_event_cb_t` is a plain function pointer, which only
stateless (non-capturing) lambdas can satisfy.

---

## 11. Navigation between screens

Use `ScreenManager::getInstance()` from anywhere inside a screen class.

```cpp
// Push a new screen on top (user can go back with the back button)
ScreenManager::getInstance().navigateTo(new TargetScreen());

// Push with a specific animation and duration in milliseconds
ScreenManager::getInstance().navigateTo(
    new TargetScreen(), LV_SCR_LOAD_ANIM_MOVE_LEFT, 200);

// Go back to the previous screen (destroys current, resumes previous)
ScreenManager::getInstance().goBack();

// Jump directly to the root screen, clearing the entire stack
ScreenManager::getInstance().goHome();

// Replace current screen without adding to history (no back navigation possible)
ScreenManager::getInstance().replaceCurrent(new TargetScreen());

// Check if there is a screen to go back to
if (ScreenManager::getInstance().canGoBack()) { /* ... */ }
```

Available animation types:

| Constant | Effect |
|---|---|
| `LV_SCR_LOAD_ANIM_NONE` | Instant swap, no animation |
| `LV_SCR_LOAD_ANIM_FADE_ON` | New screen fades in |
| `LV_SCR_LOAD_ANIM_FADE_OUT` | Current screen fades out |
| `LV_SCR_LOAD_ANIM_MOVE_LEFT` | New screen slides in from the right |
| `LV_SCR_LOAD_ANIM_MOVE_RIGHT` | New screen slides in from the left |
| `LV_SCR_LOAD_ANIM_MOVE_TOP` | New screen slides in from the bottom |
| `LV_SCR_LOAD_ANIM_MOVE_BOTTOM` | New screen slides in from the top |
| `LV_SCR_LOAD_ANIM_OVER_LEFT` | New screen slides over from the right |
| `LV_SCR_LOAD_ANIM_OVER_RIGHT` | New screen slides over from the left |

The convention in this project: navigate forward with `MOVE_LEFT`, go back with
`MOVE_RIGHT` (these are the defaults already set in `ScreenManager`).

---

## 12. Registering the screen in main.cpp

Open `main.cpp` and make two additions.

**Step 1.** Include the new header at the top with the other screen includes:

```cpp
#include "ui/screens/home_screen.h"
#include "ui/screens/about_screen.h"
#include "ui/screens/settings_screen.h"   // add this
```

**Step 2.** Add a launcher function below the existing launchers:

```cpp
void launchAbout() {
    ScreenManager::getInstance().navigateTo(
        new AboutScreen(), LV_SCR_LOAD_ANIM_FADE_ON);
}

// Add this block
void launchSettings() {
    ScreenManager::getInstance().navigateTo(
        new SettingsScreen(), LV_SCR_LOAD_ANIM_MOVE_LEFT);
}
```

That is all that changes in `main.cpp`. The launcher is a plain free function that the
home screen can call via `extern` without including the screen header directly, which
would create a circular dependency.

---

## 13. Adding the app icon in home_screen.h

Open `home_screen.h` and make two changes inside `_buildAppGrid()`.

**Step 1.** Add a forward declaration at the top of the file if not already present:

```cpp
// Forward declarations for avoiding circular includes
class AboutScreen;
class SettingsScreen;   // add this
```

**Step 2.** Inside `_buildAppGrid()`, find the static callback for Settings and
replace the placeholder comment with the actual launcher call:

```cpp
static auto cbSettings = [](lv_event_t* e) {
    extern void launchSettings();
    launchSettings();
};
```

The apps array and the rendering loop do not change. The icon symbol
`LV_SYMBOL_SETTINGS` and the label `"Settings"` are already in place at index 0.

If you are adding a brand new slot that does not yet exist in the grid, add a new
entry to the `apps[]` array and increase the loop count:

```cpp
const AppIcon apps[] = {
    { LV_SYMBOL_SETTINGS, "Settings", cbSettings },
    { LV_SYMBOL_WIFI,     "WiFi",     nullptr     },
    { LV_SYMBOL_SD_CARD,  "Files",    nullptr     },
    { LV_SYMBOL_BELL,     "Alerts",   nullptr     },
    { LV_SYMBOL_PLAY,     "Games",    nullptr     },
    { LV_SYMBOL_LIST,     "About",    cbAbout     },
    { LV_SYMBOL_EDIT,     "Notes",    cbNotes     }, // new entry
};

for (int i = 0; i < 7; i++) { // was 6, now 7
```

The grid layout arithmetic recalculates positions automatically based on `COLS` and
`ICON_SIZE`. If you want a different number of columns, change the `COLS` constant and
`START_X` will adjust accordingly.

---

## 14. Available LVGL symbols

LVGL provides built-in icon glyphs through the Montserrat font. Use them anywhere a
string is accepted. A font size of 14 or 18 is recommended.

| Constant | Icon |
|---|---|
| `LV_SYMBOL_SETTINGS` | Gear |
| `LV_SYMBOL_WIFI` | WiFi waves |
| `LV_SYMBOL_SD_CARD` | SD card |
| `LV_SYMBOL_BELL` | Bell |
| `LV_SYMBOL_PLAY` | Play triangle |
| `LV_SYMBOL_LIST` | List lines |
| `LV_SYMBOL_EDIT` | Pencil |
| `LV_SYMBOL_LEFT` | Left arrow |
| `LV_SYMBOL_RIGHT` | Right arrow |
| `LV_SYMBOL_UP` | Up arrow |
| `LV_SYMBOL_DOWN` | Down arrow |
| `LV_SYMBOL_HOME` | House |
| `LV_SYMBOL_CLOSE` | X mark |
| `LV_SYMBOL_OK` | Checkmark |
| `LV_SYMBOL_PLUS` | Plus sign |
| `LV_SYMBOL_MINUS` | Minus sign |
| `LV_SYMBOL_TRASH` | Trash bin |
| `LV_SYMBOL_SAVE` | Floppy disk |
| `LV_SYMBOL_FILE` | File page |
| `LV_SYMBOL_DIRECTORY` | Folder |
| `LV_SYMBOL_BLUETOOTH` | Bluetooth mark |
| `LV_SYMBOL_GPS` | Location pin |
| `LV_SYMBOL_BATTERY_FULL` | Full battery |
| `LV_SYMBOL_BATTERY_EMPTY` | Empty battery |
| `LV_SYMBOL_POWER` | Power circle |
| `LV_SYMBOL_REFRESH` | Circular arrows |
| `LV_SYMBOL_LOOP` | Loop arrows |
| `LV_SYMBOL_SHUFFLE` | Shuffle arrows |
| `LV_SYMBOL_PAUSE` | Pause bars |
| `LV_SYMBOL_STOP` | Stop square |
| `LV_SYMBOL_AUDIO` | Audio waves |
| `LV_SYMBOL_MUTE` | Muted speaker |
| `LV_SYMBOL_VOLUME_MAX` | Speaker with waves |
| `LV_SYMBOL_IMAGE` | Picture frame |
| `LV_SYMBOL_KEYBOARD` | Keyboard outline |
| `LV_SYMBOL_WARNING` | Exclamation triangle |
| `LV_SYMBOL_CHARGE` | Lightning bolt |
| `LV_SYMBOL_CALL` | Phone handset |
| `LV_SYMBOL_ENVELOPE` | Envelope |
| `LV_SYMBOL_DOWNLOAD` | Down arrow with line |
| `LV_SYMBOL_UPLOAD` | Up arrow with line |
| `LV_SYMBOL_EYE_OPEN` | Open eye |
| `LV_SYMBOL_EYE_CLOSE` | Closed eye |
| `LV_SYMBOL_LOCK` | Padlock |
| `LV_SYMBOL_UNLOCK` | Open padlock |
| `LV_SYMBOL_BACKSPACE` | Backspace arrow |
| `LV_SYMBOL_NEW_LINE` | Enter arrow |

You can concatenate symbols with text in a single string:

```cpp
lv_label_set_text(lbl, LV_SYMBOL_WIFI " WiFi Status");
lv_label_set_text(lbl, LV_SYMBOL_OK " Connected");
lv_label_set_text(lbl, LV_SYMBOL_WARNING " No signal");
```

---

## 15. Complete example - settings_screen.h

This is the full `settings_screen.h` file. It uses every widget and pattern described
in this document.

```cpp
#pragma once
#include <esp_heap_caps.h>
#include "screen_base.h"
#include "../screen_manager.h"

// SettingsScreen - System settings with all widget types demonstrated
class SettingsScreen : public ScreenBase {
public:
    void onCreate() override {
        _screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(_screen, lv_color_hex(0x121212), LV_PART_MAIN);

        _buildHeader();
        _buildContent();
    }

    void onResume() override {
        _refreshSystemStats();
    }

    void onDestroy() override {
        if (_screen) {
            lv_obj_del(_screen);
            _screen = nullptr;
        }
    }

private:
    // Member pointers for widgets that are updated at runtime
    lv_obj_t* _brightnessLabel = nullptr;
    lv_obj_t* _ramBar = nullptr;
    lv_obj_t* _ramLabel = nullptr;

    // Header with back button and screen title
    void _buildHeader() {
        lv_obj_t* header = lv_obj_create(_screen);
        lv_obj_set_size(header, 240, 40);
        lv_obj_set_pos(header, 0, 0);
        lv_obj_set_style_bg_color(header, lv_color_hex(0x1E1E2E), LV_PART_MAIN);
        lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(header, 0, LV_PART_MAIN);
        lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* backBtn = lv_btn_create(header);
        lv_obj_set_size(backBtn, 36, 28);
        lv_obj_align(backBtn, LV_ALIGN_LEFT_MID, 4, 0);
        lv_obj_set_style_bg_color(backBtn, lv_color_hex(0x313244), LV_PART_MAIN);
        lv_obj_set_style_radius(backBtn, 8, LV_PART_MAIN);
        lv_obj_add_event_cb(backBtn, [](lv_event_t* e) {
            ScreenManager::getInstance().goBack();
        }, LV_EVENT_CLICKED, NULL);

        lv_obj_t* backIco = lv_label_create(backBtn);
        lv_label_set_text(backIco, LV_SYMBOL_LEFT);
        lv_obj_set_style_text_color(backIco, lv_color_hex(0x89B4FA), LV_PART_MAIN);
        lv_obj_center(backIco);

        lv_obj_t* title = lv_label_create(header);
        lv_label_set_text(title, "Settings");
        lv_obj_set_style_text_color(title, lv_color_hex(0xCDD6F4), LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
    }

    // Scrollable area that holds all section cards
    void _buildContent() {
        lv_obj_t* scroll = lv_obj_create(_screen);
        lv_obj_set_size(scroll, 240, 280);
        lv_obj_set_pos(scroll, 0, 40);
        lv_obj_set_style_bg_color(scroll, lv_color_hex(0x121212), LV_PART_MAIN);
        lv_obj_set_style_border_width(scroll, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(scroll, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(scroll, 8, LV_PART_MAIN);
        lv_obj_set_style_pad_row(scroll, 8, LV_PART_MAIN);
        lv_obj_set_flex_flow(scroll, LV_FLEX_FLOW_COLUMN);

        _buildDisplaySection(scroll);
        _buildConnectivitySection(scroll);
        _buildSystemSection(scroll);
        _buildDangerSection(scroll);
    }

    // Section: Display
    // Contains: slider with live value label, switch
    void _buildDisplaySection(lv_obj_t* parent) {
        lv_obj_t* card = _makeCard(parent);

        // Section title with icon
        lv_obj_t* sectionTitle = lv_label_create(card);
        lv_label_set_text(sectionTitle, LV_SYMBOL_IMAGE " Display");
        lv_obj_set_style_text_color(sectionTitle, lv_color_hex(0x89B4FA), LV_PART_MAIN);
        lv_obj_set_style_text_font(sectionTitle, &lv_font_montserrat_14, LV_PART_MAIN);

        _makeSeparator(card);

        // Brightness row: label on left, current value on right
        lv_obj_t* rowBrightness = _makeRow(card, "Brightness");
        _brightnessLabel = lv_label_create(rowBrightness);
        lv_label_set_text(_brightnessLabel, "128");
        lv_obj_set_style_text_color(_brightnessLabel, lv_color_hex(0x585B70), LV_PART_MAIN);
        lv_obj_set_style_text_font(_brightnessLabel, &lv_font_montserrat_10, LV_PART_MAIN);
        lv_obj_align(_brightnessLabel, LV_ALIGN_RIGHT_MID, 0, 0);

        // Slider - full width, updates _brightnessLabel on drag
        // Uses this pointer as user_data to access the member variable from the callback
        lv_obj_t* slider = lv_slider_create(card);
        lv_obj_set_width(slider, lv_pct(100));
        lv_slider_set_range(slider, 0, 255);
        lv_slider_set_value(slider, 128, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(slider, lv_color_hex(0x313244), LV_PART_MAIN);
        lv_obj_set_style_bg_color(slider, lv_color_hex(0x89B4FA), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(slider, lv_color_hex(0xCDD6F4), LV_PART_KNOB);
        lv_obj_add_event_cb(slider, [](lv_event_t* e) {
            SettingsScreen* self = (SettingsScreen*)lv_event_get_user_data(e);
            int32_t val = lv_slider_get_value(lv_event_get_target(e));
            lv_label_set_text_fmt(self->_brightnessLabel, "%d", val);
        }, LV_EVENT_VALUE_CHANGED, this);

        _makeSeparator(card);

        // Dark mode row: label on left, switch on right
        lv_obj_t* rowDark = _makeRow(card, "Dark mode");
        lv_obj_t* swDark = lv_switch_create(rowDark);
        lv_obj_set_style_bg_color(swDark, lv_color_hex(0x313244), LV_PART_MAIN);
        lv_obj_set_style_bg_color(swDark, lv_color_hex(0x89B4FA), LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(swDark, lv_color_hex(0xCDD6F4), LV_PART_KNOB);
        lv_obj_add_state(swDark, LV_STATE_CHECKED);
        lv_obj_align(swDark, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_add_event_cb(swDark, [](lv_event_t* e) {
            bool on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
            (void)on;
        }, LV_EVENT_VALUE_CHANGED, NULL);
    }

    // Section: Connectivity
    // Contains: switch, dropdown, text area, checkbox
    void _buildConnectivitySection(lv_obj_t* parent) {
        lv_obj_t* card = _makeCard(parent);

        lv_obj_t* sectionTitle = lv_label_create(card);
        lv_label_set_text(sectionTitle, LV_SYMBOL_WIFI " Connectivity");
        lv_obj_set_style_text_color(sectionTitle, lv_color_hex(0x89B4FA), LV_PART_MAIN);
        lv_obj_set_style_text_font(sectionTitle, &lv_font_montserrat_14, LV_PART_MAIN);

        _makeSeparator(card);

        // WiFi enabled toggle
        lv_obj_t* rowWifi = _makeRow(card, "WiFi enabled");
        lv_obj_t* swWifi = lv_switch_create(rowWifi);
        lv_obj_set_style_bg_color(swWifi, lv_color_hex(0x313244), LV_PART_MAIN);
        lv_obj_set_style_bg_color(swWifi, lv_color_hex(0x89B4FA), LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(swWifi, lv_color_hex(0xCDD6F4), LV_PART_KNOB);
        lv_obj_add_state(swWifi, LV_STATE_CHECKED);
        lv_obj_align(swWifi, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_add_event_cb(swWifi, [](lv_event_t* e) {
            bool on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
            (void)on;
        }, LV_EVENT_VALUE_CHANGED, NULL);

        _makeSeparator(card);

        // Band dropdown - compact selector that opens a list when tapped
        lv_obj_t* rowBand = _makeRow(card, "Band");
        lv_obj_t* dd = lv_dropdown_create(rowBand);
        lv_dropdown_set_options(dd, "Auto\n2.4 GHz\n5 GHz");
        lv_dropdown_set_selected(dd, 0);
        lv_obj_set_width(dd, 110);
        lv_obj_set_style_bg_color(dd, lv_color_hex(0x313244), LV_PART_MAIN);
        lv_obj_set_style_text_color(dd, lv_color_hex(0xCDD6F4), LV_PART_MAIN);
        lv_obj_set_style_border_width(dd, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(dd, 8, LV_PART_MAIN);
        lv_obj_align(dd, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_add_event_cb(dd, [](lv_event_t* e) {
            uint16_t idx = lv_dropdown_get_selected(lv_event_get_target(e));
            (void)idx;
        }, LV_EVENT_VALUE_CHANGED, NULL);

        _makeSeparator(card);

        // Hostname label above the text area
        lv_obj_t* lblHost = lv_label_create(card);
        lv_label_set_text(lblHost, "Hostname");
        lv_obj_set_style_text_color(lblHost, lv_color_hex(0xCDD6F4), LV_PART_MAIN);
        lv_obj_set_style_text_font(lblHost, &lv_font_montserrat_14, LV_PART_MAIN);

        // Text area - single-line input for a device hostname
        // Shows a placeholder when empty and highlights the border when focused
        lv_obj_t* ta = lv_textarea_create(card);
        lv_obj_set_width(ta, lv_pct(100));
        lv_obj_set_height(ta, 38);
        lv_textarea_set_one_line(ta, true);
        lv_textarea_set_placeholder_text(ta, "pickle-os.local");
        lv_textarea_set_max_length(ta, 64);
        lv_obj_set_style_bg_color(ta, lv_color_hex(0x313244), LV_PART_MAIN);
        lv_obj_set_style_text_color(ta, lv_color_hex(0xCDD6F4), LV_PART_MAIN);
        lv_obj_set_style_border_color(ta, lv_color_hex(0x45475A), LV_PART_MAIN);
        lv_obj_set_style_border_width(ta, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(ta, lv_color_hex(0x89B4FA), LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_radius(ta, 8, LV_PART_MAIN);

        _makeSeparator(card);

        // Checkbox - boolean option with an inline text label
        lv_obj_t* cb = lv_checkbox_create(card);
        lv_checkbox_set_text(cb, "Auto-reconnect on boot");
        lv_obj_set_style_text_color(cb, lv_color_hex(0xCDD6F4), LV_PART_MAIN);
        lv_obj_set_style_text_font(cb, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_bg_color(cb, lv_color_hex(0x313244), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(cb, lv_color_hex(0x89B4FA), LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_border_color(cb, lv_color_hex(0x45475A), LV_PART_INDICATOR);
        lv_obj_set_style_border_width(cb, 1, LV_PART_INDICATOR);
        lv_obj_add_state(cb, LV_STATE_CHECKED);
        lv_obj_add_event_cb(cb, [](lv_event_t* e) {
            bool checked = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
            (void)checked;
        }, LV_EVENT_VALUE_CHANGED, NULL);
    }

    // Section: System
    // Contains: bar (progress bar) with dynamic label, static info rows
    void _buildSystemSection(lv_obj_t* parent) {
        lv_obj_t* card = _makeCard(parent);

        lv_obj_t* sectionTitle = lv_label_create(card);
        lv_label_set_text(sectionTitle, LV_SYMBOL_SETTINGS " System");
        lv_obj_set_style_text_color(sectionTitle, lv_color_hex(0x89B4FA), LV_PART_MAIN);
        lv_obj_set_style_text_font(sectionTitle, &lv_font_montserrat_14, LV_PART_MAIN);

        _makeSeparator(card);

        // Free RAM row: label on left, KB value on right
        // Both the bar and the label are stored as members to be updated in onResume()
        lv_obj_t* rowRam = _makeRow(card, "Free RAM");
        _ramLabel = lv_label_create(rowRam);
        lv_label_set_text(_ramLabel, "-- KB");
        lv_obj_set_style_text_color(_ramLabel, lv_color_hex(0x585B70), LV_PART_MAIN);
        lv_obj_set_style_text_font(_ramLabel, &lv_font_montserrat_10, LV_PART_MAIN);
        lv_obj_align(_ramLabel, LV_ALIGN_RIGHT_MID, 0, 0);

        // Bar showing RAM usage as a filled indicator
        _ramBar = lv_bar_create(card);
        lv_obj_set_width(_ramBar, lv_pct(100));
        lv_obj_set_height(_ramBar, 12);
        lv_bar_set_range(_ramBar, 0, 320);      // ESP32 has ~320 KB total SRAM
        lv_bar_set_value(_ramBar, 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(_ramBar, lv_color_hex(0x313244), LV_PART_MAIN);
        lv_obj_set_style_bg_color(_ramBar, lv_color_hex(0x89B4FA), LV_PART_INDICATOR);
        lv_obj_set_style_radius(_ramBar, 6, LV_PART_MAIN);
        lv_obj_set_style_radius(_ramBar, 6, LV_PART_INDICATOR);

        _makeSeparator(card);

        // Static info rows: key on left, value on right, no interactive control
        _makeInfoRow(card, "Firmware", "v0.1.0");
        _makeInfoRow(card, "LVGL", "8.3");
        _makeInfoRow(card, "Board", "ESP32-2432S028");
    }

    // Section: Danger zone
    // Contains: destructive button with red styling
    void _buildDangerSection(lv_obj_t* parent) {
        lv_obj_t* card = _makeCard(parent);

        // Section title in danger color to signal destructive actions
        lv_obj_t* sectionTitle = lv_label_create(card);
        lv_label_set_text(sectionTitle, LV_SYMBOL_WARNING " Danger zone");
        lv_obj_set_style_text_color(sectionTitle, lv_color_hex(0xF38BA8), LV_PART_MAIN);
        lv_obj_set_style_text_font(sectionTitle, &lv_font_montserrat_14, LV_PART_MAIN);

        _makeSeparator(card);

        // Destructive button - same structure as a normal button, danger colors only
        lv_obj_t* btnReset = lv_btn_create(card);
        lv_obj_set_width(btnReset, lv_pct(100));
        lv_obj_set_height(btnReset, 36);
        lv_obj_set_style_bg_color(btnReset, lv_color_hex(0x3D1E25), LV_PART_MAIN);
        lv_obj_set_style_bg_color(btnReset, lv_color_hex(0x5C2030), LV_STATE_PRESSED);
        lv_obj_set_style_radius(btnReset, 8, LV_PART_MAIN);
        lv_obj_add_event_cb(btnReset, [](lv_event_t* e) {
            // Trigger factory reset here when implemented
        }, LV_EVENT_CLICKED, NULL);

        lv_obj_t* btnLbl = lv_label_create(btnReset);
        lv_label_set_text(btnLbl, LV_SYMBOL_TRASH " Reset to factory defaults");
        lv_obj_set_style_text_color(btnLbl, lv_color_hex(0xF38BA8), LV_PART_MAIN);
        lv_obj_set_style_text_font(btnLbl, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_center(btnLbl);
    }

    // Called from onResume() to refresh dynamic values
    void _refreshSystemStats() {
        if (!_ramBar || !_ramLabel) return;
        uint32_t freeKb = esp_get_free_heap_size() / 1024;
        lv_bar_set_value(_ramBar, (int32_t)freeKb, LV_ANIM_ON);
        lv_label_set_text_fmt(_ramLabel, "%d KB", freeKb);
    }

    // Helper: creates a rounded section card with flex column layout
    lv_obj_t* _makeCard(lv_obj_t* parent) {
        lv_obj_t* card = lv_obj_create(parent);
        lv_obj_set_width(card, lv_pct(100));
        lv_obj_set_height(card, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x1E1E2E), LV_PART_MAIN);
        lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(card, 12, LV_PART_MAIN);
        lv_obj_set_style_pad_all(card, 12, LV_PART_MAIN);
        lv_obj_set_style_pad_row(card, 10, LV_PART_MAIN);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        return card;
    }

    // Helper: creates a transparent row with a left-aligned label
    // Returns the row so the caller can add a right-side control
    lv_obj_t* _makeRow(lv_obj_t* parent, const char* labelText) {
        lv_obj_t* row = lv_obj_create(parent);
        lv_obj_set_size(row, lv_pct(100), 32);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lbl = lv_label_create(row);
        lv_label_set_text(lbl, labelText);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xCDD6F4), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

        return row;
    }

    // Helper: creates a static key/value display row with no interactive control
    void _makeInfoRow(lv_obj_t* parent, const char* key, const char* val) {
        lv_obj_t* row = lv_obj_create(parent);
        lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* keyLbl = lv_label_create(row);
        lv_label_set_text(keyLbl, key);
        lv_obj_set_style_text_color(keyLbl, lv_color_hex(0x585B70), LV_PART_MAIN);
        lv_obj_set_style_text_font(keyLbl, &lv_font_montserrat_10, LV_PART_MAIN);
        lv_obj_align(keyLbl, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t* valLbl = lv_label_create(row);
        lv_label_set_text(valLbl, val);
        lv_obj_set_style_text_color(valLbl, lv_color_hex(0xCDD6F4), LV_PART_MAIN);
        lv_obj_set_style_text_font(valLbl, &lv_font_montserrat_10, LV_PART_MAIN);
        lv_obj_align(valLbl, LV_ALIGN_RIGHT_MID, 0, 0);
    }

    // Helper: creates a 1px horizontal separator line
    void _makeSeparator(lv_obj_t* parent) {
        lv_obj_t* sep = lv_obj_create(parent);
        lv_obj_set_size(sep, lv_pct(100), 1);
        lv_obj_set_style_bg_color(sep, lv_color_hex(0x45475A), LV_PART_MAIN);
        lv_obj_set_style_border_width(sep, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(sep, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(sep, 0, LV_PART_MAIN);
    }
};
```