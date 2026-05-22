#pragma once
#include <Arduino.h>

// Connector where a module can be plugged in.
enum class ModuleConnector {
    CN1,
    P3
};

// Categories of modules currently planned for Pickle OS.
enum class ModuleType {
    UNKNOWN,
    TEMP_HUMIDITY,
    RFID,
    CUSTOM
};

// Module - Abstract interface every hardware module must implement.
class Module {
public:
    virtual ~Module() = default;

    virtual void init() = 0;
    virtual void update() = 0;

    virtual const char* name() const = 0;
    virtual ModuleType type() const = 0;
    virtual ModuleConnector connector() const = 0;
    virtual bool isConnected() const = 0;
};
