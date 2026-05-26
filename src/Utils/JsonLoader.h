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
#include <filesystem>
#include <vector>
#include "Log.h"
#include "../Renderer/SpriteSheet.h"
#include "../Battle/StatModifier.h"
#include "../Battle/EnemyEncounterData.h"
#include "../Battle/BattlerStats.h"
#include "../Battle/ItemData.h"
#include "../Systems/ICollisionSystem.h"

namespace JsonLoader {

// ============================================================
// Internal helpers - not part of the public API
// ============================================================
namespace detail {

// ------------------------------------------------------------
// Warn if the file is UTF-16. std::ifstream fails to parse UTF-16
// correctly because of interspersed null bytes.
// ------------------------------------------------------------
inline void WarnIfUTF16(const std::string& src, const std::string& path)
{
    if (src.size() >= 2) {
        unsigned char b1 = static_cast<unsigned char>(src[0]);
        unsigned char b2 = static_cast<unsigned char>(src[1]);
        if ((b1 == 0xFF && b2 == 0xFE) || (b1 == 0xFE && b2 == 0xFF)) {
            LOG("[JsonLoader] FATAL ERROR: File '%s' is saved as UTF-16! C++ parser requires UTF-8. Please re-save your file.", path.c_str());
        }
    }
}

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
// Parse a float from the raw value string returned by ValueOf.
// Returns defaultVal if the string is empty or not a valid number.
// Used for fields like contactRadius and cameraFocusOffsetY that
// JSON authors express as decimal literals (e.g. 90.0, -128.0).
// ------------------------------------------------------------
inline float ParseFloat(const std::string& raw, float defaultVal = 0.0f)
{
    if (raw.empty()) return defaultVal;
    try { return std::stof(raw); }
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
// Parse string dynamically cleanly removing internal bounding quotes
// ------------------------------------------------------------
inline std::string CleanString(const std::string& raw)
{
    std::string s = raw;
    if (s.size() >= 2 && s.front() == '"') s = s.substr(1, s.size() - 2);
    return s;
}

// ------------------------------------------------------------
// Extract a shallow array of JSON strings from a named field.
// Used for data files that need a small list of asset paths without
// pulling in a full JSON dependency.
// ------------------------------------------------------------
inline std::vector<std::string> ExtractStringArray(const std::string& src, const std::string& key)
{
    std::vector<std::string> values;
    const std::string searchKey = "\"" + key + "\"";
    const size_t keyPos = src.find(searchKey);
    if (keyPos == std::string::npos) return values;

    const size_t openBracket = src.find('[', keyPos + searchKey.size());
    if (openBracket == std::string::npos) return values;

    int depth = 1;
    size_t cursor = openBracket + 1;
    while (cursor < src.size() && depth > 0) {
        if (src[cursor] == '[') ++depth;
        if (src[cursor] == ']') --depth;
        ++cursor;
    }

    if (depth != 0 || cursor <= openBracket + 1) return values;

    const std::string body = src.substr(openBracket + 1, cursor - openBracket - 2);
    size_t scan = 0;
    while (scan < body.size()) {
        const size_t quoteStart = body.find('"', scan);
        if (quoteStart == std::string::npos) break;

        const size_t quoteEnd = body.find('"', quoteStart + 1);
        if (quoteEnd == std::string::npos) break;

        values.push_back(body.substr(quoteStart + 1, quoteEnd - quoteStart - 1));
        scan = quoteEnd + 1;
    }

    return values;
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
    // raw still has surrounding quotes from ValueOf() - strip them.
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

// ------------------------------------------------------------
// Extract an array of raw JSON object strings from a named array key.
// Returns one string per { ... } object found inside the array.
//
// Example input:  "battleParty": [ { "hp": 50 }, { "hp": 30 } ]
// Returns: [ "{ \"hp\": 50 }", "{ \"hp\": 30 }" ]
//
// Handles nested braces correctly via depth tracking.
// Returns an empty vector if the key is not found.
// ------------------------------------------------------------
inline std::vector<std::string> ExtractObjectsFromArray(
    const std::string& src, const std::string& arrayKey)
{
    std::vector<std::string> objects;
    const std::string searchKey = "\"" + arrayKey + "\"";
    const size_t kpos = src.find(searchKey);
    if (kpos == std::string::npos) return objects;

    const size_t arrStart = src.find('[', kpos);
    if (arrStart == std::string::npos) return objects;

    size_t i = arrStart + 1;
    while (i < src.size())
    {
        // Skip whitespace and commas between objects.
        while (i < src.size() &&
               (src[i] == ' ' || src[i] == '\t' ||
                src[i] == '\r' || src[i] == '\n' || src[i] == ','))
            ++i;

        if (i >= src.size() || src[i] == ']') break;

        if (src[i] == '{')
        {
            // Walk forward tracking brace depth to find the matching '}'.
            int    depth    = 1;
            size_t objStart = i;
            ++i;
            while (i < src.size() && depth > 0)
            {
                if (src[i] == '{') ++depth;
                if (src[i] == '}') --depth;
                ++i;
            }
            // i now points one past the closing '}' - include the whole block.
            objects.push_back(src.substr(objStart, i - objStart));
        }
        else
        {
            ++i;
        }
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
//   path  - path to the .json file (UTF-8, relative or absolute)
//   sheet - output struct; overwritten on success
// Returns:
//   true  - all required fields parsed successfully
//   false - file not found or required fields missing
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

    for (int clipIndex = 0; clipIndex < static_cast<int>(objects.size()); ++clipIndex)
    {
        const auto& obj = objects[clipIndex];
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

        // Each clip occupies its own row in the atlas.
        // The i-th clip in the animations array lives on row i (0-based).
        // This convention means the atlas layout must match the JSON order:
        //   animations[0] -> row 0 (top row)
        //   animations[1] -> row 1
        //   ...
        clip.startRow = clipIndex;

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

// ============================================================
// Formation data structures
// ============================================================

// One slot entry inside a formation -
//   offsetX/offsetY are world-space units relative to the battle center.
//   Positive Y is downward (screen convention).  Represents the ground
//   contact point (feet) of the character assigned to this slot.
struct FormationSlot
{
    int   slot    = 0;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
};

// All slots for both teams in one formation file.
struct FormationData
{
    FormationSlot player[3];   // up to 3 player slots (indices 0-2)
    FormationSlot enemy [3];   // up to 3 enemy  slots (indices 0-2)
};

// ------------------------------------------------------------
// Function: LoadFormations
// Purpose:
//   Parse a formations.json file and fill a FormationData struct.
//   Both "player_offsets" and "enemy_offsets" arrays are read;
//   each element provides { slot, offset_x, offset_y }.
//
//   Offsets are world-space pixels relative to the battle center.
//   The caller computes final world positions as:
//     worldX = battleCenterX + slot.offsetX
//     worldY = battleCenterY + slot.offsetY
//
// Parameters:
//   path - path to the JSON file (e.g. "data/formations.json")
//   out  - populated on success; left unchanged on failure
// Returns:
//   true on success, false if the file cannot be opened.
// ------------------------------------------------------------
inline bool LoadFormations(const std::string& path, FormationData& out)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG("[JsonLoader] Cannot open formations file: '%s'", path.c_str());
        return false;
    }
    std::ostringstream buf;
    buf << file.rdbuf();
    const std::string src = buf.str();

    // Helper: extract the text of a named top-level array "key": [ ... ]
    auto extractArray = [&](const std::string& key) -> std::string
    {
        std::string searchKey = '"' + key + '"';
        size_t kpos = src.find(searchKey);
        if (kpos == std::string::npos) return {};
        size_t bracket = src.find('[', kpos + searchKey.size());
        if (bracket == std::string::npos) return {};
        int depth = 1;
        size_t i  = bracket + 1;
        while (i < src.size() && depth > 0) {
            if (src[i] == '[') ++depth;
            if (src[i] == ']') --depth;
            ++i;
        }
        return src.substr(bracket + 1, i - bracket - 2);
    };

    // Fill a FormationSlot[3] array from the raw array text.
    auto parseSlots = [](const std::string& arrayText, FormationSlot (&slots)[3])
    {
        // Initialise all three with sequential defaults so missing entries
        // still have sane slot indices.
        for (int i = 0; i < 3; ++i) slots[i] = { i, 0.0f, 0.0f };

        auto objects = detail::SplitObjects(arrayText);
        for (const auto& obj : objects)
        {
            int   s  = detail::ParseInt  (detail::ValueOf(obj, "slot"));
            float ox = static_cast<float>(detail::ParseInt(detail::ValueOf(obj, "offset_x")));
            float oy = static_cast<float>(detail::ParseInt(detail::ValueOf(obj, "offset_y")));
            if (s >= 0 && s < 3)
            {
                slots[s].slot    = s;
                slots[s].offsetX = ox;
                slots[s].offsetY = oy;
            }
        }
    };

    parseSlots(extractArray("player_offsets"), out.player);
    parseSlots(extractArray("enemy_offsets"),  out.enemy);

    LOG("[JsonLoader] Loaded formations from '%s'.", path.c_str());
    return true;
}

// ------------------------------------------------------------
// Function: LoadCharacterData
// Purpose:
//   Parse a data/characters/*.json file directly into BattlerStats.
// ------------------------------------------------------------
inline bool LoadCharacterData(const std::string& path, BattlerStats& out)
{
    namespace fs = std::filesystem;

    fs::path resolvedPath(path);
    std::ifstream file;
    file.open(resolvedPath);

    if (!file.is_open() && !resolvedPath.is_absolute()) {
        resolvedPath = fs::path("..") / path;
        file.clear();
        file.open(resolvedPath);
    }

    if (!file.is_open()) {
        LOG("[JsonLoader] Cannot open character file: '%s'", path.c_str());
        return false;
    }

    std::ostringstream buf;
    buf << file.rdbuf();
    const std::string src = buf.str();

    detail::WarnIfUTF16(src, path);

    out.hp      = detail::ParseInt(detail::ValueOf(src, "hp"), 100);
    out.maxHp   = detail::ParseInt(detail::ValueOf(src, "maxHp"), 100);
    out.mp      = detail::ParseInt(detail::ValueOf(src, "mp"), 50);
    out.maxMp   = detail::ParseInt(detail::ValueOf(src, "maxMp"), 50);
    out.atk     = detail::ParseInt(detail::ValueOf(src, "atk"), 25);
    out.def     = detail::ParseInt(detail::ValueOf(src, "def"), 10);
    out.matk    = detail::ParseInt(detail::ValueOf(src, "matk"), 25);
    out.mdef    = detail::ParseInt(detail::ValueOf(src, "mdef"), 10);
    out.spd     = detail::ParseInt(detail::ValueOf(src, "spd"), 10);
    out.rage    = detail::ParseInt(detail::ValueOf(src, "rage"), 0);
    out.maxRage = detail::ParseInt(detail::ValueOf(src, "maxRage"), 100);

    out.level   = detail::ParseInt(detail::ValueOf(src, "level"), 1);
    out.exp     = detail::ParseInt(detail::ValueOf(src, "exp"), 0);

    out.growth.maxHp = detail::ParseInt(detail::ValueOf(src, "growth_maxHp"), 0);
    out.growth.maxMp = detail::ParseInt(detail::ValueOf(src, "growth_maxMp"), 0);
    out.growth.atk   = detail::ParseInt(detail::ValueOf(src, "growth_atk"), 0);
    out.growth.def   = detail::ParseInt(detail::ValueOf(src, "growth_def"), 0);
    out.growth.matk  = detail::ParseInt(detail::ValueOf(src, "growth_matk"), 0);
    out.growth.mdef  = detail::ParseInt(detail::ValueOf(src, "growth_mdef"), 0);
    out.growth.spd   = detail::ParseInt(detail::ValueOf(src, "growth_spd"), 0);

    LOG("[JsonLoader] Loaded CharacterData from '%s'.", path.c_str());
    return true;
}

inline std::vector<std::string> LoadStringArrayFromFile(const std::string& path, const std::string& key)
{
    namespace fs = std::filesystem;

    fs::path resolvedPath(path);
    std::ifstream file(resolvedPath);
    if (!file.is_open() && !resolvedPath.is_absolute())
    {
        resolvedPath = fs::path("..") / path;
        file.clear();
        file.open(resolvedPath);
    }

    if (!file.is_open())
    {
        LOG("[JsonLoader] Cannot open string-array file: '%s'", path.c_str());
        return {};
    }

    std::ostringstream buf;
    buf << file.rdbuf();
    const std::string src = buf.str();
    detail::WarnIfUTF16(src, path);
    return detail::ExtractStringArray(src, key);
}

// ------------------------------------------------------------
// Function: LoadDeadOverlayConfig
// ------------------------------------------------------------
struct DeadOverlayConfig {
    float width = 1254.0f;
    float height = 1254.0f;
    float scaleTarget = 256.0f; // natively map to your frame bounds or tweak individually
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float pivotX = 627.0f; // Native texture center
    float pivotY = 627.0f; // Native texture center
};

inline bool LoadDeadOverlayConfig(const std::string& path, DeadOverlayConfig& out)
{
    namespace fs = std::filesystem;
    fs::path resolvedPath(path);
    std::ifstream file(resolvedPath);

    // Support both workspace-root cwd and bin/ cwd at runtime.
    if (!file.is_open() && !resolvedPath.is_absolute()) {
        resolvedPath = fs::path("..") / path;
        file.clear(); // Important: must clear fail bit before open
        file.open(resolvedPath);
    }

    if (!file.is_open()) {
        LOG("[JsonLoader] Cannot open dead overlay config file: '%s'", path.c_str());
        return false;
    }

    std::ostringstream buf; buf << file.rdbuf();
    const std::string src = buf.str();

    out.width = detail::ParseFloat(detail::ValueOf(src, "width"), 1254.0f);
    out.height = detail::ParseFloat(detail::ValueOf(src, "height"), 1254.0f);
    out.scaleTarget = detail::ParseFloat(detail::ValueOf(src, "scaleTarget"), 256.0f);
    out.offsetX = detail::ParseFloat(detail::ValueOf(src, "offsetX"), 0.0f);
    out.offsetY = detail::ParseFloat(detail::ValueOf(src, "offsetY"), 0.0f);
    out.pivotX = detail::ParseFloat(detail::ValueOf(src, "pivotX"), out.width / 2.0f);
    out.pivotY = detail::ParseFloat(detail::ValueOf(src, "pivotY"), out.height / 2.0f);
    return true;
}

// ------------------------------------------------------------
// Function: LoadSpriteSheet
// Purpose: Parse custom metadata + frame array
// ------------------------------------------------------------

// ------------------------------------------------------------
// Function: LoadEnemyEncounterData
// Purpose:
//   Parse a data/enemies/*.json file into an EnemyEncounterData struct.
//   The struct is used by OverworldEnemy (overworld sprite + collision)
//   and passed directly to BattleState so enemy slots use the same
//   texture, stats, and animation as the overworld entity.
//
// JSON schema (all fields required):
//   name              - display name string
//   texturePath       - narrow ASCII path, converted to wstring internally
//   jsonPath          - sprite sheet JSON path
//   idleClip          - starting animation clip name
//   hp / atk / def / spd     - battle stats (integers)
//   contactRadius     - overworld collision radius in world pixels (float)
//   cameraFocusOffsetY - battle camera focus correction in world pixels (float)
//
// Parameters:
//   path  - path to the enemy .json file
//   out   - populated struct on success; left unchanged on failure
// Returns:
//   true  - all required fields parsed
//   false - file not found or required fields are default-zero
// ------------------------------------------------------------
inline bool LoadEnemyEncounterData(const std::string& path, EnemyEncounterData& out)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG("[JsonLoader] Cannot open enemy file: '%s'", path.c_str());
        return false;
    }
    std::ostringstream buf;
    buf << file.rdbuf();
    const std::string src = buf.str();
    out.battleParty.clear();

    // Helper: strip surrounding quotes from a ValueOf() string token.
    auto stripQ = [](const std::string& s) -> std::string {
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
            return s.substr(1, s.size() - 2);
        return s;
    };

    // Helper: convert a narrow ASCII path string to std::wstring.
    // All asset paths in this project are 7-bit ASCII - no multibyte handling needed.
    auto toWide = [](const std::string& s) -> std::wstring {
        return std::wstring(s.begin(), s.end());
    };

    // Parse the overworld identity and sprite fields (top-level).
    // These are used by OverworldEnemy for its own world-space rendering.
    out.name         = stripQ(detail::ValueOf(src, "name"));
    out.nameKey      = stripQ(detail::ValueOf(src, "nameKey"));
    out.texturePath  = toWide(stripQ(detail::ValueOf(src, "texturePath")));
    out.jsonPath     = stripQ(detail::ValueOf(src, "jsonPath"));
    out.idleClip     = stripQ(detail::ValueOf(src, "idleClip"));
    out.contactRadius= detail::ParseFloat(detail::ValueOf(src, "contactRadius"), 80.0f);
    out.environmentPath = stripQ(detail::ValueOf(src, "environmentPath"));
    out.bgmTrackId      = stripQ(detail::ValueOf(src, "bgmTrackId"));
    out.victoryBgmTrackId = stripQ(detail::ValueOf(src, "victoryBgmTrackId"));
    out.defeatBgmTrackId = stripQ(detail::ValueOf(src, "defeatBgmTrackId"));

    if (out.name.empty() || out.texturePath.empty())
    {
        LOG("[JsonLoader] Missing required top-level fields in enemy file: '%s'", path.c_str());
        return false;
    }

    // Parse the battleParty array - defines each enemy combatant in battle.
    // Each object maps to one EnemySlotData (texture, stats, camera offset).
    // A missing array is not a fatal error: BattleState falls back to a
    // hardcoded skeleton when battleParty is empty.
    const auto slotSrcs = detail::ExtractObjectsFromArray(src, "battleParty");
    for (const auto& slotSrc : slotSrcs)
    {
        EnemySlotData slot;
        slot.texturePath       = toWide(stripQ(detail::ValueOf(slotSrc, "texturePath")));
        slot.jsonPath          = stripQ(detail::ValueOf(slotSrc, "jsonPath"));
        slot.idleClip          = stripQ(detail::ValueOf(slotSrc, "idleClip"));
        slot.turnViewPath      = toWide(stripQ(detail::ValueOf(slotSrc, "turnViewPath")));
        slot.hp                = detail::ParseInt  (detail::ValueOf(slotSrc, "hp"));
        slot.atk               = detail::ParseInt  (detail::ValueOf(slotSrc, "atk"));
        slot.def               = detail::ParseInt  (detail::ValueOf(slotSrc, "def"));
        slot.spd               = detail::ParseInt  (detail::ValueOf(slotSrc, "spd"));
        slot.expReward         = detail::ParseInt  (detail::ValueOf(slotSrc, "expReward"), 0);
        slot.coinReward        = detail::ParseInt  (detail::ValueOf(slotSrc, "coinReward"), 0);
        slot.cameraFocusOffsetY= detail::ParseFloat(detail::ValueOf(slotSrc, "cameraFocusOffsetY"), -128.0f);
        
        std::string attackJson = stripQ(detail::ValueOf(slotSrc, "attackJsonPath"));
        if (!attackJson.empty()) {
            slot.attackJsonPath = attackJson;
        }

        out.battleParty.push_back(std::move(slot));
    }

    LOG("[JsonLoader] Loaded enemy '%s' from '%s': %d battle slot(s).",
        out.name.c_str(), path.c_str(), static_cast<int>(out.battleParty.size()));
    return true;
}

struct TurnViewConfig {
    float width = 256.0f;
    float height = 128.0f;
    float topScale = 1.0f;
    float normalScale = 0.7f;
    float startX = 20.0f;
    float startY = 20.0f;
    float spacing = 15.0f;
    float topSpacing = 25.0f;
    float animSpeed = 12.0f;
    float slideOffsetX = -80.0f;
    float popOffX = -150.0f;
    float popScale = 1.2f;
    float fadeSpeed = 3.0f;
    float spawnOffsetY = 150.0f;
};

inline bool LoadTurnViewConfig(const std::string& path, TurnViewConfig& out)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG("[JsonLoader] Cannot open turn view config file: '%s'", path.c_str());
        return false;
    }
    std::ostringstream buf;
    buf << file.rdbuf();
    const std::string src = buf.str();

