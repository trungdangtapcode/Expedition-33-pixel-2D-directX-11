// ============================================================
// File: JsonLoader.h
// Responsibility: Minimal, header-only JSON parser for sprite sheet
//                 descriptor files.
//
// Scope:
//   This is NOT a general-purpose JSON library.  It parses exactly
//   the schema used by assets/animations/*.json.  It handles:
//     - Top-level string and integer fields
//     - A single "animations" array of objects
//     - String, integer, bool, and 2-element int-array values
//
//   For anything more complex (nested objects, floats in all contexts,
//   null values), replace this with a full library such as nlohmann/json.
//
// Why no external dependency?
//   The verso.json schema is small and fixed.  Pulling in a third-party
//   JSON library just for one data file would bloat the build and add a
//   vcpkg dependency that needs maintenance.  This parser is < 200 lines
//   and is trivially auditable.
//
// Usage:
//   SpriteSheet sheet;
//   if (!JsonLoader::LoadSpriteSheet("assets/animations/verso.json", sheet))
//       LOG("Failed to load sheet");
// ============================================================
#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include "../Renderer/SpriteSheet.h"
#include "Log.h"

namespace JsonLoader {

// ============================================================
// Internal helpers — not part of the public API
// ============================================================
namespace detail {

// ------------------------------------------------------------
// Trim leading and trailing whitespace (space, tab, CR, LF).
// Used to clean up values extracted from the JSON text.
// ------------------------------------------------------------
inline std::string Trim(const std::string& s)
{
    const char* ws = " \t\r\n";
    size_t start = s.find_first_not_of(ws);
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
}

// ------------------------------------------------------------
// Extract the raw text between the first '"' and the matching '"',
// searching from position pos in src.
// Returns empty string if no quoted value is found.
// ------------------------------------------------------------
inline std::string ParseString(const std::string& src, size_t pos = 0)
{
    size_t a = src.find('"', pos);
    if (a == std::string::npos) return "";
    size_t b = src.find('"', a + 1);
    if (b == std::string::npos) return "";
    return src.substr(a + 1, b - a - 1);
}

// ------------------------------------------------------------
// Find "key": <value> and return the raw string after the colon.
// Works for  "key": 123,  "key": "text",  "key": true, etc.
// Returns empty string if key is not present.
// ------------------------------------------------------------
inline std::string ValueOf(const std::string& src, const std::string& key)
{
    // Find the key including its surrounding quotes.
    std::string searchKey = "\"" + key + "\"";
    size_t kpos = src.find(searchKey);
    if (kpos == std::string::npos) return "";

    // Advance past the key and find the colon.
    size_t colon = src.find(':', kpos + searchKey.size());
    if (colon == std::string::npos) return "";

    // The value runs from after the colon to the next comma, ] or }.
    size_t vstart = colon + 1;
    size_t vend   = src.find_first_of(",}]", vstart);
    if (vend == std::string::npos) vend = src.size();

    return Trim(src.substr(vstart, vend - vstart));
}

// ------------------------------------------------------------
// Parse an integer from the raw value string returned by ValueOf.
// Returns defaultVal if the string is empty or not a valid integer.
// ------------------------------------------------------------
inline int ParseInt(const std::string& raw, int defaultVal = 0)
{
    if (raw.empty()) return defaultVal;
    try { return std::stoi(raw); }
    catch (...) { return defaultVal; }
}

// ------------------------------------------------------------
// Parse a boolean ("true" / "false") from a raw value string.
// ------------------------------------------------------------
inline bool ParseBool(const std::string& raw, bool defaultVal = false)
{
    if (raw == "true")  return true;
    if (raw == "false") return false;
    return defaultVal;
}

// ------------------------------------------------------------
// Parse the "align" string from a JSON clip object into a SpriteAlign enum.
// Supported values match the nine standard anchor points:
//   "top-left"      "top-center"      "top-right"
//   "middle-left"   "middle-center"   "middle-right"
//   "bottom-left"   "bottom-center"   "bottom-right"
// Any unrecognized value returns SpriteAlign::Unknown (renderer falls back
// to BottomCenter so the sprite is never silently invisible).
// ------------------------------------------------------------
inline SpriteAlign ParseAlign(const std::string& raw)
{
    // raw still has surrounding quotes from ValueOf() — strip them.
    std::string s = raw;
    if (s.size() >= 2 && s.front() == '"') s = s.substr(1, s.size() - 2);

    if (s == "top-left")      return SpriteAlign::TopLeft;
    if (s == "top-center")    return SpriteAlign::TopCenter;
    if (s == "top-right")     return SpriteAlign::TopRight;
    if (s == "middle-left")   return SpriteAlign::MiddleLeft;
    if (s == "middle-center") return SpriteAlign::MiddleCenter;
    if (s == "middle-right")  return SpriteAlign::MiddleRight;
    if (s == "bottom-left")   return SpriteAlign::BottomLeft;
    if (s == "bottom-center") return SpriteAlign::BottomCenter;
    if (s == "bottom-right")  return SpriteAlign::BottomRight;
    return SpriteAlign::Unknown;
}

// ------------------------------------------------------------
// Parse a 2-element integer array "[x, y]".
// Fills outX and outY.  Returns false if parsing fails.
// ------------------------------------------------------------
inline bool ParseIntArray2(const std::string& src, const std::string& key,
                           int& outX, int& outY)
{
    std::string searchKey = "\"" + key + "\"";
    size_t kpos = src.find(searchKey);
    if (kpos == std::string::npos) return false;

    size_t bracket = src.find('[', kpos);
    size_t close   = src.find(']', bracket);
    if (bracket == std::string::npos || close == std::string::npos) return false;

    std::string inner = src.substr(bracket + 1, close - bracket - 1);
    size_t comma = inner.find(',');
    if (comma == std::string::npos) return false;

    outX = ParseInt(Trim(inner.substr(0, comma)));
    outY = ParseInt(Trim(inner.substr(comma + 1)));
    return true;
}

// ------------------------------------------------------------
// Extract the text of the JSON array value for "animations": [...].
// Returns the block between the matching [ and ] braces.
// ------------------------------------------------------------
inline std::string ExtractAnimationsArray(const std::string& src)
{
    size_t kpos = src.find("\"animations\"");
    if (kpos == std::string::npos) return "";
    size_t bracket = src.find('[', kpos);
    if (bracket == std::string::npos) return "";

    // Walk forward tracking brace depth to find the matching ']'.
    int depth = 1;
    size_t i  = bracket + 1;
    while (i < src.size() && depth > 0) {
        if (src[i] == '[') ++depth;
        if (src[i] == ']') --depth;
        ++i;
    }
    // i now points one past the closing ']'.
    return src.substr(bracket + 1, i - bracket - 2);
}

// ------------------------------------------------------------
// Split the animations array text into individual object blocks "{...}".
// Each block is the raw JSON text for one AnimationClip.
// ------------------------------------------------------------
inline std::vector<std::string> SplitObjects(const std::string& arrayText)
{
    std::vector<std::string> objects;
    size_t i = 0;
    while (i < arrayText.size()) {
        size_t open = arrayText.find('{', i);
        if (open == std::string::npos) break;

        int depth = 1;
        size_t j  = open + 1;
        while (j < arrayText.size() && depth > 0) {
            if (arrayText[j] == '{') ++depth;
            if (arrayText[j] == '}') --depth;
            ++j;
        }
        objects.push_back(arrayText.substr(open + 1, j - open - 2));
        i = j;
    }
    return objects;
}

} // namespace detail

// ============================================================
// Public API
// ============================================================

// ------------------------------------------------------------
// Function: LoadSpriteSheet
// Purpose:
//   Read a JSON sprite sheet descriptor from disk and populate
//   a SpriteSheet struct.
// Parameters:
//   path  — path to the .json file (UTF-8, relative or absolute)
//   sheet — output struct; overwritten on success
// Returns:
//   true  — all required fields parsed successfully
//   false — file not found or required fields missing
// ------------------------------------------------------------
inline bool LoadSpriteSheet(const std::string& path, SpriteSheet& sheet)
{
    // Read the entire file into a std::string.
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG("[JsonLoader] Cannot open file: '%s'", path.c_str());
        return false;
    }
    std::ostringstream buf;
    buf << file.rdbuf();
    const std::string src = buf.str();

