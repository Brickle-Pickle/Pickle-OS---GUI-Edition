#pragma once
#include "screen_base.h"
#include "../screen_manager.h"
#include "../theme/theme.h"
#include "../toast/toast_manager.h"
#include "../../storage/sd_manager.h"
#include "notepad_screen.h"

// Files Screen - List and manage files on SD card
class FilesScreen : public ScreenBase {
public:
    void onCreate() override {
        _screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(_screen, gTheme->background, LV_PART_MAIN);

        _buildHeader();
        _buildContent();
        _buildFooter();
    }

    void onDestroy() override {
        if (_screen) {
            lv_obj_del(_screen);
            _screen = nullptr;
        }
    }

    void onResume() override {
        // Refresh the title and toggle label in case we switched storage types
        if (_pathLabel) {
            lv_label_set_text(_pathLabel, _currentPath.c_str());
        }

        // Refresh file list when returning to this screen
        _buildContent();
    }

private:
    struct ItemCtx {
        char* name;
        bool isDir;
        FilesScreen* screen;
    };

    bool _isLocal = true; // Whether we're showing local (SD) or internal files
    String _currentPath = "/pickle-os"; // Current path being displayed
    lv_obj_t* _pathLabel = nullptr; // Label to show current path
    lv_obj_t* _toggleLbl = nullptr; // Label inside the SD/Internal toggle button
    lv_obj_t* _fileList = nullptr; // List widget for files

    // Create dialog state
    lv_obj_t* _dialog = nullptr;
    lv_obj_t* _dialogTitle = nullptr;
    lv_obj_t* _dialogInput = nullptr;
    lv_obj_t* _dialogKb = nullptr;
    bool _creatingDir = false;

    // Delete dialog state
    lv_obj_t* _deleteDialog = nullptr;
    String _deleteTargetName;
    bool _deleteTargetIsDir = false;

    // When a long-press fires, the trailing CLICKED event must be ignored
    bool _suppressNextClick = false;