    out.width = detail::ParseFloat(detail::ValueOf(src, "width"), 256.0f);
    out.height = detail::ParseFloat(detail::ValueOf(src, "height"), 128.0f);
    out.topScale = detail::ParseFloat(detail::ValueOf(src, "topScale"), 1.0f);
    out.normalScale = detail::ParseFloat(detail::ValueOf(src, "normalScale"), 0.7f);
    out.startX = detail::ParseFloat(detail::ValueOf(src, "startX"), 20.0f);
    out.startY = detail::ParseFloat(detail::ValueOf(src, "startY"), 20.0f);
    out.spacing = detail::ParseFloat(detail::ValueOf(src, "spacing"), 15.0f);
    out.topSpacing = detail::ParseFloat(detail::ValueOf(src, "topSpacing"), 25.0f);
    out.animSpeed = detail::ParseFloat(detail::ValueOf(src, "animSpeed"), 12.0f);
    out.slideOffsetX = detail::ParseFloat(detail::ValueOf(src, "slideOffsetX"), -80.0f);
    out.popOffX = detail::ParseFloat(detail::ValueOf(src, "popOffX"), -150.0f);
    out.popScale = detail::ParseFloat(detail::ValueOf(src, "popScale"), 1.2f);
    out.fadeSpeed = detail::ParseFloat(detail::ValueOf(src, "fadeSpeed"), 3.0f);
    out.spawnOffsetY = detail::ParseFloat(detail::ValueOf(src, "spawnOffsetY"), 150.0f);

