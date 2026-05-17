#pragma once
#include <esp_heap_caps.h>
#include "screen_base.h"
#include "../screen_manager.h"
#include "../keyboard_overlay.h"
#include "../theme/theme.h"

// SettingsScreen - System settings with all widget types demonstrated
class SettingsScreen : public ScreenBase {
public:
    void onCreate() override {
        _screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(_screen, gTheme->background, LV_PART_MAIN);

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
    lv_obj_t* _scroll = nullptr;
    lv_obj_t* _taHostname = nullptr;
    KeyboardOverlay _kb;

    // Header with back button and screen title
    void _buildHeader() {
        lv_obj_t* header = lv_obj_create(_screen);
        lv_obj_set_size(header, 240, 40);
        lv_obj_set_pos(header, 0, 0);
        lv_obj_set_style_bg_color(header, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(header, 0, LV_PART_MAIN);
        lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* backBtn = lv_btn_create(header);
        lv_obj_set_size(backBtn, 36, 28);
        lv_obj_align(backBtn, LV_ALIGN_LEFT_MID, 4, 0);
        lv_obj_set_style_bg_color(backBtn, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_radius(backBtn, 8, LV_PART_MAIN);
        lv_obj_add_event_cb(backBtn, [](lv_event_t* e) {
            ScreenManager::getInstance().goBack();
        }, LV_EVENT_CLICKED, NULL);

        lv_obj_t* backIco = lv_label_create(backBtn);
        lv_label_set_text(backIco, LV_SYMBOL_LEFT);
        lv_obj_set_style_text_color(backIco, gTheme->primary, LV_PART_MAIN);
        lv_obj_center(backIco);

        lv_obj_t* title = lv_label_create(header);
        lv_label_set_text(title, "Settings");
        lv_obj_set_style_text_color(title, gTheme->textDark, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
    }

    // Scrollable area that holds all section cards
    void _buildContent() {
        _scroll = lv_obj_create(_screen);
        lv_obj_set_size(_scroll, 240, 280);
        lv_obj_set_pos(_scroll, 0, 40);
        lv_obj_set_style_bg_color(_scroll, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_border_width(_scroll, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(_scroll, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_scroll, 8, LV_PART_MAIN);
        lv_obj_set_style_pad_row(_scroll, 8, LV_PART_MAIN);
        lv_obj_set_flex_flow(_scroll, LV_FLEX_FLOW_COLUMN);

        _buildDisplaySection(_scroll);
        _buildConnectivitySection(_scroll);
        _buildSystemSection(_scroll);
        _buildDangerSection(_scroll);

        // Keyboard: sits at screen bottom, shows when hostname textarea gains focus
        _kb.create(_screen);
        _kb.linkTo(_taHostname);

        lv_obj_add_event_cb(_taHostname, [](lv_event_t* e) {
            SettingsScreen* self = (SettingsScreen*)lv_event_get_user_data(e);
            lv_obj_set_height(self->_scroll, 140); // shrink scroll above keyboard
            self->_kb.show();
            lv_obj_scroll_to_view(self->_taHostname, LV_ANIM_ON);
        }, LV_EVENT_FOCUSED, this);

        // Hide keyboard and restore scroll on OK (✓) or Cancel (✕)
        auto hideKb = [](lv_event_t* e) {
            SettingsScreen* self = (SettingsScreen*)lv_event_get_user_data(e);
            self->_kb.hide();
            lv_obj_set_height(self->_scroll, 280);
        };
        lv_obj_add_event_cb(_kb.obj(), hideKb, LV_EVENT_READY,  this);
        lv_obj_add_event_cb(_kb.obj(), hideKb, LV_EVENT_CANCEL, this);
    }

    // Section: Display
    // Contains: slider with live value label, switch
    void _buildDisplaySection(lv_obj_t* parent) {
        lv_obj_t* card = _makeCard(parent);

        // Section title with icon
        lv_obj_t* sectionTitle = lv_label_create(card);
        lv_label_set_text(sectionTitle, LV_SYMBOL_IMAGE " Display");
        lv_obj_set_style_text_color(sectionTitle, gTheme->primary, LV_PART_MAIN);
        lv_obj_set_style_text_font(sectionTitle, gFontIcon, LV_PART_MAIN);

        _makeSeparator(card);

        // Brightness row: label on left, current value on right
        lv_obj_t* rowBrightness = _makeRow(card, "Brightness");
        _brightnessLabel = lv_label_create(rowBrightness);
        lv_label_set_text(_brightnessLabel, "128");
        lv_obj_set_style_text_color(_brightnessLabel, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_text_font(_brightnessLabel, gFontSmall, LV_PART_MAIN);
        lv_obj_align(_brightnessLabel, LV_ALIGN_RIGHT_MID, 0, 0);

        // Slider - full width, updates _brightnessLabel on drag
        // Uses this pointer as user_data to access the member variable from the callback
        lv_obj_t* slider = lv_slider_create(card);
        lv_obj_set_width(slider, lv_pct(100));
        lv_slider_set_range(slider, 0, 255);
        lv_slider_set_value(slider, 128, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(slider, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_bg_color(slider, gTheme->primary, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(slider, gTheme->textDark, LV_PART_KNOB);
        lv_obj_add_event_cb(slider, [](lv_event_t* e) {
            SettingsScreen* self = (SettingsScreen*)lv_event_get_user_data(e);
            int32_t val = lv_slider_get_value(lv_event_get_target(e));
            lv_label_set_text_fmt(self->_brightnessLabel, "%d", val);
        }, LV_EVENT_VALUE_CHANGED, this);

        _makeSeparator(card);

        // Dark mode row: label on left, switch on right
        lv_obj_t* rowDark = _makeRow(card, "Light mode");
        lv_obj_t* swDark = lv_switch_create(rowDark);
        lv_obj_set_style_bg_color(swDark, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_bg_color(swDark, gTheme->primary, LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(swDark, gTheme->textDark, LV_PART_KNOB);
        lv_obj_add_state(swDark, LV_STATE_CHECKED);
        lv_obj_align(swDark, LV_ALIGN_RIGHT_MID, 0, 0);
        if (isDarkTheme()) {
            lv_obj_add_state(swDark, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(swDark, LV_STATE_CHECKED);
        }
        lv_obj_align(swDark, LV_ALIGN_RIGHT_MID, 0, 0);

        lv_obj_add_event_cb(swDark, [](lv_event_t* e) {
            bool on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
            saveTheme(on);
            extern void resetToHome();
            resetToHome();
        }, LV_EVENT_VALUE_CHANGED, NULL);

        _makeSeparator(card);

        // Font selector
        lv_obj_t* rowFont = _makeRow(card, "Font");
        lv_obj_t* ddFont = lv_dropdown_create(rowFont);
        lv_dropdown_set_options(ddFont, "Montserrat\nIndie Flower");
        lv_dropdown_set_selected(ddFont, strcmp(gFontName, "indie_flower") == 0 ? 1 : 0);
        lv_obj_set_width(ddFont, 120);
        lv_obj_set_style_bg_color(ddFont, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_text_color(ddFont, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(ddFont, gFontSmall, LV_PART_MAIN);
        lv_obj_set_style_border_width(ddFont, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(ddFont, 8, LV_PART_MAIN);
        lv_obj_align(ddFont, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_add_event_cb(ddFont, [](lv_event_t* e) {
            uint16_t idx = lv_dropdown_get_selected(lv_event_get_target(e));
            saveFont(idx == 1 ? "indie_flower" : "montserrat");
            extern void resetToHome();
            resetToHome();
        }, LV_EVENT_VALUE_CHANGED, NULL);
    }

    // Section: Connectivity
    // Contains: switch, dropdown, text area, checkbox
    void _buildConnectivitySection(lv_obj_t* parent) {
        lv_obj_t* card = _makeCard(parent);

        lv_obj_t* sectionTitle = lv_label_create(card);
        lv_label_set_text(sectionTitle, LV_SYMBOL_WIFI " Connectivity");
        lv_obj_set_style_text_color(sectionTitle, gTheme->primary, LV_PART_MAIN);
        lv_obj_set_style_text_font(sectionTitle, gFontIcon, LV_PART_MAIN);

        _makeSeparator(card);

        // WiFi enabled toggle
        lv_obj_t* rowWifi = _makeRow(card, "WiFi enabled");
        lv_obj_t* swWifi = lv_switch_create(rowWifi);
        lv_obj_set_style_bg_color(swWifi, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_bg_color(swWifi, gTheme->primary, LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(swWifi, gTheme->textDark, LV_PART_KNOB);
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
        lv_obj_set_style_bg_color(dd, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_text_color(dd, gTheme->textDark, LV_PART_MAIN);
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
        lv_obj_set_style_text_color(lblHost, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(lblHost, gFontNormal, LV_PART_MAIN);

        // Text area - single-line input for a device hostname
        // Shows a placeholder when empty and highlights the border when focused
        _taHostname = lv_textarea_create(card);
        lv_obj_set_width(_taHostname, lv_pct(100));
        lv_obj_set_height(_taHostname, 38);
        lv_textarea_set_one_line(_taHostname, true);
        lv_textarea_set_placeholder_text(_taHostname, "pickle-os.local");
        lv_textarea_set_max_length(_taHostname, 64);
        lv_obj_set_style_bg_color(_taHostname, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_text_color(_taHostname, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_border_color(_taHostname, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_border_width(_taHostname, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(_taHostname, gTheme->primary, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_radius(_taHostname, 8, LV_PART_MAIN);

        _makeSeparator(card);

        // Checkbox - boolean option with an inline text label
        lv_obj_t* cb = lv_checkbox_create(card);
        lv_checkbox_set_text(cb, "Auto-reconnect on boot");
        lv_obj_set_style_text_color(cb, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(cb, gFontNormal, LV_PART_MAIN);
        lv_obj_set_style_bg_color(cb, gTheme->primaryDark, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(cb, gTheme->primary, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_border_color(cb, gTheme->textSoft, LV_PART_INDICATOR);
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
        lv_obj_set_style_text_color(sectionTitle, gTheme->primary, LV_PART_MAIN);
        lv_obj_set_style_text_font(sectionTitle, gFontIcon, LV_PART_MAIN);

        _makeSeparator(card);

        // Free RAM row: label on left, KB value on right
        // Both the bar and the label are stored as members to be updated in onResume()
        lv_obj_t* rowRam = _makeRow(card, "Free RAM");
        _ramLabel = lv_label_create(rowRam);
        lv_label_set_text(_ramLabel, "-- KB");
        lv_obj_set_style_text_color(_ramLabel, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_text_font(_ramLabel, gFontSmall, LV_PART_MAIN);
        lv_obj_align(_ramLabel, LV_ALIGN_RIGHT_MID, 0, 0);

        // Bar showing RAM usage as a filled indicator
        _ramBar = lv_bar_create(card);
        lv_obj_set_width(_ramBar, lv_pct(100));
        lv_obj_set_height(_ramBar, 12);
        lv_bar_set_range(_ramBar, 0, 320); // ESP32 has ~320 KB total SRAM
        lv_bar_set_value(_ramBar, 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(_ramBar, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_bg_color(_ramBar, gTheme->primary, LV_PART_INDICATOR);
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
        lv_obj_set_style_text_color(sectionTitle, gTheme->errorText, LV_PART_MAIN);
        lv_obj_set_style_text_font(sectionTitle, gFontIcon, LV_PART_MAIN);

        _makeSeparator(card);

        // Destructive button - same structure as a normal button, danger colors only
        lv_obj_t* btnReset = lv_btn_create(card);
        lv_obj_set_width(btnReset, lv_pct(100));
        lv_obj_set_height(btnReset, 36);
        lv_obj_set_style_bg_color(btnReset, gTheme->errorBg, LV_PART_MAIN);
        lv_obj_set_style_bg_color(btnReset, gTheme->errorText, LV_STATE_PRESSED);
        lv_obj_set_style_radius(btnReset, 8, LV_PART_MAIN);
        lv_obj_add_event_cb(btnReset, [](lv_event_t* e) {
            // Trigger factory reset here when implemented
        }, LV_EVENT_CLICKED, NULL);

        lv_obj_t* btnLbl = lv_label_create(btnReset);
        lv_label_set_text(btnLbl, LV_SYMBOL_TRASH " Reset to factory defaults");
        lv_obj_set_style_text_color(btnLbl, gTheme->errorText, LV_PART_MAIN);
        lv_obj_set_style_text_font(btnLbl, gFontIcon, LV_PART_MAIN);
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
        lv_obj_set_style_bg_color(card, gTheme->backgroundPopup, LV_PART_MAIN);
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
        lv_obj_set_style_text_color(lbl, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, gFontNormal, LV_PART_MAIN);
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
        lv_obj_set_style_text_color(keyLbl, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_text_font(keyLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_align(keyLbl, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t* valLbl = lv_label_create(row);
        lv_label_set_text(valLbl, val);
        lv_obj_set_style_text_color(valLbl, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(valLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_align(valLbl, LV_ALIGN_RIGHT_MID, 0, 0);
    }

    // Helper: creates a 1px horizontal separator line
    void _makeSeparator(lv_obj_t* parent) {
        lv_obj_t* sep = lv_obj_create(parent);
        lv_obj_set_size(sep, lv_pct(100), 1);
        lv_obj_set_style_bg_color(sep, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_border_width(sep, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(sep, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(sep, 0, LV_PART_MAIN);
    }
};