    // --- Top-level scalar fields ---
    sheet.spriteName  = detail::ParseString(detail::ValueOf(src, "sprite_name").empty()
                            ? src : detail::ValueOf(src, "sprite_name") + "\"", 0);
    // Re-parse properly: ValueOf returns the raw token (may include quotes for strings).
    {
        // For string fields the raw value is "\"text\"", strip the quotes.
        auto rawName = detail::ValueOf(src, "sprite_name");
        auto rawChar = detail::ValueOf(src, "character");
        // Remove surrounding quotes if present.
        auto stripQ = [](const std::string& s) -> std::string {
            if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
                return s.substr(1, s.size() - 2);
            return s;
        };
        sheet.spriteName = stripQ(rawName);
        sheet.character  = stripQ(rawChar);
    }

    sheet.sheetWidth   = detail::ParseInt(detail::ValueOf(src, "width"));
    sheet.sheetHeight  = detail::ParseInt(detail::ValueOf(src, "height"));
    sheet.frameWidth   = detail::ParseInt(detail::ValueOf(src, "frame_width"));
    sheet.frameHeight  = detail::ParseInt(detail::ValueOf(src, "frame_height"));

    if (sheet.frameWidth == 0 || sheet.frameHeight == 0) {
        LOG("[JsonLoader] Invalid frame dimensions in '%s'.", path.c_str());
        return false;
    }