    return true;
}

// ============================================================
// Battle Menu Layout
// ============================================================

struct BattleMenuLayout
{
    struct MenuConfig {
        float width = 180.0f;
        float height = 45.0f;
        float spacing = 10.0f;
        float textOffsetX = 30.0f;
        float textOffsetY = 12.0f;
        float sliceScale = 0.3f;
        float hoverScale = 1.05f;
        
        // Animation params
        float entryDelay = 0.0f;
        float entryDuration = 0.25f;
        float slideOffsetX = -40.0f;
        float fadeStartAlpha = 0.0f;
    };

    struct CommandMenuConfig : MenuConfig {
        float paddingLeft = 40.0f;
        float paddingBottom = 40.0f;
    };
    
    struct SkillMenuConfig : MenuConfig {
        float offsetX = 80.0f;
        float offsetY = -100.0f;
        float costOffsetX = 170.0f;
        float costOffsetY = 12.0f;
    };

    struct PartyHudConfig {
        std::string align = "bottom-right";
        float originX = -30.0f;
        float originY = -30.0f;
        float spacingX = -260.0f;
        float spacingY = 0.0f;
    };

    CommandMenuConfig command;
    SkillMenuConfig skill;
    PartyHudConfig partyHud;
};

// ------------------------------------------------------------
// Function: LoadBattleMenuLayout
// Purpose: Load UI magic numbers for the Battle State menu.
// ------------------------------------------------------------
inline bool LoadBattleMenuLayout(const std::string& path, BattleMenuLayout& out)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG("[JsonLoader] Cannot open battle menu layout file: '%s'", path.c_str());
        return false;
    }
    std::ostringstream buf;
    buf << file.rdbuf();
    const std::string src = buf.str();

    out.command.width = detail::ParseFloat(detail::ValueOf(src, "cmd_width"), 180.0f);
    out.command.height = detail::ParseFloat(detail::ValueOf(src, "cmd_height"), 45.0f);
    out.command.spacing = detail::ParseFloat(detail::ValueOf(src, "cmd_spacing"), 10.0f);
    out.command.textOffsetX = detail::ParseFloat(detail::ValueOf(src, "cmd_textOffsetX"), 30.0f);
    out.command.textOffsetY = detail::ParseFloat(detail::ValueOf(src, "cmd_textOffsetY"), 12.0f);
    out.command.sliceScale = detail::ParseFloat(detail::ValueOf(src, "cmd_sliceScale"), 0.3f);
    out.command.hoverScale = detail::ParseFloat(detail::ValueOf(src, "cmd_hoverScale"), 1.05f);
    out.command.paddingLeft = detail::ParseFloat(detail::ValueOf(src, "cmd_paddingLeft"), 40.0f);
    out.command.paddingBottom = detail::ParseFloat(detail::ValueOf(src, "cmd_paddingBottom"), 40.0f);
    out.command.entryDelay = detail::ParseFloat(detail::ValueOf(src, "cmd_entryDelay"), 0.0f);
    out.command.entryDuration = detail::ParseFloat(detail::ValueOf(src, "cmd_entryDuration"), 0.25f);
    out.command.slideOffsetX = detail::ParseFloat(detail::ValueOf(src, "cmd_slideOffsetX"), -40.0f);
    out.command.fadeStartAlpha = detail::ParseFloat(detail::ValueOf(src, "cmd_fadeStartAlpha"), 0.0f);

    out.skill.width = detail::ParseFloat(detail::ValueOf(src, "skill_width"), 240.0f);
    out.skill.height = detail::ParseFloat(detail::ValueOf(src, "skill_height"), 45.0f);
    out.skill.spacing = detail::ParseFloat(detail::ValueOf(src, "skill_spacing"), 5.0f);
    out.skill.textOffsetX = detail::ParseFloat(detail::ValueOf(src, "skill_textOffsetX"), 25.0f);
    out.skill.textOffsetY = detail::ParseFloat(detail::ValueOf(src, "skill_textOffsetY"), 12.0f);
    out.skill.sliceScale = detail::ParseFloat(detail::ValueOf(src, "skill_sliceScale"), 0.3f);
    out.skill.hoverScale = detail::ParseFloat(detail::ValueOf(src, "skill_hoverScale"), 1.05f);
    out.skill.offsetX = detail::ParseFloat(detail::ValueOf(src, "skill_offsetX"), 80.0f);
    out.skill.offsetY = detail::ParseFloat(detail::ValueOf(src, "skill_offsetY"), -100.0f);
    out.skill.costOffsetX = detail::ParseFloat(detail::ValueOf(src, "skill_costOffsetX"), 170.0f);
    out.skill.costOffsetY = detail::ParseFloat(detail::ValueOf(src, "skill_costOffsetY"), 12.0f);
    out.skill.entryDelay = detail::ParseFloat(detail::ValueOf(src, "skill_entryDelay"), 0.0f);
    out.skill.entryDuration = detail::ParseFloat(detail::ValueOf(src, "skill_entryDuration"), 0.25f);
    out.skill.slideOffsetX = detail::ParseFloat(detail::ValueOf(src, "skill_slideOffsetX"), -40.0f);
    out.skill.fadeStartAlpha = detail::ParseFloat(detail::ValueOf(src, "skill_fadeStartAlpha"), 0.0f);

    out.partyHud.align = detail::ValueOf(src, "party_hud_align");
    // ValueOf returns quoted strings if found, strip them
    if (out.partyHud.align.size() >= 2 && out.partyHud.align.front() == '"') 
        out.partyHud.align = out.partyHud.align.substr(1, out.partyHud.align.size() - 2);
    if (out.partyHud.align.empty()) out.partyHud.align = "bottom-right";

    out.partyHud.originX = detail::ParseFloat(detail::ValueOf(src, "party_hud_origin_x"), -30.0f);
    out.partyHud.originY = detail::ParseFloat(detail::ValueOf(src, "party_hud_origin_y"), -30.0f);
    out.partyHud.spacingX = detail::ParseFloat(detail::ValueOf(src, "party_hud_spacing_x"), -260.0f);
    out.partyHud.spacingY = detail::ParseFloat(detail::ValueOf(src, "party_hud_spacing_y"), 0.0f);


    LOG("[JsonLoader] Loaded BattleMenuLayout from '%s'.", path.c_str());
    return true;
}