    // Header with back button, title, and toggle for local/internal
    void _buildHeader() {
        // Header container
        lv_obj_t* header = lv_obj_create(_screen);
        lv_obj_set_size(header, 240, 36);
        lv_obj_set_pos(header, 0, 0);
        lv_obj_set_style_bg_color(header, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(header, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
        lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

        // Back button
        lv_obj_t* backBtn = lv_btn_create(header);
        lv_obj_set_size(backBtn, 32, 28);
        lv_obj_align(backBtn, LV_ALIGN_LEFT_MID, 4, 0);
        lv_obj_set_style_bg_color(backBtn, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_bg_color(backBtn, gTheme->primary, LV_STATE_PRESSED);
        lv_obj_set_style_radius(backBtn, 8, LV_PART_MAIN);
        lv_obj_set_style_pad_all(backBtn, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(backBtn, 0, LV_PART_MAIN);
        lv_obj_clear_flag(backBtn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* backIcon = lv_label_create(backBtn);
        lv_label_set_text(backIcon, LV_SYMBOL_LEFT);
        lv_obj_set_style_text_color(backIcon, gTheme->textDark, LV_PART_MAIN);
        lv_obj_center(backIcon);
        lv_obj_add_event_cb(backBtn, [](lv_event_t* e) {
            // On back, if we're not at root, go up one level. Otherwise, go back to home.
            FilesScreen* screen = static_cast<FilesScreen*>(lv_event_get_user_data(e));
            if (screen->_currentPath != "/pickle-os") {
                int lastSlash = screen->_currentPath.lastIndexOf('/');
                screen->_currentPath = screen->_currentPath.substring(0, lastSlash);
                if (screen->_pathLabel) {
                    lv_label_set_text(screen->_pathLabel, screen->_currentPath.c_str());
                }
                screen->_buildContent();
            } else {
                ScreenManager::getInstance().goBack();
            }
        }, LV_EVENT_CLICKED, this);

        // Title
        _pathLabel = lv_label_create(header);
        lv_label_set_text(_pathLabel, _currentPath.c_str());
        lv_label_set_long_mode(_pathLabel, LV_LABEL_LONG_DOT);
        lv_obj_set_width(_pathLabel, 110);
        lv_obj_set_style_text_color(_pathLabel, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(_pathLabel, gFontSmall, LV_PART_MAIN);
        lv_obj_align(_pathLabel, LV_ALIGN_LEFT_MID, 44, 0);

        // Toggle button
        lv_obj_t* toggle = lv_btn_create(header);
        lv_obj_set_size(toggle, 72, 28);
        lv_obj_align(toggle, LV_ALIGN_RIGHT_MID, -4, 0);
        lv_obj_set_style_bg_color(toggle, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_bg_color(toggle, gTheme->primary, LV_STATE_PRESSED);
        lv_obj_set_style_radius(toggle, 8, LV_PART_MAIN);
        lv_obj_set_style_pad_all(toggle, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(toggle, 0, LV_PART_MAIN);
        lv_obj_clear_flag(toggle, LV_OBJ_FLAG_SCROLLABLE);
        _toggleLbl = lv_label_create(toggle);
        lv_label_set_text(_toggleLbl, _isLocal ? "SD" : "Internal");
        lv_obj_set_style_text_color(_toggleLbl, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(_toggleLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_center(_toggleLbl);
        lv_obj_add_event_cb(toggle, [](lv_event_t* e) {
            FilesScreen* screen = static_cast<FilesScreen*>(lv_event_get_user_data(e));
            screen->_isLocal = !screen->_isLocal;
            if (screen->_toggleLbl) {
                lv_label_set_text(screen->_toggleLbl, screen->_isLocal ? "SD" : "Internal");
            }
            screen->_buildContent();
        }, LV_EVENT_CLICKED, this);
    }

    // Content area with file list
    void _buildContent() {
        // If file list already exists, delete it before rebuilding
        if (_fileList) {
            lv_obj_del(_fileList);
            _fileList = nullptr;
        }

        _fileList = lv_list_create(_screen);
        lv_obj_set_size(_fileList, 240, 320 - 36 - 40);
        lv_obj_set_pos(_fileList, 0, 36);
        lv_obj_set_style_bg_color(_fileList, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(_fileList, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(_fileList, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(_fileList, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_fileList, 4, LV_PART_MAIN);
        lv_obj_set_style_pad_row(_fileList, 4, LV_PART_MAIN);
        // Scrollbar styling
        lv_obj_set_style_bg_color(_fileList, gTheme->primaryDark, LV_PART_SCROLLBAR);
        lv_obj_set_style_bg_opa(_fileList, LV_OPA_60, LV_PART_SCROLLBAR);
        lv_obj_set_style_width(_fileList, 4, LV_PART_SCROLLBAR);
        lv_obj_set_style_radius(_fileList, 2, LV_PART_SCROLLBAR);

        // Get files from the appropriate storage.
        // SDManager::listDir writes lines like "[DIR]  name" / "[FILE] name" into a String.
        String raw;
        if (_isLocal) {
            SDManager::getInstance().listDir(_currentPath.c_str(), raw);
        } else {
            // For internal storage, we can mock some files for now
            raw = "[FILE] config.txt\n[FILE] data.log\n[FILE] notes.md\n";
        }

        // Parse the raw listing into rows and add them to the list
        int count = 0;
        int start = 0;
        while (start < (int)raw.length()) {
            int nl = raw.indexOf('\n', start);
            if (nl < 0) nl = raw.length();
            String line = raw.substring(start, nl);
            start = nl + 1;
            if (line.length() == 0) continue;

            bool isDir = line.startsWith("[DIR]");
            int sp = line.indexOf(' ');
            String name = (sp >= 0) ? line.substring(sp + 1) : line;
            name.trim();
            if (name.length() == 0) continue;

            const char* icon = isDir ? LV_SYMBOL_DIRECTORY : LV_SYMBOL_FILE;
            lv_obj_t* item = lv_list_add_btn(_fileList, icon, name.c_str());

            // Heap-allocated context owned by the item; freed on LV_EVENT_DELETE.
            ItemCtx* ctx = new ItemCtx{ strdup(name.c_str()), isDir, this };
            lv_obj_set_user_data(item, ctx);

            // Click: navigate into folders (files are no-op for now)
            lv_obj_add_event_cb(item, [](lv_event_t* e) {
                lv_obj_t* target = lv_event_get_target(e);
                ItemCtx* c = static_cast<ItemCtx*>(lv_obj_get_user_data(target));
                if (!c || !c->screen) return;
                FilesScreen* screen = c->screen;
                // Suppress the click that follows a long-press
                if (screen->_suppressNextClick) {
                    screen->_suppressNextClick = false;
                    return;
                }
                String childPath = screen->_currentPath;
                if (!childPath.endsWith("/")) childPath += "/";
                childPath += c->name;

                if (c->isDir) {
                    screen->_currentPath = childPath;
                    if (screen->_pathLabel) {
                        lv_label_set_text(screen->_pathLabel, screen->_currentPath.c_str());
                    }
                    screen->_buildContent();
                } else {
                    if (!screen->_isLocal) {
                        ToastManager::getInstance().showToast("Internal storage is read-only",
                                                              ToastType::ALERT);
                        return;
                    }
                    ScreenManager::getInstance().navigateTo(new NotepadScreen(childPath));
                }
            }, LV_EVENT_CLICKED, this);

            // Long-press: open delete confirmation
            lv_obj_add_event_cb(item, [](lv_event_t* e) {
                lv_obj_t* target = lv_event_get_target(e);
                ItemCtx* c = static_cast<ItemCtx*>(lv_obj_get_user_data(target));
                if (!c || !c->screen) return;
                c->screen->_suppressNextClick = true;
                c->screen->_openDeleteDialog(c->name, c->isDir);
            }, LV_EVENT_LONG_PRESSED, this);

            // Free heap context when the item is destroyed
            lv_obj_add_event_cb(item, [](lv_event_t* e) {
                lv_obj_t* target = lv_event_get_target(e);
                ItemCtx* c = static_cast<ItemCtx*>(lv_obj_get_user_data(target));
                if (c) {
                    if (c->name) free(c->name);
                    delete c;
                    lv_obj_set_user_data(target, nullptr);
                }
            }, LV_EVENT_DELETE, nullptr);

            // Item container styling
            lv_obj_set_style_bg_color(item, gTheme->backgroundPopup, LV_PART_MAIN);
            lv_obj_set_style_bg_color(item, gTheme->primaryDark, LV_STATE_PRESSED);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_border_width(item, 0, LV_PART_MAIN);
            lv_obj_set_style_radius(item, 8, LV_PART_MAIN);
            lv_obj_set_style_pad_hor(item, 10, LV_PART_MAIN);
            lv_obj_set_style_pad_ver(item, 8, LV_PART_MAIN);
            lv_obj_set_style_text_color(item, gTheme->textDark, LV_PART_MAIN);
            lv_obj_set_style_text_font(item, gFontSmall, LV_PART_MAIN);

            // Style children: child 0 = icon (needs a font with symbol glyphs), child 1 = name label
            uint32_t childCount = lv_obj_get_child_cnt(item);
            for (uint32_t c = 0; c < childCount; c++) {
                lv_obj_t* child = lv_obj_get_child(item, c);
                if (c == 0) {
                    lv_obj_set_style_text_font(child, gFontIcon, LV_PART_MAIN);
                    lv_obj_set_style_text_color(child, isDir ? gTheme->primary : gTheme->textSoft, LV_PART_MAIN);
                    lv_obj_set_style_img_recolor(child, isDir ? gTheme->primary : gTheme->textSoft, LV_PART_MAIN);
                    lv_obj_set_style_img_recolor_opa(child, LV_OPA_COVER, LV_PART_MAIN);
                } else {
                    lv_obj_set_style_text_font(child, gFontSmall, LV_PART_MAIN);
                    lv_obj_set_style_text_color(child, gTheme->textDark, LV_PART_MAIN);
                }
            }
            count++;
        }

        if (count == 0) {
            lv_obj_t* emptyLabel = lv_label_create(_fileList);
            lv_label_set_text(emptyLabel, "No files found");
            lv_obj_set_style_text_color(emptyLabel, gTheme->textSoft, LV_PART_MAIN);
            lv_obj_set_style_text_font(emptyLabel, gFontSmall, LV_PART_MAIN);
            lv_obj_center(emptyLabel);
        }
    }
    
    // Footer with "New File" and "New Folder" buttons
    void _buildFooter() {
        lv_obj_t* footer = lv_obj_create(_screen);
        lv_obj_set_size(footer, 240, 40);
        lv_obj_set_pos(footer, 0, 320 - 40);
        lv_obj_set_style_bg_color(footer, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_border_width(footer, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(footer, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(footer, 4, LV_PART_MAIN);
        lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

        auto makeBtn = [&](const char* text, int xAlign, bool isDir) -> lv_obj_t* {
            lv_obj_t* btn = lv_btn_create(footer);
            lv_obj_set_size(btn, 112, 32);
            lv_obj_align(btn, LV_ALIGN_LEFT_MID, xAlign, 0);
            lv_obj_set_style_bg_color(btn, gTheme->primaryDark, LV_PART_MAIN);
            lv_obj_set_style_bg_color(btn, gTheme->primary, LV_STATE_PRESSED);
            lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
            lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN);
            lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_t* lbl = lv_label_create(btn);
            lv_label_set_text(lbl, text);
            lv_obj_set_style_text_color(lbl, gTheme->textDark, LV_PART_MAIN);
            lv_obj_set_style_text_font(lbl, gFontSmall, LV_PART_MAIN);
            lv_obj_center(lbl);
            // Pack isDir flag into user_data via this+flag using a small struct on heap
            lv_obj_set_user_data(btn, this);
            lv_obj_add_event_cb(btn, isDir ? &FilesScreen::_onNewDirCb : &FilesScreen::_onNewFileCb,
                                LV_EVENT_CLICKED, this);
            return btn;
        };

        makeBtn("+ File", 4, false);
        makeBtn("+ Folder", 124, true);
    }

    static void _onNewFileCb(lv_event_t* e) {
        FilesScreen* s = static_cast<FilesScreen*>(lv_event_get_user_data(e));
        if (s) s->_openCreateDialog(false);
    }
    static void _onNewDirCb(lv_event_t* e) {
        FilesScreen* s = static_cast<FilesScreen*>(lv_event_get_user_data(e));
        if (s) s->_openCreateDialog(true);
    }

    void _openCreateDialog(bool isDir) {
        if (!_isLocal) {
            ToastManager::getInstance().showToast("Internal storage is read-only",
                                                  ToastType::ALERT);
            return;
        }
        _creatingDir = isDir;
        if (_dialog) {
            lv_obj_del(_dialog);
            _dialog = nullptr;
        }

        // Modal backdrop covering the whole screen
        _dialog = lv_obj_create(_screen);
        lv_obj_set_size(_dialog, 240, 320);
        lv_obj_set_pos(_dialog, 0, 0);
        lv_obj_set_style_bg_color(_dialog, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(_dialog, LV_OPA_50, LV_PART_MAIN);
        lv_obj_set_style_border_width(_dialog, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(_dialog, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_dialog, 0, LV_PART_MAIN);
        lv_obj_clear_flag(_dialog, LV_OBJ_FLAG_SCROLLABLE);

        // Card
        lv_obj_t* card = lv_obj_create(_dialog);
        lv_obj_set_size(card, 220, 140);
        lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 16);
        lv_obj_set_style_bg_color(card, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(card, 10, LV_PART_MAIN);
        lv_obj_set_style_pad_all(card, 10, LV_PART_MAIN);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        _dialogTitle = lv_label_create(card);
        lv_label_set_text(_dialogTitle, isDir ? "New folder name" : "New file name");
        lv_obj_set_style_text_color(_dialogTitle, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(_dialogTitle, gFontSmall, LV_PART_MAIN);
        lv_obj_align(_dialogTitle, LV_ALIGN_TOP_LEFT, 0, 0);

        _dialogInput = lv_textarea_create(card);
        lv_obj_set_size(_dialogInput, 200, 36);
        lv_obj_align(_dialogInput, LV_ALIGN_TOP_LEFT, 0, 22);
        lv_textarea_set_one_line(_dialogInput, true);
        lv_textarea_set_placeholder_text(_dialogInput, isDir ? "folder" : "file.txt");
        lv_obj_set_style_bg_color(_dialogInput, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_text_color(_dialogInput, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(_dialogInput, gFontSmall, LV_PART_MAIN);
        lv_obj_set_style_border_width(_dialogInput, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(_dialogInput, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_radius(_dialogInput, 6, LV_PART_MAIN);

        // Cancel button
        lv_obj_t* cancelBtn = lv_btn_create(card);
        lv_obj_set_size(cancelBtn, 90, 32);
        lv_obj_align(cancelBtn, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_set_style_bg_color(cancelBtn, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_radius(cancelBtn, 8, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(cancelBtn, 0, LV_PART_MAIN);
        lv_obj_clear_flag(cancelBtn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* cancelLbl = lv_label_create(cancelBtn);
        lv_label_set_text(cancelLbl, "Cancel");
        lv_obj_set_style_text_color(cancelLbl, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(cancelLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_center(cancelLbl);
        lv_obj_add_event_cb(cancelBtn, [](lv_event_t* e) {
            FilesScreen* s = static_cast<FilesScreen*>(lv_event_get_user_data(e));
            if (s) s->_closeCreateDialog();
        }, LV_EVENT_CLICKED, this);

        // Create button
        lv_obj_t* okBtn = lv_btn_create(card);
        lv_obj_set_size(okBtn, 90, 32);
        lv_obj_align(okBtn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
        lv_obj_set_style_bg_color(okBtn, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_bg_color(okBtn, gTheme->primary, LV_STATE_PRESSED);
        lv_obj_set_style_radius(okBtn, 8, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(okBtn, 0, LV_PART_MAIN);
        lv_obj_clear_flag(okBtn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* okLbl = lv_label_create(okBtn);
        lv_label_set_text(okLbl, "Create");
        lv_obj_set_style_text_color(okLbl, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(okLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_center(okLbl);
        lv_obj_add_event_cb(okBtn, [](lv_event_t* e) {
            FilesScreen* s = static_cast<FilesScreen*>(lv_event_get_user_data(e));
            if (s) s->_confirmCreate();
        }, LV_EVENT_CLICKED, this);

        // Keyboard at the bottom, attached to the textarea
        _dialogKb = lv_keyboard_create(_dialog);
        lv_obj_set_size(_dialogKb, 240, 140);
        lv_obj_align(_dialogKb, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_keyboard_set_textarea(_dialogKb, _dialogInput);
        lv_keyboard_set_mode(_dialogKb, LV_KEYBOARD_MODE_TEXT_LOWER);

        lv_obj_add_state(_dialogInput, LV_STATE_FOCUSED);
    }

    void _closeCreateDialog() {
        if (_dialog) {
            lv_obj_del(_dialog);
            _dialog = nullptr;
            _dialogInput = nullptr;
            _dialogKb = nullptr;
            _dialogTitle = nullptr;
        }
    }

    void _confirmCreate() {
        if (!_dialogInput) return;
        const char* raw = lv_textarea_get_text(_dialogInput);
        String name = raw ? String(raw) : String("");
        name.trim();
        if (name.length() == 0) {
            ToastManager::getInstance().showToast("Name cannot be empty", ToastType::ALERT);
            return;
        }
        if (name.indexOf('/') >= 0 || name.indexOf('\\') >= 0) {
            ToastManager::getInstance().showToast("Invalid name", ToastType::ALERT);
            return;
        }

        String full = _currentPath;
        if (!full.endsWith("/")) full += "/";
        full += name;

        bool ok = false;
        if (_creatingDir) {
            ok = SDManager::getInstance().makeDir(full.c_str());
        } else {
            if (SDManager::getInstance().exists(full.c_str())) {
                ToastManager::getInstance().showToast("File already exists", ToastType::ALERT);
                return;
            }
            ok = SDManager::getInstance().writeFile(full.c_str(), "", false);
        }

        if (ok) {
            ToastManager::getInstance().showToast(_creatingDir ? "Folder created" : "File created",
                                                  ToastType::SUCCESS);
            _closeCreateDialog();
            _buildContent();
        } else {
            ToastManager::getInstance().showToast("Create failed", ToastType::ALERT);
        }
    }

    void _openDeleteDialog(const char* name, bool isDir) {
        if (!_isLocal) {
            ToastManager::getInstance().showToast("Internal storage is read-only",
                                                  ToastType::ALERT);
            return;
        }
        if (!name) return;
        _deleteTargetName = name;
        _deleteTargetIsDir = isDir;

        if (_deleteDialog) {
            lv_obj_del(_deleteDialog);
            _deleteDialog = nullptr;
        }

        _deleteDialog = lv_obj_create(_screen);
        lv_obj_set_size(_deleteDialog, 240, 320);
        lv_obj_set_pos(_deleteDialog, 0, 0);
        lv_obj_set_style_bg_color(_deleteDialog, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(_deleteDialog, LV_OPA_50, LV_PART_MAIN);
        lv_obj_set_style_border_width(_deleteDialog, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(_deleteDialog, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_deleteDialog, 0, LV_PART_MAIN);
        lv_obj_clear_flag(_deleteDialog, LV_OBJ_FLAG_SCROLLABLE);

        // Card
        lv_obj_t* card = lv_obj_create(_deleteDialog);
        lv_obj_set_size(card, 220, 150);
        lv_obj_center(card);
        lv_obj_set_style_bg_color(card, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(card, 10, LV_PART_MAIN);
        lv_obj_set_style_pad_all(card, 12, LV_PART_MAIN);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* title = lv_label_create(card);
        lv_label_set_text(title, isDir ? "Delete folder?" : "Delete file?");
        lv_obj_set_style_text_color(title, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(title, gFontSmall, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

        lv_obj_t* nameLbl = lv_label_create(card);
        lv_label_set_text(nameLbl, _deleteTargetName.c_str());
        lv_label_set_long_mode(nameLbl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(nameLbl, 196);
        lv_obj_set_style_text_color(nameLbl, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_text_font(nameLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_align(nameLbl, LV_ALIGN_TOP_LEFT, 0, 24);

        if (isDir) {
            lv_obj_t* hint = lv_label_create(card);
            lv_label_set_text(hint, "Folder must be empty.");
            lv_obj_set_style_text_color(hint, gTheme->textSoft, LV_PART_MAIN);
            lv_obj_set_style_text_font(hint, gFontSmall, LV_PART_MAIN);
            lv_obj_align(hint, LV_ALIGN_TOP_LEFT, 0, 48);
        }

        // Cancel
        lv_obj_t* cancelBtn = lv_btn_create(card);
        lv_obj_set_size(cancelBtn, 90, 32);
        lv_obj_align(cancelBtn, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_set_style_bg_color(cancelBtn, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_radius(cancelBtn, 8, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(cancelBtn, 0, LV_PART_MAIN);
        lv_obj_clear_flag(cancelBtn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* cancelLbl = lv_label_create(cancelBtn);
        lv_label_set_text(cancelLbl, "Cancel");
        lv_obj_set_style_text_color(cancelLbl, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(cancelLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_center(cancelLbl);
        lv_obj_add_event_cb(cancelBtn, [](lv_event_t* e) {
            FilesScreen* s = static_cast<FilesScreen*>(lv_event_get_user_data(e));
            if (s) s->_closeDeleteDialog();
        }, LV_EVENT_CLICKED, this);

        // Delete
        lv_obj_t* okBtn = lv_btn_create(card);
        lv_obj_set_size(okBtn, 90, 32);
        lv_obj_align(okBtn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
        lv_obj_set_style_bg_color(okBtn, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
        lv_obj_set_style_bg_color(okBtn, lv_palette_darken(LV_PALETTE_RED, 2), LV_STATE_PRESSED);
        lv_obj_set_style_radius(okBtn, 8, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(okBtn, 0, LV_PART_MAIN);
        lv_obj_clear_flag(okBtn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* okLbl = lv_label_create(okBtn);
        lv_label_set_text(okLbl, "Delete");
        lv_obj_set_style_text_color(okLbl, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_font(okLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_center(okLbl);
        lv_obj_add_event_cb(okBtn, [](lv_event_t* e) {
            FilesScreen* s = static_cast<FilesScreen*>(lv_event_get_user_data(e));
            if (s) s->_confirmDelete();
        }, LV_EVENT_CLICKED, this);
    }

    void _closeDeleteDialog() {
        if (_deleteDialog) {
            lv_obj_del(_deleteDialog);
            _deleteDialog = nullptr;
        }
    }

    void _confirmDelete() {
        if (_deleteTargetName.length() == 0) {
            _closeDeleteDialog();
            return;
        }

        String full = _currentPath;
        if (!full.endsWith("/")) full += "/";
        full += _deleteTargetName;

        bool ok = _deleteTargetIsDir
                      ? SDManager::getInstance().removeDir(full.c_str())
                      : SDManager::getInstance().removeFile(full.c_str());

        if (ok) {
            ToastManager::getInstance().showToast(_deleteTargetIsDir ? "Folder deleted" : "File deleted",
                                                  ToastType::SUCCESS);
            _closeDeleteDialog();
            _buildContent();
        } else {
            ToastManager::getInstance().showToast("Delete failed", ToastType::ALERT);
        }
    }
};