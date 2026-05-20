#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include "../storage/config_store.h"

// Fallback if the SD config does not provide a game_server entry.
#define PICKLE_GAME_SERVER_DEFAULT "https://pickle-os-game-store.onrender.com/"

// GameApi - thin singleton wrapping HTTPClient calls against the Pickle OS
// game store backend. All requests are blocking and meant to be issued from a
// dedicated FreeRTOS task, never from the LVGL task.
class GameApi {
public:
    static GameApi& getInstance() {
        static GameApi instance;
        return instance;
    }

    // Returns the configured base URL with any trailing slash stripped.
    String baseUrl() const {
        String url = ConfigStore::getInstance().get("game_server", PICKLE_GAME_SERVER_DEFAULT);
        if (url.endsWith("/")) url.remove(url.length() - 1);
        return url;
    }

    // Fetches the list of available games as a raw JSON string.
    // Returns an empty string on transport failure.
    String fetchCatalog() {
        return _get(baseUrl() + "/api/games");
    }

    // Fetches the full manifest for a single game as a raw JSON string.
    String fetchManifest(const String& id) {
        return _get(baseUrl() + "/api/games/" + id);
    }

    // Downloads an asset file under games/<id>/files/<filename>.
    String fetchAsset(const String& id, const String& filename) {
        return _get(baseUrl() + "/api/games/" + id + "/files/" + filename);
    }

private:
    String _get(const String& url) {
        HTTPClient http;
        // Render free tier sleeps after 15 min idle; first request takes ~30 s
        // to wake the dyno. 45 s gives margin without hanging the UI forever.
        http.setTimeout(45000);
        http.setConnectTimeout(15000);
        http.setReuse(false);
        if (!http.begin(url)) {
            Serial.printf("[GameApi] begin() failed for %s\n", url.c_str());
            return "";
        }
        int code = http.GET();
        if (code != HTTP_CODE_OK) {
            Serial.printf("[GameApi] HTTP %d on %s\n", code, url.c_str());
            http.end();
            return "";
        }
        String body = http.getString();
        http.end();
        return body;
    }
};