struct BattleResultLayout
{
    float scrimAlpha = 0.72f;
    float vignetteAlpha = 0.35f;
    float victoryEnterDuration = 0.70f;
    float defeatSplashDuration = 1.80f;
    int noDamageBonusPercent = 20;
    std::string defaultVictoryBgmTrackId;
    std::string defaultDefeatBgmTrackId;
    float victoryImpactDuration = 1.10f;
    float victoryImpactCenterX = 640.0f;
    float victoryImpactCenterY = 380.0f;
    float victoryImpactFadeStartProgress = 0.22f;
    float victoryImpactFlashR = 1.0f;
    float victoryImpactFlashG = 0.92f;
    float victoryImpactFlashB = 0.86f;
    float victoryImpactFlashAlpha = 0.36f;
    float victoryImpactFlashDuration = 0.22f;
    float victoryImpactWarmTintR = 0.88f;
    float victoryImpactWarmTintG = 0.20f;
    float victoryImpactWarmTintB = 0.16f;
    float victoryImpactWarmTintAlpha = 0.18f;
    float victoryImpactRayColorR = 1.0f;
    float victoryImpactRayColorG = 0.30f;
    float victoryImpactRayColorB = 0.26f;
    float victoryImpactRayCoreAlpha = 0.80f;
    float victoryImpactRayGlowAlpha = 0.48f;
    int victoryImpactRayCount = 24;
    float victoryImpactRayLengthMin = 180.0f;
    float victoryImpactRayLengthMax = 690.0f;
    float victoryImpactRayLengthVarianceMin = 0.72f;
    float victoryImpactRayLengthVarianceMax = 1.14f;
    float victoryImpactRayThickness = 3.0f;
    float victoryImpactRayThicknessVarianceMin = 0.75f;
    float victoryImpactRayThicknessVarianceMax = 1.40f;
    float victoryImpactRayAngleJitter = 0.20f;
    float victoryImpactRayStartOffsetMin = 4.0f;
    float victoryImpactRayStartOffsetMax = 34.0f;
    float victoryImpactRayGlowThicknessScale = 3.2f;
    float victoryImpactRayCoreLengthScale = 0.78f;
    float victoryImpactRayCoreThicknessScale = 0.55f;
    float victoryImpactRayCoreMinThickness = 1.0f;
    float victoryImpactRingStartRadius = 18.0f;
    float victoryImpactRingEndRadius = 520.0f;
    float victoryImpactRingThickness = 3.0f;
    float victoryImpactRingColorR = 1.0f;
    float victoryImpactRingColorG = 0.76f;
    float victoryImpactRingColorB = 0.62f;
    float victoryImpactRingAlpha = 0.48f;
    int victoryImpactRingSegments = 56;
    float victoryImpactRingArcScale = 0.58f;

    float titleX = 96.0f;
    float titleY = 74.0f;
    float titleScale = 4.3f;
    float subtitleScale = 1.05f;

    float lootX = 110.0f;
    float lootY = 246.0f;
    float statsX = 460.0f;
    float statsY = 540.0f;
    float rowGap = 33.0f;

    float partyPanelX = 920.0f;
    float partyPanelY = 302.0f;
    float partyPanelW = 318.0f;
    float partyPanelH = 258.0f;
    float partyPanelFillR = 0.015f;
    float partyPanelFillG = 0.014f;
    float partyPanelFillB = 0.012f;
    float partyPanelFillAlpha = 0.62f;
    float partyPanelFrameAlpha = 0.62f;
    float partyRowGap = 76.0f;
    float partyPortraitXOffset = 22.0f;
    float partyPortraitYOffset = 38.0f;
    float partyPortraitSize = 52.0f;
    float partyPortraitSourceX = 64.0f;
    float partyPortraitSourceY = 0.0f;
    float partyPortraitSourceW = 128.0f;
    float partyPortraitSourceH = 128.0f;
    float partyTextXOffset = 86.0f;
    float partyTextYOffset = 34.0f;
    float partyLevelTextOffsetY = 24.0f;
    float partyExpTextOffsetY = 44.0f;
    float partyLevelUpOffsetX = 130.0f;

    float promptX = 830.0f;
    float promptY = 332.0f;
    float promptW = 360.0f;
    float promptH = 118.0f;
    float promptOptionGap = 136.0f;

    std::string defeatSigilTexturePath;
    std::string promptPanelTexturePath;
    std::string vignetteTexturePath;
    std::string victoryFlourishTexturePath;
    float defeatSigilCenterX = 640.0f;
    float defeatSigilCenterY = 304.0f;
    float defeatSigilW = 278.0f;
    float defeatSigilH = 314.0f;
    float vignetteTextureAlpha = 0.72f;
    float victoryFlourishX = 82.0f;
    float victoryFlourishY = 156.0f;
    float victoryFlourishW = 430.0f;
    float victoryFlourishH = 82.0f;

    std::string victoryAppearSfxId = "battle_result_victory_appear";
    std::string defeatAppearSfxId = "battle_result_defeat_appear";
    std::string statsOpenSfxId = "battle_result_stats_open";
    std::string closeSfxId = "battle_result_close";
};

