#pragma once
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

class SDManager {
public:
    static SDManager& getInstance() {
        static SDManager instance;
        return instance;
    }

    // Initialize SD card on the given CS pin and SPI bus
    void init(uint8_t csPin, SPIClass& spi);

    bool isMounted() const { return _mounted; }

    // File operations
    bool   exists(const char* path);
    String readFile(const char* path);
    bool   writeFile(const char* path, const char* content, bool append = false);
    bool   removeFile(const char* path);

    // Directory operations
    bool makeDir(const char* path);
    bool removeDir(const char* path); // Only removes empty directories
    void listDir(const char* path, String& out);

private:
    SDManager() = default;
    SDManager(const SDManager&) = delete;
    SDManager& operator=(const SDManager&) = delete;

    bool _mounted = false;
};
