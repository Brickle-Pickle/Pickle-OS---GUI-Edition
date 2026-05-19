#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "esp_sntp.h"
#include "../storage/config_store.h"

// WifiManager - singleton handling WiFi credentials, connection state and NTP sync.
class WifiManager {
public:
    static WifiManager& getInstance() {
        static WifiManager instance;
        return instance;
    }

    void begin() {
        ConfigStore& cfg = ConfigStore::getInstance();
        _ssid    = cfg.get("wifi_ssid");
        _pass    = cfg.get("wifi_pass");
        _enabled = cfg.getBool("wifi_enabled", false);

        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);
        // Modem-sleep keeps the radio off between beacons; without it the WiFi
        // task starves the SPI/LVGL pipeline on the shared core causing visible
        // tearing and flicker on the CYD display.
        WiFi.setSleep(WIFI_PS_MIN_MODEM);

        configTzTime("CET-1CEST,M3.5.0,M10.5.0/3",
                     "pool.ntp.org", "time.google.com");
        setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
        tzset();

        if (_enabled && _ssid.length()) {
            connect();
        }
    }

    void setCredentials(const String& ssid, const String& pass) {
        _ssid = ssid;
        _pass = pass;
        ConfigStore& cfg = ConfigStore::getInstance();
        cfg.set("wifi_ssid", ssid);
        cfg.set("wifi_pass", pass);
        // Intentionally do NOT auto-reconnect here. Restarting the WiFi stack
    }

    void setEnabled(bool on) {
        if (_enabled == on) return;
        _enabled = on;
        ConfigStore::getInstance().setBool("wifi_enabled", on);
        if (on) connect();
        else    disconnect();
    }

    void connect() {
        if (!_ssid.length()) return;
        WiFi.begin(_ssid.c_str(), _pass.c_str());
    }

    void disconnect() {
        // Don't power-down the radio (first arg false) — keeps WiFi.mode and
        // SNTP service alive so re-enabling is a clean WiFi.begin() again.
        WiFi.disconnect(false, false);
    }

    // Force a manual NTP resync. Safe to call from UI; just nudges SNTP.
    void syncClock() {
        sntp_restart();
    }

    bool enabled()   const { return _enabled; }
    bool connected() const { return WiFi.status() == WL_CONNECTED; }
    const String& ssid() const { return _ssid; }
    String ip()      const { return WiFi.localIP().toString(); }

private:
    String _ssid;
    String _pass;
    bool   _enabled = false;
};

// Format current time as HH:MM into `out`. Returns false (and writes "--:--")
// when the clock has not yet been synced via NTP.
inline bool getClockHHMM(char* out, size_t n) {
    time_t now = time(nullptr);
    // 1700000000 ~ 2023-11-14, anything before that means NTP hasn't synced yet
    if (now < 1700000000) {
        strncpy(out, "--:--", n);
        if (n) out[n - 1] = '\0';
        return false;
    }
    // Defensively reassert TZ — sntp_restart()/sntp callbacks in some SDK
    // versions reset it back to UTC after a successful sync, making the
    // clock display 1–2 hours behind in Madrid.
    static bool _tzPinned = false;
    if (!_tzPinned) {
        setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
        tzset();
        _tzPinned = true;
    }
    struct tm tLocal, tUtc;
    localtime_r(&now, &tLocal);
    gmtime_r(&now, &tUtc);

    // newlib on ESP32 has no tm_gmtoff, so detect "TZ not applied" by
    // checking whether localtime() differs from gmtime(). If they're equal
    // the TZ env var isn't taking effect and we apply Madrid offset manually.
    bool tzApplied = (tLocal.tm_hour != tUtc.tm_hour) ||
                     (tLocal.tm_mday != tUtc.tm_mday);
    if (!tzApplied) {
        setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
        tzset();
        localtime_r(&now, &tLocal);
        tzApplied = (tLocal.tm_hour != tUtc.tm_hour) ||
                    (tLocal.tm_mday != tUtc.tm_mday);
    }
    if (!tzApplied) {
        // Manual fallback: assume CEST (DST) between last Sun of Mar and last
        // Sun of Oct. Good enough for the display when libc TZ doesn't work.
        int month = tUtc.tm_mon + 1;
        bool dst = (month > 3 && month < 10) ||
                   (month == 3 && tUtc.tm_mday >= 25) ||
                   (month == 10 && tUtc.tm_mday < 25);
        time_t adj = now + (dst ? 2 : 1) * 3600;
        gmtime_r(&adj, &tLocal);
    }
    snprintf(out, n, "%02d:%02d", tLocal.tm_hour, tLocal.tm_min);
    return true;
}
