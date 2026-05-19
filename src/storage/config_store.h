#pragma once
#include <Arduino.h>
#include <map>
#include <string>
#include "sd_manager.h"

// ConfigStore - in-memory key/value mirror of /pickle-os/sys/config.txt on SD.
class ConfigStore {
public:
    static ConfigStore& getInstance() {
        static ConfigStore instance;
        return instance;
    }

    static constexpr const char* PATH = "/pickle-os/sys/config.txt";

    void load() {
        _kv.clear();
        if (!SDManager::getInstance().exists(PATH)) return;
        String all = SDManager::getInstance().readFile(PATH);
        int start = 0;
        while (start <= (int)all.length()) {
            int nl = all.indexOf('\n', start);
            String line = (nl < 0) ? all.substring(start) : all.substring(start, nl);
            line.trim();
            int eq = line.indexOf('=');
            if (eq > 0) {
                String k = line.substring(0, eq);
                String v = line.substring(eq + 1);
                k.trim();
                _kv[std::string(k.c_str())] = std::string(v.c_str());
            }
            if (nl < 0) break;
            start = nl + 1;
        }
    }

    String get(const char* key, const char* defVal = "") const {
        auto it = _kv.find(key);
        if (it == _kv.end()) return String(defVal);
        return String(it->second.c_str());
    }

    bool getBool(const char* key, bool defVal = false) const {
        auto it = _kv.find(key);
        if (it == _kv.end()) return defVal;
        const std::string& v = it->second;
        return v == "1" || v == "true" || v == "yes" || v == "on";
    }

    void set(const char* key, const String& val) {
        _kv[key] = std::string(val.c_str());
        _flush();
    }

    void setBool(const char* key, bool val) {
        _kv[key] = val ? "1" : "0";
        _flush();
    }

private:
    void _flush() {
        if (!SDManager::getInstance().isMounted()) return;
        String out;
        for (auto& kv : _kv) {
            out += kv.first.c_str();
            out += "=";
            out += kv.second.c_str();
            out += "\n";
        }
        SDManager::getInstance().writeFile(PATH, out.c_str());
    }

    std::map<std::string, std::string> _kv;
};
