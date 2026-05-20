#pragma once
#include <Arduino.h>
#include <vector>
#include <SD.h>
#include "sd_manager.h"

// GameInstaller - manages the on-disk layout for downloaded games under
// /pickle-os/games-hub/{id}/. Each game owns a manifest.json plus optional
// asset files received from the backend.
class GameInstaller {
public:
    static constexpr const char* ROOT = "/pickle-os/games-hub";

    static GameInstaller& getInstance() {
        static GameInstaller instance;
        return instance;
    }

    // Make sure the games-hub directory tree exists.
    void ensureRoot() {
        SDManager& sd = SDManager::getInstance();
        if (!sd.isMounted()) return;
        if (!sd.exists("/pickle-os")) sd.makeDir("/pickle-os");
        if (!sd.exists(ROOT)) sd.makeDir(ROOT);
    }

    // Lists the IDs of all installed games (any subdir of ROOT containing
    // manifest.json). Returns an empty list if SD is not ready.
    std::vector<String> listInstalled() {
        std::vector<String> out;
        ensureRoot();
        if (!SDManager::getInstance().isMounted()) return out;

        File dir = SD.open(ROOT);
        if (!dir || !dir.isDirectory()) return out;
        File entry = dir.openNextFile();
        while (entry) {
            if (entry.isDirectory()) {
                String name = entry.name();
                int slash = name.lastIndexOf('/');
                String id = (slash >= 0) ? name.substring(slash + 1) : name;
                String manifest = String(ROOT) + "/" + id + "/manifest.json";
                if (SD.exists(manifest)) out.push_back(id);
            }
            entry.close();
            entry = dir.openNextFile();
        }
        dir.close();
        return out;
    }

    bool isInstalled(const String& id) {
        return SDManager::getInstance().exists(_manifestPath(id).c_str());
    }

    String manifestPath(const String& id) { return _manifestPath(id); }

    // Reads the stored manifest as a raw JSON string. Empty on failure.
    String readManifest(const String& id) {
        return SDManager::getInstance().readFile(_manifestPath(id).c_str());
    }

    // Writes (or overwrites) the manifest for a game. Creates the folder if needed.
    bool writeManifest(const String& id, const String& json) {
        ensureRoot();
        SDManager& sd = SDManager::getInstance();
        String dir = String(ROOT) + "/" + id;
        if (!sd.exists(dir.c_str())) sd.makeDir(dir.c_str());
        return sd.writeFile(_manifestPath(id).c_str(), json.c_str());
    }

    // Writes an asset file inside the game folder.
    bool writeAsset(const String& id, const String& filename, const String& content) {
        ensureRoot();
        SDManager& sd = SDManager::getInstance();
        String dir = String(ROOT) + "/" + id;
        if (!sd.exists(dir.c_str())) sd.makeDir(dir.c_str());
        String path = dir + "/" + filename;
        return sd.writeFile(path.c_str(), content.c_str());
    }

    // Removes the game folder and every file under it. Collects the child
    // paths first and closes the dir before deleting, so the iterator never
    // walks a mutating directory. Each child path is rebuilt as
    // ROOT/<id>/<basename> because Arduino-ESP32's File::name() may return
    // just the basename depending on the SD driver version.
    bool uninstall(const String& id) {
        SDManager& sd = SDManager::getInstance();
        if (!sd.isMounted()) return false;
        String dir = String(ROOT) + "/" + id;
        if (!sd.exists(dir.c_str())) return true;
        std::vector<String> files;
        File d = SD.open(dir.c_str());
        if (d && d.isDirectory()) {
            File f = d.openNextFile();
            while (f) {
                if (!f.isDirectory()) {
                    String raw = f.name();
                    int slash = raw.lastIndexOf('/');
                    String basename = (slash >= 0) ? raw.substring(slash + 1) : raw;
                    files.push_back(dir + "/" + basename);
                }
                f.close();
                f = d.openNextFile();
            }
            d.close();
        }
        for (const String& path : files) {
            if (!SD.remove(path.c_str())) {
                Serial.printf("[GameInstaller] remove failed: %s\n", path.c_str());
            }
        }
        return sd.removeDir(dir.c_str());
    }

private:
    String _manifestPath(const String& id) {
        return String(ROOT) + "/" + id + "/manifest.json";
    }
};
