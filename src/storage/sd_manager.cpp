#include "sd_manager.h"

void SDManager::init(uint8_t csPin, SPIClass& spi) {
    if (_mounted) return;

    if (!SD.begin(csPin, spi)) {
        Serial.println("[SDManager] Mount failed. Check CS pin and card format (FAT32).");
        return;
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("[SDManager] No SD card detected.");
        return;
    }

    _mounted = true;
    Serial.printf("[SDManager] Mounted. Size: %llu MB\n", SD.cardSize() / (1024 * 1024));
}

bool SDManager::exists(const char* path) {
    if (!_mounted) return false;
    return SD.exists(path);
}

String SDManager::readFile(const char* path) {
    if (!_mounted) return "";

    File file = SD.open(path, FILE_READ);
    if (!file) {
        Serial.printf("[SDManager] Cannot open for read: %s\n", path);
        return "";
    }

    String content;
    while (file.available()) {
        content += (char)file.read();
    }
    file.close();
    return content;
}

bool SDManager::writeFile(const char* path, const char* content, bool append) {
    if (!_mounted) return false;

    const char* mode = append ? FILE_APPEND : FILE_WRITE;
    File file = SD.open(path, mode);
    if (!file) {
        Serial.printf("[SDManager] Cannot open for write: %s\n", path);
        return false;
    }

    bool ok = file.print(content);
    file.close();
    return ok;
}

bool SDManager::removeFile(const char* path) {
    if (!_mounted) return false;
    return SD.remove(path);
}

bool SDManager::makeDir(const char* path) {
    if (!_mounted) return false;
    return SD.mkdir(path);
}

bool SDManager::removeDir(const char* path) {
    if (!_mounted) return false;
    return SD.rmdir(path);
}

void SDManager::listDir(const char* path, String& out) {
    if (!_mounted) return;

    File dir = SD.open(path);
    if (!dir || !dir.isDirectory()) {
        Serial.printf("[SDManager] Not a directory: %s\n", path);
        return;
    }

    File entry = dir.openNextFile();
    while (entry) {
        out += entry.isDirectory() ? "[DIR]  " : "[FILE] ";
        out += entry.name();
        out += "\n";
        entry.close();
        entry = dir.openNextFile();
    }
    dir.close();
}