    // --- Parse animations array ---
    std::string arrayText = detail::ExtractAnimationsArray(src);
    auto objects = detail::SplitObjects(arrayText);

    sheet.animations.clear();
    sheet.animations.reserve(objects.size());

    for (const auto& obj : objects) {
        AnimationClip clip;

        // Strip quotes from string values.
        auto rawName = detail::ValueOf(obj, "name");
        if (!rawName.empty() && rawName.front() == '"') {
            clip.name = rawName.substr(1, rawName.size() - 2);
        } else {
            clip.name = rawName;
        }

        clip.numFrames = detail::ParseInt(detail::ValueOf(obj, "num_frames"));
        clip.frameRate = static_cast<float>(
                            detail::ParseInt(detail::ValueOf(obj, "frame_rate"), 8));
        clip.loop      = detail::ParseBool(detail::ValueOf(obj, "loop"), true);

        // Parse "pivot": [x, y]
        clip.pivotX = 0;
        clip.pivotY = 0;
        detail::ParseIntArray2(obj, "pivot", clip.pivotX, clip.pivotY);

        // Parse "align": "bottom-center" etc.
        // Unknown values default to BottomCenter in the renderer.
        clip.align = detail::ParseAlign(detail::ValueOf(obj, "align"));

        if (clip.name.empty() || clip.numFrames <= 0) {
            LOG("[JsonLoader] Skipping malformed clip in '%s'.", path.c_str());
            continue;
        }

        sheet.animations.push_back(std::move(clip));
    }

    LOG("[JsonLoader] Loaded '%s': %dx%d, frames %dx%d, %d clip(s).",
        path.c_str(),
        sheet.sheetWidth, sheet.sheetHeight,
        sheet.frameWidth, sheet.frameHeight,
        (int)sheet.animations.size());

    return true;
}

} // namespace JsonLoader
