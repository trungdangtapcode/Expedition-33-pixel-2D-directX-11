// ============================================================
// File: HealthBarConfig.h
// Responsibility: Plain data struct describing the layout of an HP bar UI
//                 sprite sheet, loaded from a JSON descriptor file.
//
// JSON schema (HP_description.json):
//   {
//     "width":  256,             <- full texture width  in pixels
//     "height": 256,             <- full texture height in pixels
//     "health_bar_topleft":     [60, 174],   <- fill area top-left pixel
//     "health_bar_bottomright": [186, 189],  <- fill area bottom-right pixel
//     "mana_bar_topleft":       [60, 190],
//     "mana_bar_bottomright":   [186, 204],
//     "align": "top-left"        <- screen anchor
//   }
//
// Ownership:
//   HealthBarConfig is a VALUE type — no GPU resources, no virtual methods.
//   HealthBarRenderer owns one by value.
//
// Parsing:
//   LoadFromJson() is a minimal hand-written parser that reads only the
//   fields this system needs.  It avoids a third-party JSON library
//   dependency while staying data-driven.
//
// Common mistakes:
//   1. Passing a path relative to the executable — GameApp::Initialize()
//      sets the working directory to the workspace root, so all paths must
//      be relative to there (e.g. "assets/UI/HP_description.json").
//   2. Assuming ParseIntArray always succeeds — check return value.
// ============================================================
#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include "../Utils/Log.h"

struct HealthBarConfig
{
    // Full texture dimensions (the background and frame sheets are the same size).
    int textureWidth  = 256;
    int textureHeight = 256;

    // Pixel coordinates of the HP fill area WITHIN the background texture.
    // The fill is drawn by clipping the source RECT right edge:
    //   fillWidth = (health_bar_bottomright.x - health_bar_topleft.x) * hpRatio
    int hpBarLeft   = 0;
    int hpBarTop    = 0;
    int hpBarRight  = 0;
    int hpBarBottom = 0;

    // Pixel coordinates of the MP fill area (reserved for future use).
    int mpBarLeft   = 0;
    int mpBarTop    = 0;
    int mpBarRight  = 0;
    int mpBarBottom = 0;

    // Derived: full pixel width / height of the HP fill strip.
    int HpBarWidth()  const { return hpBarRight  - hpBarLeft; }
    int HpBarHeight() const { return hpBarBottom - hpBarTop;  }
    int MpBarWidth()  const { return mpBarRight  - mpBarLeft; }
    int MpBarHeight() const { return mpBarBottom - mpBarTop;  }

    // ----------------------------------------------------------------
    // LoadFromJson
    // Purpose:
    //   Parse HP_description.json and populate this struct.
    //   Uses a minimal hand-written token search — no third-party library.
    //
    // Returns true on success.  Logs and returns false on any error.
    //
    // How the parser works:
    //   1. Read the entire file into a string.
    //   2. For each known field name, find the first occurrence of the
    //      quoted key, then scan forward past ": " to the value.
    //   3. For integer arrays like [60, 174], parse the two numbers.
    //   4. For integers like 256, parse the single number.
    // ----------------------------------------------------------------
    bool LoadFromJson(const std::string& path)
    {
        // -- Open file --
        std::ifstream file(path);
        if (!file.is_open())
        {
            LOG("[HealthBarConfig] Cannot open '%s'", path.c_str());
            return false;
        }

        // Slurp entire file into one string for easy substring search.
        std::ostringstream ss;
        ss << file.rdbuf();
        const std::string json = ss.str();

        // ----------------------------------------------------------------
        // Helper: find an integer after "key": <value>
        // Returns INT_MIN if the key is not found.
        // ----------------------------------------------------------------
        auto readInt = [&](const std::string& key, int& out) -> bool
        {
            const std::string search = "\"" + key + "\"";
            const std::size_t pos = json.find(search);
            if (pos == std::string::npos) return false;

            // Scan past the key + ": " to the number.
            std::size_t numPos = json.find_first_of("0123456789", pos + search.size());
            if (numPos == std::string::npos) return false;

            out = std::stoi(json.substr(numPos));
            return true;
        };

        // ----------------------------------------------------------------
        // Helper: parse [x, y] array after "key": [x, y]
        // Fills out[0] and out[1].
        // ----------------------------------------------------------------
        auto readIntPair = [&](const std::string& key, int& x, int& y) -> bool
        {
            const std::string search = "\"" + key + "\"";
            const std::size_t keyPos = json.find(search);
            if (keyPos == std::string::npos) return false;

            // Find the opening '[' after the key.
            const std::size_t bracket = json.find('[', keyPos + search.size());
            if (bracket == std::string::npos) return false;

            // Parse first integer.
            std::size_t p = json.find_first_of("0123456789", bracket + 1);
            if (p == std::string::npos) return false;
            x = std::stoi(json.substr(p));

            // Move past first integer and comma to second integer.
            std::size_t comma = json.find(',', p);
            if (comma == std::string::npos) return false;
            p = json.find_first_of("0123456789", comma + 1);
            if (p == std::string::npos) return false;
            y = std::stoi(json.substr(p));

            return true;
        };

        // -- Parse all fields --
        bool ok = true;
        ok &= readInt("width",  textureWidth);
        ok &= readInt("height", textureHeight);

        int hpTlX = 0, hpTlY = 0, hpBrX = 0, hpBrY = 0;
        ok &= readIntPair("health_bar_topleft",     hpTlX, hpTlY);
        ok &= readIntPair("health_bar_bottomright", hpBrX, hpBrY);

        int mpTlX = 0, mpTlY = 0, mpBrX = 0, mpBrY = 0;
        // MP bar is optional — do not fail if absent.
        readIntPair("mana_bar_topleft",     mpTlX, mpTlY);
        readIntPair("mana_bar_bottomright", mpBrX, mpBrY);

        if (!ok)
        {
            LOG("[HealthBarConfig] Failed to parse one or more required fields in '%s'",
                path.c_str());
            return false;
        }

        hpBarLeft   = hpTlX;
        hpBarTop    = hpTlY;
        hpBarRight  = hpBrX;
        hpBarBottom = hpBrY;

        mpBarLeft   = mpTlX;
        mpBarTop    = mpTlY;
        mpBarRight  = mpBrX;
        mpBarBottom = mpBrY;

        LOG("[HealthBarConfig] Loaded from '%s' — HP bar: [%d,%d]→[%d,%d]",
            path.c_str(), hpBarLeft, hpBarTop, hpBarRight, hpBarBottom);
        return true;
    }
};