inline bool LoadBattleResultLayout(const std::string& path, BattleResultLayout& out)
{
    namespace fs = std::filesystem;

    fs::path resolvedPath(path);
    std::ifstream file;
    file.open(resolvedPath);

    if (!file.is_open() && !resolvedPath.is_absolute()) {
        resolvedPath = fs::path("..") / path;
        file.clear();
        file.open(resolvedPath);
    }

    if (!file.is_open()) {
        LOG("[JsonLoader] Cannot open battle result layout file: '%s'", path.c_str());
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string src = buffer.str();

    detail::WarnIfUTF16(src, path);

    out.scrimAlpha = detail::ParseFloat(detail::ValueOf(src, "scrimAlpha"), out.scrimAlpha);
    out.vignetteAlpha = detail::ParseFloat(detail::ValueOf(src, "vignetteAlpha"), out.vignetteAlpha);
    out.victoryEnterDuration = detail::ParseFloat(detail::ValueOf(src, "victoryEnterDuration"), out.victoryEnterDuration);
    out.defeatSplashDuration = detail::ParseFloat(detail::ValueOf(src, "defeatSplashDuration"), out.defeatSplashDuration);
    out.noDamageBonusPercent = static_cast<int>(detail::ParseFloat(detail::ValueOf(src, "noDamageBonusPercent"), static_cast<float>(out.noDamageBonusPercent)));
    const std::string defaultVictoryBgm = detail::ValueOf(src, "defaultVictoryBgmTrackId");
    if (!defaultVictoryBgm.empty()) out.defaultVictoryBgmTrackId = detail::CleanString(defaultVictoryBgm);
    const std::string defaultDefeatBgm = detail::ValueOf(src, "defaultDefeatBgmTrackId");
    if (!defaultDefeatBgm.empty()) out.defaultDefeatBgmTrackId = detail::CleanString(defaultDefeatBgm);
    out.victoryImpactDuration = detail::ParseFloat(detail::ValueOf(src, "victoryImpactDuration"), out.victoryImpactDuration);
    out.victoryImpactCenterX = detail::ParseFloat(detail::ValueOf(src, "victoryImpactCenterX"), out.victoryImpactCenterX);
    out.victoryImpactCenterY = detail::ParseFloat(detail::ValueOf(src, "victoryImpactCenterY"), out.victoryImpactCenterY);
    out.victoryImpactFadeStartProgress = detail::ParseFloat(detail::ValueOf(src, "victoryImpactFadeStartProgress"), out.victoryImpactFadeStartProgress);
    out.victoryImpactFlashR = detail::ParseFloat(detail::ValueOf(src, "victoryImpactFlashR"), out.victoryImpactFlashR);
    out.victoryImpactFlashG = detail::ParseFloat(detail::ValueOf(src, "victoryImpactFlashG"), out.victoryImpactFlashG);
    out.victoryImpactFlashB = detail::ParseFloat(detail::ValueOf(src, "victoryImpactFlashB"), out.victoryImpactFlashB);
    out.victoryImpactFlashAlpha = detail::ParseFloat(detail::ValueOf(src, "victoryImpactFlashAlpha"), out.victoryImpactFlashAlpha);
    out.victoryImpactFlashDuration = detail::ParseFloat(detail::ValueOf(src, "victoryImpactFlashDuration"), out.victoryImpactFlashDuration);
    out.victoryImpactWarmTintR = detail::ParseFloat(detail::ValueOf(src, "victoryImpactWarmTintR"), out.victoryImpactWarmTintR);
    out.victoryImpactWarmTintG = detail::ParseFloat(detail::ValueOf(src, "victoryImpactWarmTintG"), out.victoryImpactWarmTintG);
    out.victoryImpactWarmTintB = detail::ParseFloat(detail::ValueOf(src, "victoryImpactWarmTintB"), out.victoryImpactWarmTintB);
    out.victoryImpactWarmTintAlpha = detail::ParseFloat(detail::ValueOf(src, "victoryImpactWarmTintAlpha"), out.victoryImpactWarmTintAlpha);
    out.victoryImpactRayColorR = detail::ParseFloat(detail::ValueOf(src, "victoryImpactRayColorR"), out.victoryImpactRayColorR);
    out.victoryImpactRayColorG = detail::ParseFloat(detail::ValueOf(src, "victoryImpactRayColorG"), out.victoryImpactRayColorG);
    out.victoryImpactRayColorB = detail::ParseFloat(detail::ValueOf(src, "victoryImpactRayColorB"), out.victoryImpactRayColorB);
    out.victoryImpactRayCoreAlpha = detail::ParseFloat(detail::ValueOf(src, "victoryImpactRayCoreAlpha"), out.victoryImpactRayCoreAlpha);
    out.victoryImpactRayGlowAlpha = detail::ParseFloat(detail::ValueOf(src, "victoryImpactRayGlowAlpha"), out.victoryImpactRayGlowAlpha);
    out.victoryImpactRayCount = static_cast<int>(detail::ParseFloat(detail::ValueOf(src, "victoryImpactRayCount"), static_cast<float>(out.victoryImpactRayCount)));
    out.victoryImpactRayLengthMin = detail::ParseFloat(detail::ValueOf(src, "victoryImpactRayLengthMin"), out.victoryImpactRayLengthMin);
    out.victoryImpactRayLengthMax = detail::ParseFloat(detail::ValueOf(src, "victoryImpactRayLengthMax"), out.victoryImpactRayLengthMax);
    out.victoryImpactRayLengthVarianceMin = detail::ParseFloat(detail::ValueOf(src, "victoryImpactRayLengthVarianceMin"), out.victoryImpactRayLengthVarianceMin);
    out.victoryImpactRayLengthVarianceMax = detail::ParseFloat(detail::ValueOf(src, "victoryImpactRayLengthVarianceMax"), out.victoryImpactRayLengthVarianceMax);
    out.victoryImpactRayThickness = detail::ParseFloat(detail::ValueOf(src, "victoryImpactRayThickness"), out.victoryImpactRayThickness);
    out.victoryImpactRayThicknessVarianceMin = detail::ParseFloat(detail::ValueOf(src, "victoryImpactRayThicknessVarianceMin"), out.victoryImpactRayThicknessVarianceMin);
    out.victoryImpactRayThicknessVarianceMax = detail::ParseFloat(detail::ValueOf(src, "victoryImpactRayThicknessVarianceMax"), out.victoryImpactRayThicknessVarianceMax);
    out.victoryImpactRayAngleJitter = detail::ParseFloat(detail::ValueOf(src, "victoryImpactRayAngleJitter"), out.victoryImpactRayAngleJitter);
    out.victoryImpactRayStartOffsetMin = detail::ParseFloat(detail::ValueOf(src, "victoryImpactRayStartOffsetMin"), out.victoryImpactRayStartOffsetMin);
    out.victoryImpactRayStartOffsetMax = detail::ParseFloat(detail::ValueOf(src, "victoryImpactRayStartOffsetMax"), out.victoryImpactRayStartOffsetMax);
    out.victoryImpactRayGlowThicknessScale = detail::ParseFloat(detail::ValueOf(src, "victoryImpactRayGlowThicknessScale"), out.victoryImpactRayGlowThicknessScale);
    out.victoryImpactRayCoreLengthScale = detail::ParseFloat(detail::ValueOf(src, "victoryImpactRayCoreLengthScale"), out.victoryImpactRayCoreLengthScale);
    out.victoryImpactRayCoreThicknessScale = detail::ParseFloat(detail::ValueOf(src, "victoryImpactRayCoreThicknessScale"), out.victoryImpactRayCoreThicknessScale);
    out.victoryImpactRayCoreMinThickness = detail::ParseFloat(detail::ValueOf(src, "victoryImpactRayCoreMinThickness"), out.victoryImpactRayCoreMinThickness);
    out.victoryImpactRingStartRadius = detail::ParseFloat(detail::ValueOf(src, "victoryImpactRingStartRadius"), out.victoryImpactRingStartRadius);
    out.victoryImpactRingEndRadius = detail::ParseFloat(detail::ValueOf(src, "victoryImpactRingEndRadius"), out.victoryImpactRingEndRadius);
    out.victoryImpactRingThickness = detail::ParseFloat(detail::ValueOf(src, "victoryImpactRingThickness"), out.victoryImpactRingThickness);
    out.victoryImpactRingColorR = detail::ParseFloat(detail::ValueOf(src, "victoryImpactRingColorR"), out.victoryImpactRingColorR);
    out.victoryImpactRingColorG = detail::ParseFloat(detail::ValueOf(src, "victoryImpactRingColorG"), out.victoryImpactRingColorG);
    out.victoryImpactRingColorB = detail::ParseFloat(detail::ValueOf(src, "victoryImpactRingColorB"), out.victoryImpactRingColorB);
    out.victoryImpactRingAlpha = detail::ParseFloat(detail::ValueOf(src, "victoryImpactRingAlpha"), out.victoryImpactRingAlpha);
    out.victoryImpactRingSegments = static_cast<int>(detail::ParseFloat(detail::ValueOf(src, "victoryImpactRingSegments"), static_cast<float>(out.victoryImpactRingSegments)));
    out.victoryImpactRingArcScale = detail::ParseFloat(detail::ValueOf(src, "victoryImpactRingArcScale"), out.victoryImpactRingArcScale);

    out.titleX = detail::ParseFloat(detail::ValueOf(src, "titleX"), out.titleX);
    out.titleY = detail::ParseFloat(detail::ValueOf(src, "titleY"), out.titleY);
    out.titleScale = detail::ParseFloat(detail::ValueOf(src, "titleScale"), out.titleScale);
    out.subtitleScale = detail::ParseFloat(detail::ValueOf(src, "subtitleScale"), out.subtitleScale);

    out.lootX = detail::ParseFloat(detail::ValueOf(src, "lootX"), out.lootX);
    out.lootY = detail::ParseFloat(detail::ValueOf(src, "lootY"), out.lootY);
    out.statsX = detail::ParseFloat(detail::ValueOf(src, "statsX"), out.statsX);
    out.statsY = detail::ParseFloat(detail::ValueOf(src, "statsY"), out.statsY);
    out.rowGap = detail::ParseFloat(detail::ValueOf(src, "rowGap"), out.rowGap);

    out.partyPanelX = detail::ParseFloat(detail::ValueOf(src, "partyPanelX"), out.partyPanelX);
    out.partyPanelY = detail::ParseFloat(detail::ValueOf(src, "partyPanelY"), out.partyPanelY);
    out.partyPanelW = detail::ParseFloat(detail::ValueOf(src, "partyPanelW"), out.partyPanelW);
    out.partyPanelH = detail::ParseFloat(detail::ValueOf(src, "partyPanelH"), out.partyPanelH);
    out.partyPanelFillR = detail::ParseFloat(detail::ValueOf(src, "partyPanelFillR"), out.partyPanelFillR);
    out.partyPanelFillG = detail::ParseFloat(detail::ValueOf(src, "partyPanelFillG"), out.partyPanelFillG);
    out.partyPanelFillB = detail::ParseFloat(detail::ValueOf(src, "partyPanelFillB"), out.partyPanelFillB);
    out.partyPanelFillAlpha = detail::ParseFloat(detail::ValueOf(src, "partyPanelFillAlpha"), out.partyPanelFillAlpha);
    out.partyPanelFrameAlpha = detail::ParseFloat(detail::ValueOf(src, "partyPanelFrameAlpha"), out.partyPanelFrameAlpha);
    out.partyRowGap = detail::ParseFloat(detail::ValueOf(src, "partyRowGap"), out.partyRowGap);
    out.partyPortraitXOffset = detail::ParseFloat(detail::ValueOf(src, "partyPortraitXOffset"), out.partyPortraitXOffset);
    out.partyPortraitYOffset = detail::ParseFloat(detail::ValueOf(src, "partyPortraitYOffset"), out.partyPortraitYOffset);
    out.partyPortraitSize = detail::ParseFloat(detail::ValueOf(src, "partyPortraitSize"), out.partyPortraitSize);
    out.partyPortraitSourceX = detail::ParseFloat(detail::ValueOf(src, "partyPortraitSourceX"), out.partyPortraitSourceX);
    out.partyPortraitSourceY = detail::ParseFloat(detail::ValueOf(src, "partyPortraitSourceY"), out.partyPortraitSourceY);
    out.partyPortraitSourceW = detail::ParseFloat(detail::ValueOf(src, "partyPortraitSourceW"), out.partyPortraitSourceW);
    out.partyPortraitSourceH = detail::ParseFloat(detail::ValueOf(src, "partyPortraitSourceH"), out.partyPortraitSourceH);
    out.partyTextXOffset = detail::ParseFloat(detail::ValueOf(src, "partyTextXOffset"), out.partyTextXOffset);
    out.partyTextYOffset = detail::ParseFloat(detail::ValueOf(src, "partyTextYOffset"), out.partyTextYOffset);
    out.partyLevelTextOffsetY = detail::ParseFloat(detail::ValueOf(src, "partyLevelTextOffsetY"), out.partyLevelTextOffsetY);
    out.partyExpTextOffsetY = detail::ParseFloat(detail::ValueOf(src, "partyExpTextOffsetY"), out.partyExpTextOffsetY);
    out.partyLevelUpOffsetX = detail::ParseFloat(detail::ValueOf(src, "partyLevelUpOffsetX"), out.partyLevelUpOffsetX);

    out.promptX = detail::ParseFloat(detail::ValueOf(src, "promptX"), out.promptX);
    out.promptY = detail::ParseFloat(detail::ValueOf(src, "promptY"), out.promptY);
    out.promptW = detail::ParseFloat(detail::ValueOf(src, "promptW"), out.promptW);
    out.promptH = detail::ParseFloat(detail::ValueOf(src, "promptH"), out.promptH);
    out.promptOptionGap = detail::ParseFloat(detail::ValueOf(src, "promptOptionGap"), out.promptOptionGap);

    const std::string sigilPath = detail::ValueOf(src, "defeatSigilTexturePath");
    if (!sigilPath.empty()) out.defeatSigilTexturePath = detail::CleanString(sigilPath);
    const std::string panelPath = detail::ValueOf(src, "promptPanelTexturePath");
    if (!panelPath.empty()) out.promptPanelTexturePath = detail::CleanString(panelPath);
    const std::string vignettePath = detail::ValueOf(src, "vignetteTexturePath");
    if (!vignettePath.empty()) out.vignetteTexturePath = detail::CleanString(vignettePath);
    const std::string flourishPath = detail::ValueOf(src, "victoryFlourishTexturePath");
    if (!flourishPath.empty()) out.victoryFlourishTexturePath = detail::CleanString(flourishPath);

    out.defeatSigilCenterX = detail::ParseFloat(detail::ValueOf(src, "defeatSigilCenterX"), out.defeatSigilCenterX);
    out.defeatSigilCenterY = detail::ParseFloat(detail::ValueOf(src, "defeatSigilCenterY"), out.defeatSigilCenterY);
    out.defeatSigilW = detail::ParseFloat(detail::ValueOf(src, "defeatSigilW"), out.defeatSigilW);
    out.defeatSigilH = detail::ParseFloat(detail::ValueOf(src, "defeatSigilH"), out.defeatSigilH);
    out.vignetteTextureAlpha = detail::ParseFloat(detail::ValueOf(src, "vignetteTextureAlpha"), out.vignetteTextureAlpha);
    out.victoryFlourishX = detail::ParseFloat(detail::ValueOf(src, "victoryFlourishX"), out.victoryFlourishX);
    out.victoryFlourishY = detail::ParseFloat(detail::ValueOf(src, "victoryFlourishY"), out.victoryFlourishY);
    out.victoryFlourishW = detail::ParseFloat(detail::ValueOf(src, "victoryFlourishW"), out.victoryFlourishW);
    out.victoryFlourishH = detail::ParseFloat(detail::ValueOf(src, "victoryFlourishH"), out.victoryFlourishH);

    const std::string victorySfx = detail::ValueOf(src, "victoryAppearSfxId");
    if (!victorySfx.empty()) out.victoryAppearSfxId = detail::CleanString(victorySfx);
    const std::string defeatSfx = detail::ValueOf(src, "defeatAppearSfxId");
    if (!defeatSfx.empty()) out.defeatAppearSfxId = detail::CleanString(defeatSfx);
    const std::string statsSfx = detail::ValueOf(src, "statsOpenSfxId");
    if (!statsSfx.empty()) out.statsOpenSfxId = detail::CleanString(statsSfx);
    const std::string closeSfx = detail::ValueOf(src, "closeSfxId");
    if (!closeSfx.empty()) out.closeSfxId = detail::CleanString(closeSfx);

    LOG("[JsonLoader] Loaded BattleResultLayout from '%s'.", path.c_str());
    return true;
}

struct BattleSystemConfig {
    float qteSlowMoScale = 0.1f;
    float qteFadeInRatio = 0.15f;
    float qteFadeOutDuration = 0.20f;
    float qteCameraZoom = 1.4f;
    std::string qteStartSfxId;
    std::string qteMissSfxId;
    std::string qteGoodSfxId;
    std::string qtePerfectSfxId;
    float introWalkDuration = 1.2f;
    float introWalkDistance = 600.0f;
    std::string introWalkAnim = "walk";
    std::string defaultEnvironmentPath;
};

inline bool LoadBattleSystemConfig(const std::string& path, BattleSystemConfig& out)
{
    namespace fs = std::filesystem;

    fs::path resolvedPath(path);
    std::ifstream file;
    file.open(resolvedPath);

    // Support both workspace-root cwd and bin/ cwd at runtime.
    if (!file.is_open() && !resolvedPath.is_absolute()) {
        resolvedPath = fs::path("..") / path;
        file.clear();
        file.open(resolvedPath);
    }

    if (!file.is_open()) {
        LOG("[JsonLoader] Cannot open system config file: '%s'", path.c_str());
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string src = buffer.str();

    detail::WarnIfUTF16(src, path);

    out.qteSlowMoScale = detail::ParseFloat(detail::ValueOf(src, "qteSlowMoScale"), 0.1f);
    out.qteFadeInRatio = detail::ParseFloat(detail::ValueOf(src, "qteFadeInRatio"), 0.15f);
    out.qteFadeOutDuration = detail::ParseFloat(detail::ValueOf(src, "qteFadeOutDuration"), 0.20f);
    out.qteCameraZoom = detail::ParseFloat(detail::ValueOf(src, "qteCameraZoom"), 1.4f);

    const std::string qteStartSfx = detail::ValueOf(src, "qteStartSfxId");
    if (!qteStartSfx.empty()) {
        out.qteStartSfxId = detail::CleanString(qteStartSfx);
    }

    const std::string qteMissSfx = detail::ValueOf(src, "qteMissSfxId");
    if (!qteMissSfx.empty()) {
        out.qteMissSfxId = detail::CleanString(qteMissSfx);
    }

    const std::string qteGoodSfx = detail::ValueOf(src, "qteGoodSfxId");
    if (!qteGoodSfx.empty()) {
        out.qteGoodSfxId = detail::CleanString(qteGoodSfx);
    }

    const std::string qtePerfectSfx = detail::ValueOf(src, "qtePerfectSfxId");
    if (!qtePerfectSfx.empty()) {
        out.qtePerfectSfxId = detail::CleanString(qtePerfectSfx);
    }

    out.introWalkDuration = detail::ParseFloat(detail::ValueOf(src, "introWalkDuration"), 1.2f);
    out.introWalkDistance = detail::ParseFloat(detail::ValueOf(src, "introWalkDistance"), 600.0f);

    std::string walkVal = detail::ValueOf(src, "introWalkAnim");
    if (!walkVal.empty()) {
        out.introWalkAnim = detail::ParseString(walkVal, 0);
    }

    std::string defaultEnv = detail::ValueOf(src, "defaultEnvironmentPath");
    if (!defaultEnv.empty()) {
        out.defaultEnvironmentPath = detail::CleanString(defaultEnv);
    }

    LOG("[JsonLoader] Loaded BattleSystemConfig from '%s'.", path.c_str());
    return true;
}


struct SkillData {
    std::string id;
    std::string kind = "attack";
    std::string effect;
    std::string nameKey;
    std::string descriptionKey;
    std::string iconId;
    std::string targeting = "single_enemy";
    std::string damageType = "physical";
    std::string statusEffectId;
    std::string uiSortGroup;
    std::string damageGradeKey;
    std::vector<std::string> extraRuleKeys;
    std::string effectTiming;
    int mpCost = 0;
    int amount = 0;
    int hitCount = 1;
    bool requiresFullRage = false;
    bool consumesAllRage = false;
    float skillMultiplier = 1.0f;
    float statusChance = 1.0f;
    float moveDuration = 0.5f;
    float returnDuration = 0.5f;
    float meleeOffset = 80.0f;
    float damageTakenOccurMoment = 0.8f;
    
    // QTE Configuration
    bool qteSupported = false;
    bool bulletHellSupported = false;
    std::string bulletHellPatternPath = "";
    std::vector<std::string> bulletHellPatternPaths;
    std::string bulletHellPatternSelection = "fixed";
    float qteStartMoment = 0.3f;
    float qtePerfectMultiplier = 1.5f;
    float qteGoodMultiplier = 1.2f;
    float qteMissMultiplier = 0.8f;
    float qtePerfectThreshold = 0.85f;
    float qteGoodThreshold = 0.60f;
    float bonusQteCount = 0.0f;
    int qteMinCount = 1;
    int qteMaxCount = 1;
    float qteSpacing = 0.15f;
};

inline bool LoadSkillData(const std::string& path, SkillData& out)
{
    namespace fs = std::filesystem;

    fs::path resolvedPath(path);
    std::ifstream file;
    file.open(resolvedPath);

    // Support both workspace-root cwd and bin/ cwd at runtime.
    if (!file.is_open() && !resolvedPath.is_absolute()) {
        resolvedPath = fs::path("..") / path;
        file.clear();
        file.open(resolvedPath);
    }

    if (!file.is_open()) {
        LOG("[JsonLoader] Cannot open skill data file: '%s'", path.c_str());
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string src = buffer.str();

    detail::WarnIfUTF16(src, path);

    out.id = detail::CleanString(detail::ValueOf(src, "id"));
    const std::string kind = detail::CleanString(detail::ValueOf(src, "kind"));
    out.kind = kind.empty() ? "attack" : kind;
    out.effect = detail::CleanString(detail::ValueOf(src, "effect"));
    out.nameKey = detail::CleanString(detail::ValueOf(src, "nameKey"));
    out.descriptionKey = detail::CleanString(detail::ValueOf(src, "descriptionKey"));
    out.iconId = detail::CleanString(detail::ValueOf(src, "iconId"));
    const std::string targeting = detail::CleanString(detail::ValueOf(src, "targeting"));
    out.targeting = targeting.empty() ? "single_enemy" : targeting;
    const std::string damageType = detail::CleanString(detail::ValueOf(src, "damageType"));
    out.damageType = damageType.empty() ? "physical" : damageType;
    out.statusEffectId = detail::CleanString(detail::ValueOf(src, "statusEffectId"));
    out.uiSortGroup = detail::CleanString(detail::ValueOf(src, "uiSortGroup"));
    out.damageGradeKey = detail::CleanString(detail::ValueOf(src, "damageGradeKey"));
    out.extraRuleKeys = detail::ExtractStringArray(src, "extraRuleKeys");
    out.effectTiming = detail::CleanString(detail::ValueOf(src, "effectTiming"));
    out.mpCost = detail::ParseInt(detail::ValueOf(src, "mpCost"), 0);
    out.amount = detail::ParseInt(detail::ValueOf(src, "amount"), 0);
    out.hitCount = detail::ParseInt(detail::ValueOf(src, "hitCount"), 1);
    out.requiresFullRage = detail::ParseBool(detail::ValueOf(src, "requiresFullRage"), false);
    out.consumesAllRage = detail::ParseBool(detail::ValueOf(src, "consumesAllRage"), false);
    out.skillMultiplier = detail::ParseFloat(detail::ValueOf(src, "skillMultiplier"), 1.0f);
    out.statusChance = detail::ParseFloat(detail::ValueOf(src, "statusChance"), 1.0f);
    out.moveDuration = detail::ParseFloat(detail::ValueOf(src, "moveDuration"), 0.5f);
    out.returnDuration = detail::ParseFloat(detail::ValueOf(src, "returnDuration"), 0.5f);
    out.meleeOffset = detail::ParseFloat(detail::ValueOf(src, "meleeOffset"), 80.0f);
    out.damageTakenOccurMoment = detail::ParseFloat(detail::ValueOf(src, "damageTakenOccurMoment"), 0.8f);
    if (out.damageTakenOccurMoment < 0.0f) out.damageTakenOccurMoment = 0.0f;
    if (out.damageTakenOccurMoment > 1.0f) out.damageTakenOccurMoment = 1.0f;

    out.qteSupported = detail::ParseBool(detail::ValueOf(src, "qteSupported"), false);
    out.bulletHellSupported = detail::ParseBool(detail::ValueOf(src, "bulletHellSupported"), false);
    out.bulletHellPatternPath = detail::CleanString(detail::ValueOf(src, "bulletHellPatternPath"));
    out.bulletHellPatternPaths = detail::ExtractStringArray(src, "bulletHellPatternPaths");
    if (out.bulletHellPatternPaths.empty() && !out.bulletHellPatternPath.empty()) {
        out.bulletHellPatternPaths.push_back(out.bulletHellPatternPath);
    }
    std::string patternSelection = detail::CleanString(detail::ValueOf(src, "bulletHellPatternSelection"));
    out.bulletHellPatternSelection = patternSelection.empty() ? "fixed" : patternSelection;
    out.qteStartMoment = detail::ParseFloat(detail::ValueOf(src, "qteStartMoment"), 0.3f);
    out.qtePerfectMultiplier = detail::ParseFloat(detail::ValueOf(src, "qtePerfectMultiplier"), 1.5f);
    out.qteGoodMultiplier = detail::ParseFloat(detail::ValueOf(src, "qteGoodMultiplier"), 1.2f);
    out.qteMissMultiplier = detail::ParseFloat(detail::ValueOf(src, "qteMissMultiplier"), 0.8f);
    out.qtePerfectThreshold = detail::ParseFloat(detail::ValueOf(src, "qtePerfectThreshold"), 0.85f);
    out.qteGoodThreshold = detail::ParseFloat(detail::ValueOf(src, "qteGoodThreshold"), 0.60f);
    out.bonusQteCount = detail::ParseFloat(detail::ValueOf(src, "bonusQteCount"), 0.0f);
    out.qteMinCount = detail::ParseInt(detail::ValueOf(src, "qteMinCount"), 1);
    out.qteMaxCount = detail::ParseInt(detail::ValueOf(src, "qteMaxCount"), 1);
    out.qteSpacing = detail::ParseFloat(detail::ValueOf(src, "qteSpacing"), 0.15f);

    LOG("[JsonLoader] Loaded SkillData from '%s' (resolved '%s'). mMoment=%f",
        path.c_str(),
        resolvedPath.string().c_str(),
        out.damageTakenOccurMoment);
    return true;
}

// ------------------------------------------------------------
// Function: LoadBulletHellPatternData
// Purpose:
//   Parse a data/bullet_patterns/*.json file to configure multi-spawner logic.
// ------------------------------------------------------------
struct BulletSpawnerData {
    std::string type = "random_edge"; // "random_edge", "spiral", "sine", "shield_wall"
    std::string texturePath = "";
    float spawnRate = 4.0f;
    float bulletSpeed = 150.0f;
    float bulletRadius = 6.0f;
    float bulletDamageScaling = 0.15f;

    // Sine spawner specifics.
    float sineAmplitude = 50.0f;
    float sineFrequency = 5.0f;

    // Shield-wall spawner specifics.  A wall is one timed wave made of
    // lane bullets, with a configurable safe gap for the player to read.
    int laneCount = 6;
    int gapLaneCount = 1;
    int gapStep = 1;
    float lanePadding = 16.0f;
    std::string gapMode = "track_heart";
    std::string wallDirection = "alternate";
};

struct BulletHellPatternData {
    float durationSec = 5.0f;
    float boxCenterX = 640.0f;
    float boxCenterY = 480.0f;
    float boxWidth = 550.0f;
    float boxHeight = 250.0f;
    float heartRadius = 6.0f;
    float heartSpeed = 250.0f;
    float invincibilityDuration = 1.0f;
    std::vector<BulletSpawnerData> spawners;
};

inline bool LoadBulletHellPatternData(const std::string& path, BulletHellPatternData& out)
{
    namespace fs = std::filesystem;
    fs::path resolvedPath(path);
    std::ifstream file(resolvedPath);

    if (!file.is_open() && !resolvedPath.is_absolute()) {
        resolvedPath = fs::path("..") / path;
        file.clear();
        file.open(resolvedPath);
    }
    if (!file.is_open()) {
        LOG("[JsonLoader] Cannot open pattern file: '%s'", path.c_str());
        return false;
    }

    std::ostringstream buf; buf << file.rdbuf();
    std::string src = buf.str();
    detail::WarnIfUTF16(src, path);

    out.durationSec = detail::ParseFloat(detail::ValueOf(src, "durationSec"), 5.0f);
    out.boxCenterX = detail::ParseFloat(detail::ValueOf(src, "boxCenterX"), 640.0f);
    out.boxCenterY = detail::ParseFloat(detail::ValueOf(src, "boxCenterY"), 480.0f);
    out.boxWidth = detail::ParseFloat(detail::ValueOf(src, "boxWidth"), 550.0f);
    out.boxHeight = detail::ParseFloat(detail::ValueOf(src, "boxHeight"), 250.0f);
    out.heartRadius = detail::ParseFloat(detail::ValueOf(src, "heartRadius"), 6.0f);
    out.heartSpeed = detail::ParseFloat(detail::ValueOf(src, "heartSpeed"), 250.0f);
    out.invincibilityDuration = detail::ParseFloat(detail::ValueOf(src, "invincibilityDuration"), 1.0f);

    auto objects = detail::ExtractObjectsFromArray(src, "spawners");
    for (const auto& obj : objects) {
        BulletSpawnerData sd;
        sd.type = detail::CleanString(detail::ValueOf(obj, "type"));
        if(sd.type.empty()) sd.type = "random_edge";
        
        sd.texturePath = detail::CleanString(detail::ValueOf(obj, "texturePath"));
        // If empty, will use fallback circle rendering via BattleBulletHellRenderer natively
        
        sd.spawnRate = detail::ParseFloat(detail::ValueOf(obj, "spawnRate"), 4.0f);
        sd.bulletSpeed = detail::ParseFloat(detail::ValueOf(obj, "bulletSpeed"), 150.0f);
        sd.bulletRadius = detail::ParseFloat(detail::ValueOf(obj, "bulletRadius"), 6.0f);
        sd.bulletDamageScaling = detail::ParseFloat(detail::ValueOf(obj, "bulletDamageScaling"), 0.15f);
        
        sd.sineAmplitude = detail::ParseFloat(detail::ValueOf(obj, "sineAmplitude"), 50.0f);
        sd.sineFrequency = detail::ParseFloat(detail::ValueOf(obj, "sineFrequency"), 5.0f);

        sd.laneCount = detail::ParseInt(detail::ValueOf(obj, "laneCount"), 6);
        sd.gapLaneCount = detail::ParseInt(detail::ValueOf(obj, "gapLaneCount"), 1);
        sd.gapStep = detail::ParseInt(detail::ValueOf(obj, "gapStep"), 1);
        sd.lanePadding = detail::ParseFloat(detail::ValueOf(obj, "lanePadding"), 16.0f);
        sd.gapMode = detail::CleanString(detail::ValueOf(obj, "gapMode"));
        if (sd.gapMode.empty()) sd.gapMode = "track_heart";
        sd.wallDirection = detail::CleanString(detail::ValueOf(obj, "wallDirection"));
        if (sd.wallDirection.empty()) sd.wallDirection = "alternate";
        
        out.spawners.push_back(sd);
    }

    LOG("[JsonLoader] Loaded BulletHellPatternData from '%s' with %zu spawners.", path.c_str(), out.spawners.size());
    return true;
}

// ------------------------------------------------------------
struct EnvironmentConfig {
    float width = 0.0f;
    float height = 0.0f;
    std::wstring background;
    std::wstring foreground;
    std::string ambientParticleConfig;
    float zoomLevel = 1.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
};

inline bool LoadEnvironmentConfig(const std::string& path, EnvironmentConfig& out)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG("[JsonLoader] Cannot open environment config file: '%s'", path.c_str());
        return false;
    }
    std::ostringstream buf;
    buf << file.rdbuf();
    std::string src = buf.str();

    out.width = detail::ParseFloat(detail::ValueOf(src, "width"), 1920.0f);
    out.height = detail::ParseFloat(detail::ValueOf(src, "height"), 1080.0f);

    auto stripQ = [](const std::string& s) -> std::string {
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
            return s.substr(1, s.size() - 2);
        return s;
    };

    auto toWide = [](const std::string& s) -> std::wstring {
        return std::wstring(s.begin(), s.end());
    };

    std::string bg = detail::ValueOf(src, "background");
    if (!bg.empty() && bg != "null") out.background = toWide(stripQ(bg));
    
    std::string fg = detail::ValueOf(src, "foreground");
    if (!fg.empty() && fg != "null") out.foreground = toWide(stripQ(fg));

    std::string ambientParticleConfig = detail::ValueOf(src, "ambientParticleConfig");
    if (!ambientParticleConfig.empty() && ambientParticleConfig != "null")
        out.ambientParticleConfig = stripQ(ambientParticleConfig);
    else
        out.ambientParticleConfig.clear();

    out.zoomLevel = detail::ParseFloat(detail::ValueOf(src, "zoomLevel"), 1.0f);
    out.offsetX = detail::ParseFloat(detail::ValueOf(src, "offsetX"), 0.0f);
    out.offsetY = detail::ParseFloat(detail::ValueOf(src, "offsetY"), 0.0f);

    LOG("[JsonLoader] Loaded EnvironmentConfig from '%s'.", path.c_str());
    return true;
}

// ============================================================
// Tile Map Data
// ============================================================
struct TilesetInfo {
    int firstGid = 1;
    std::wstring texturePath;
    int tileWidth = 0;
    int tileHeight = 0;
};

struct TileLayer {
    std::string name;
    int cols = 0;
    int rows = 0;
    bool visible = true;
    std::vector<int> tiles;
};

struct TileMapData {
    int tileWidth = 0;
    int tileHeight = 0;
    int cols = 0;
    int rows = 0;
    std::vector<TilesetInfo> tilesets;
    std::vector<TileLayer> layers;
    std::vector<AABBCollider> colliders;
};

inline bool LoadTileMapData(const std::string& path, TileMapData& out)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG("[JsonLoader] Cannot open tile map config file: '%s'", path.c_str());
        return false;
    }
    std::ostringstream buf;
    buf << file.rdbuf();
    std::string src = buf.str();
    detail::WarnIfUTF16(src, path);

    const std::filesystem::path mapPath(path);
    const std::filesystem::path mapDir = mapPath.parent_path();

    auto stripQ = [](const std::string& s) -> std::string {
        std::string t = detail::Trim(s);
        if (t.size() >= 2 && t.front() == '"' && t.back() == '"')
            return t.substr(1, t.size() - 2);
        return t;
    };

    auto toWide = [](const std::string& s) -> std::wstring {
        return std::wstring(s.begin(), s.end());
    };

    auto normalizeSlashes = [](std::string s) -> std::string {
        for (char& c : s) {
            if (c == '\\') c = '/';
        }
        return s;
    };

    auto readTextFile = [](const std::filesystem::path& p, std::string& outText) -> bool {
        std::ifstream in(p);
        if (!in.is_open()) return false;

        std::ostringstream buffer;
        buffer << in.rdbuf();
        outText = buffer.str();
        return true;
    };

    auto resolveRelativePath = [&](const std::filesystem::path& baseDir,
                                   const std::string& rawPath) -> std::filesystem::path {
        const std::string normalized = normalizeSlashes(rawPath);
        std::filesystem::path candidate(normalized);
        if (candidate.is_absolute()) return candidate;

        const std::filesystem::path fromBase = baseDir.empty()
            ? candidate
            : (baseDir / candidate);
        if (std::filesystem::exists(fromBase)) return fromBase;

        return fromBase;
    };

    // Tiled map dimensions
    out.cols = detail::ParseInt(detail::ValueOf(src, "width"), 0);
    out.rows = detail::ParseInt(detail::ValueOf(src, "height"), 0);
    out.tileWidth = detail::ParseInt(detail::ValueOf(src, "tilewidth"), 64);
    out.tileHeight = detail::ParseInt(detail::ValueOf(src, "tileheight"), 64);

    // Tilesets.  Tiled supports embedded tilesets and external JSON
    // tileset references.  The map entry owns firstgid; an external
    // tileset file owns the image and tile dimensions.
    std::vector<std::string> tilesets = detail::ExtractObjectsFromArray(src, "tilesets");
    for (const auto& ts : tilesets) {
        TilesetInfo info;
        info.firstGid = detail::ParseInt(detail::ValueOf(ts, "firstgid"), 1);

        std::string tilesetSrc = ts;
        std::filesystem::path tilesetBaseDir = mapDir;
        const std::string sourcePath = stripQ(detail::ValueOf(ts, "source"));
        if (!sourcePath.empty() && sourcePath != "null") {
            const std::filesystem::path resolvedSource = resolveRelativePath(mapDir, sourcePath);
            std::string externalSrc;
            if (readTextFile(resolvedSource, externalSrc)) {
                detail::WarnIfUTF16(externalSrc, resolvedSource.string());
                tilesetSrc = externalSrc;
                tilesetBaseDir = resolvedSource.parent_path();
            } else {
                LOG("[JsonLoader] WARNING: Could not read external tileset '%s' for map '%s'.",
                    resolvedSource.string().c_str(), path.c_str());
            }
        }

        info.tileWidth = detail::ParseInt(detail::ValueOf(tilesetSrc, "tilewidth"), out.tileWidth);
        info.tileHeight = detail::ParseInt(detail::ValueOf(tilesetSrc, "tileheight"), out.tileHeight);
        
        std::string tp = detail::ValueOf(tilesetSrc, "image");
        if (!tp.empty() && tp != "null") {
            tp = stripQ(tp);
            const std::filesystem::path resolvedImage = resolveRelativePath(tilesetBaseDir, tp);
            info.texturePath = toWide(normalizeSlashes(resolvedImage.generic_string()));
        }
        out.tilesets.push_back(info);
    }

    // Layers: render visible tile layers and load object groups as
    // collision data.  Tile layers keep their own dimensions so a real
    // Tiled save with per-layer width/height remains valid.
    std::vector<std::string> layers = detail::ExtractObjectsFromArray(src, "layers");
    
    // Compute bounds offset based on map size so colliders match rendered tiles
    float startX = -((out.cols * out.tileWidth) / 2.0f);
    float startY = -((out.rows * out.tileHeight) / 2.0f);
    
    for (const auto& layer : layers) {
        std::string type = stripQ(detail::ValueOf(layer, "type"));
        if (type == "tilelayer") {
            const bool visible = detail::ParseBool(detail::ValueOf(layer, "visible"), true);
            if (!visible) continue;

            TileLayer tileLayer;
            tileLayer.name = stripQ(detail::ValueOf(layer, "name"));
            tileLayer.cols = detail::ParseInt(detail::ValueOf(layer, "width"), out.cols);
            tileLayer.rows = detail::ParseInt(detail::ValueOf(layer, "height"), out.rows);
            tileLayer.visible = visible;
            size_t kpos = layer.find("\"data\"");
            if (kpos != std::string::npos) {
                size_t bracket = layer.find('[', kpos);
                if (bracket != std::string::npos) {
                    int depth = 1;
                    size_t i = bracket + 1;
                    while (i < layer.size() && depth > 0) {
                        if (layer[i] == '[') ++depth;
                        if (layer[i] == ']') --depth;
                        ++i;
                    }
                    std::string inner = layer.substr(bracket + 1, i - bracket - 2);
                    size_t start = 0;
                    while (start < inner.size()) {
                        size_t comma = inner.find(',', start);
                        if (comma == std::string::npos) {
                            std::string token = detail::Trim(inner.substr(start));
                            if (!token.empty()) tileLayer.tiles.push_back(detail::ParseInt(token));
                            break;
                        }
                        std::string token = detail::Trim(inner.substr(start, comma - start));
                        if (!token.empty()) tileLayer.tiles.push_back(detail::ParseInt(token));
                        start = comma + 1;
                    }
                }
            }
            const size_t expected = static_cast<size_t>(tileLayer.cols) * static_cast<size_t>(tileLayer.rows);
            if (tileLayer.tiles.size() < expected) {
                tileLayer.tiles.resize(expected, 0);
            }
            out.layers.push_back(tileLayer);
        } else if (type == "objectgroup") {
            std::vector<std::string> objects = detail::ExtractObjectsFromArray(layer, "objects");
            for (const auto& obj : objects) {
                float x = detail::ParseFloat(detail::ValueOf(obj, "x"), 0.0f);
                float y = detail::ParseFloat(detail::ValueOf(obj, "y"), 0.0f);
                float w = detail::ParseFloat(detail::ValueOf(obj, "width"), 0.0f);
                float h = detail::ParseFloat(detail::ValueOf(obj, "height"), 0.0f);
                
                AABBCollider aabb;
                aabb.minPoint = { startX + x, startY + y };
                aabb.maxPoint = { startX + x + w, startY + y + h };
                out.colliders.push_back(aabb);
            }
        }
    }

    LOG("[JsonLoader] Loaded TileMapData from '%s'. width: %d, height: %d, layers: %zu, objects: %zu", 
        path.c_str(), out.cols, out.rows, out.layers.size(), out.colliders.size());
    return true;
}

} // namespace JsonLoader
