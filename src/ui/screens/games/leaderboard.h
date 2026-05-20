#pragma once
#include <Arduino.h>
#include <vector>
#include <algorithm>
#include "../../../storage/sd_manager.h"

// Single leaderboard entry: player name and score
struct LbEntry {
    String name;
    int    score;
};

// Leaderboard helper - reads and writes /pickle-os/games/{game}.txt
// File format: one line per score, "NAME,SCORE" (newline-terminated).
class Leaderboard {
public:
    // Load all entries from the file, sorted descending by score, truncated to topN.
    static std::vector<LbEntry> load(const char* game, int topN = 5) {
        std::vector<LbEntry> out;
        String path = _pathFor(game);
        if (!SDManager::getInstance().exists(path.c_str())) return out;
        String raw = SDManager::getInstance().readFile(path.c_str());
        int start = 0;
        while (start < (int)raw.length()) {
            int nl = raw.indexOf('\n', start);
            if (nl < 0) nl = raw.length();
            String line = raw.substring(start, nl);
            start = nl + 1;
            line.trim();
            if (line.length() == 0) continue;
            int comma = line.indexOf(',');
            if (comma < 0) continue;
            LbEntry e;
            e.name  = line.substring(0, comma);
            e.score = line.substring(comma + 1).toInt();
            out.push_back(e);
        }
        std::sort(out.begin(), out.end(), [](const LbEntry& a, const LbEntry& b) {
            return a.score > b.score;
        });
        if ((int)out.size() > topN) out.resize(topN);
        return out;
    }

    // Append a new entry. Sanitizes the name (max 10 chars, no commas/newlines).
    static bool save(const char* game, const String& name, int score) {
        if (!SDManager::getInstance().exists("/pickle-os/games")) {
            SDManager::getInstance().makeDir("/pickle-os/games");
        }
        String n = name;
        n.trim();
        if (n.length() == 0) n = "Player";
        if (n.length() > 10) n = n.substring(0, 10);
        n.replace(',', '_');
        n.replace('\n', '_');
        n.replace('\r', '_');
        String path = _pathFor(game);
        String line = n + "," + String(score) + "\n";
        return SDManager::getInstance().writeFile(path.c_str(), line.c_str(), true);
    }

private:
    static String _pathFor(const char* game) {
        return String("/pickle-os/games/") + game + ".txt";
    }
};
