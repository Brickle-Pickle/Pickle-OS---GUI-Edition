#pragma once
#include <vector>
#include "module.h"

// ModuleManager - Singleton registry of installed hardware modules.
class ModuleManager {
public:
    static ModuleManager& getInstance() {
        static ModuleManager instance;
        return instance;
    }

    // Register and initialize a module. Takes ownership of the pointer.
    void registerModule(Module* mod) {
        if (!mod) return;
        mod->init();
        _modules.push_back(mod);
    }

    void update() {
        for (Module* m : _modules) m->update();
    }

    size_t count() const { return _modules.size(); }
    Module* at(size_t i) const {
        return (i < _modules.size()) ? _modules[i] : nullptr;
    }

    // Find the first module of a given type, or nullptr.
    Module* findByType(ModuleType t) const {
        for (Module* m : _modules) {
            if (m->type() == t) return m;
        }
        return nullptr;
    }

    static const char* connectorName(ModuleConnector c) {
        switch (c) {
            case ModuleConnector::CN1: return "CN1";
            case ModuleConnector::P3:  return "P3";
        }
        return "?";
    }

private:
    ModuleManager() = default;
    ModuleManager(const ModuleManager&) = delete;
    ModuleManager& operator=(const ModuleManager&) = delete;

    std::vector<Module*> _modules;
};
