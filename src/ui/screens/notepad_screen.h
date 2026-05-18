#pragma once
#include "screen_base.h"
#include "../screen_manager.h"
#include "../theme/theme.h"
#include "../toast/toast_manager.h"
#include "../../storage/sd_manager.h"

// Notepad Screen - Edit the text contents of a file on the SD card
class NotepadScreen : public ScreenBase {
public:
    explicit NotepadScreen(const String& path) : _path(path) {}

    void onCreate() override {
        _screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(_screen, gTheme->background, LV_PART_MAIN);

        _buildHeader();
        _buildEditor();
        _buildKeyboard();

        // Load file content into the editor (empty string if it doesn't exist)
        String content = SDManager::getInstance().readFile(_path.c_str());
        lv_textarea_set_text(_editor, content.c_str());
    }

    void onDestroy() override {
        if (_screen) {
            lv_obj_del(_screen);
            _screen = nullptr;
        }
    }

private:
    String _path;
    lv_obj_t* _editor = nullptr;
    lv_obj_t* _keyboard = nullptr;
    lv_obj_t* _titleLabel = nullptr;

    // Header: file name + Save + Exit
    void _buildHeader() {
        lv_obj_t* header = lv_obj_create(_screen);
        lv_obj_set_size(header, 240, 36);
        lv_obj_set_pos(header, 0, 0);
        lv_obj_set_style_bg_color(header, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(header, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
        lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

        // Exit button (left)
        lv_obj_t* exitBtn = lv_btn_create(header);
        lv_obj_set_size(exitBtn, 56, 28);
        lv_obj_align(exitBtn, LV_ALIGN_LEFT_MID, 4, 0);
        lv_obj_set_style_bg_color(exitBtn, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_radius(exitBtn, 8, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(exitBtn, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(exitBtn, 0, LV_PART_MAIN);
        lv_obj_clear_flag(exitBtn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* exitLbl = lv_label_create(exitBtn);
        lv_label_set_text(exitLbl, "Exit");
        lv_obj_set_style_text_color(exitLbl, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(exitLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_center(exitLbl);
        lv_obj_add_event_cb(exitBtn, [](lv_event_t*) {
            ScreenManager::getInstance().goBack();
        }, LV_EVENT_CLICKED, nullptr);

        // File name in the middle (basename only)
        _titleLabel = lv_label_create(header);
        int slash = _path.lastIndexOf('/');
        String base = (slash >= 0) ? _path.substring(slash + 1) : _path;
        lv_label_set_text(_titleLabel, base.c_str());
        lv_label_set_long_mode(_titleLabel, LV_LABEL_LONG_DOT);
        lv_obj_set_width(_titleLabel, 112);
        lv_obj_set_style_text_color(_titleLabel, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(_titleLabel, gFontSmall, LV_PART_MAIN);
        lv_obj_align(_titleLabel, LV_ALIGN_CENTER, 0, 0);

        // Save button (right)
        lv_obj_t* saveBtn = lv_btn_create(header);
        lv_obj_set_size(saveBtn, 56, 28);
        lv_obj_align(saveBtn, LV_ALIGN_RIGHT_MID, -4, 0);
        lv_obj_set_style_bg_color(saveBtn, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_bg_color(saveBtn, gTheme->primary, LV_STATE_PRESSED);
        lv_obj_set_style_radius(saveBtn, 8, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(saveBtn, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(saveBtn, 0, LV_PART_MAIN);
        lv_obj_clear_flag(saveBtn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* saveLbl = lv_label_create(saveBtn);
        lv_label_set_text(saveLbl, "Save");
        lv_obj_set_style_text_color(saveLbl, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(saveLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_center(saveLbl);
        lv_obj_add_event_cb(saveBtn, [](lv_event_t* e) {
            NotepadScreen* s = static_cast<NotepadScreen*>(lv_event_get_user_data(e));
            if (s) s->_save();
        }, LV_EVENT_CLICKED, this);
    }

    // Multiline text editor that fills the space above the keyboard
    void _buildEditor() {
        _editor = lv_textarea_create(_screen);
        lv_obj_set_size(_editor, 240, 320 - 36 - 140);
        lv_obj_set_pos(_editor, 0, 36);
        lv_textarea_set_one_line(_editor, false);
        lv_textarea_set_placeholder_text(_editor, "Empty file");
        lv_obj_set_style_bg_color(_editor, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_text_color(_editor, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(_editor, gFontSmall, LV_PART_MAIN);
        lv_obj_set_style_border_width(_editor, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(_editor, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_editor, 6, LV_PART_MAIN);
        // Scrollbar styling
        lv_obj_set_style_bg_color(_editor, gTheme->primaryDark, LV_PART_SCROLLBAR);
        lv_obj_set_style_bg_opa(_editor, LV_OPA_60, LV_PART_SCROLLBAR);
        lv_obj_set_style_width(_editor, 4, LV_PART_SCROLLBAR);
        lv_obj_set_style_radius(_editor, 2, LV_PART_SCROLLBAR);
    }

    // On-screen keyboard pinned to the bottom, always visible
    void _buildKeyboard() {
        _keyboard = lv_keyboard_create(_screen);
        lv_obj_set_size(_keyboard, 240, 140);
        lv_obj_align(_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_keyboard_set_textarea(_keyboard, _editor);
        lv_keyboard_set_mode(_keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    }

    void _save() {
        const char* text = lv_textarea_get_text(_editor);
        bool ok = SDManager::getInstance().writeFile(_path.c_str(),
                                                     text ? text : "",
                                                     false);
        if (ok) {
            ToastManager::getInstance().showToast("Saved", ToastType::SUCCESS);
        } else {
            ToastManager::getInstance().showToast("Save failed", ToastType::ALERT);
        }
    }
};
