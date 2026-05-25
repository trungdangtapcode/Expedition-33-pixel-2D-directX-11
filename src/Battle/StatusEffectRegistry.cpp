// ============================================================
// File: StatusEffectRegistry.cpp
// Responsibility: Parse status effect data files into StatusEffectData.
// ============================================================
#define NOMINMAX
#include "StatusEffectRegistry.h"
#include "../Utils/JsonLoader.h"
#include "../Utils/Log.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace
{
    std::string StripQuotes(const std::string& value)
    {
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
            return value.substr(1, value.size() - 2);
        return value;
    }

    StatId ParseStatId(const std::string& raw)
    {
        const std::string value = StripQuotes(raw);
        if (value == "atk") return StatId::ATK;
        if (value == "def") return StatId::DEF;
        if (value == "matk") return StatId::MATK;
        if (value == "mdef") return StatId::MDEF;
        if (value == "spd") return StatId::SPD;
        if (value == "max_hp") return StatId::MAX_HP;
        if (value == "max_mp") return StatId::MAX_MP;
        return StatId::ATK;
    }

    StatModifier::Op ParseOp(const std::string& raw)
    {
        const std::string value = StripQuotes(raw);
        if (value == "add_percent") return StatModifier::Op::AddPercent;
        if (value == "multiply") return StatModifier::Op::Multiply;
        return StatModifier::Op::AddFlat;
    }

    StatusEffectCategory ParseCategory(const std::string& raw)
    {
        const std::string value = StripQuotes(raw);
        if (value == "buff") return StatusEffectCategory::Buff;
        if (value == "debuff") return StatusEffectCategory::Debuff;
        return StatusEffectCategory::Neutral;
    }

    StatusStackPolicy ParseStackPolicy(const std::string& raw)
    {
        const std::string value = StripQuotes(raw);
        if (value == "stack_intensity") return StatusStackPolicy::StackIntensity;
        if (value == "extend_duration") return StatusStackPolicy::ExtendDuration;
        return StatusStackPolicy::Refresh;
    }
}

StatusEffectRegistry& StatusEffectRegistry::Get()
{
    static StatusEffectRegistry instance;
    return instance;
}

void StatusEffectRegistry::EnsureLoaded()
{
    if (mLoaded) return;
    mLoaded = true;

    fs::path dir = "data/status_effects";
    if (!fs::exists(dir))
    {
        fs::path alt = fs::path("..") / dir;
        if (fs::exists(alt)) dir = alt;
    }

    if (!fs::exists(dir) || !fs::is_directory(dir))
    {
        LOG("[StatusEffectRegistry] WARNING: data/status_effects directory missing.");
        return;
    }

    int loaded = 0;
    int failed = 0;
    for (const auto& entry : fs::directory_iterator(dir))
    {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
        if (LoadFile(entry.path().string())) ++loaded;
        else ++failed;
    }

    LOG("[StatusEffectRegistry] Loaded %d status effects (%d failed).", loaded, failed);
}

const StatusEffectData* StatusEffectRegistry::Find(const std::string& id) const
{
    for (const StatusEffectData& data : mEffects)
    {
        if (data.id == id) return &data;
    }
    return nullptr;
}

bool StatusEffectRegistry::LoadFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        LOG("[StatusEffectRegistry] Cannot open '%s'.", path.c_str());
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string src = buffer.str();
    JsonLoader::detail::WarnIfUTF16(src, path);

    StatusEffectData data;
    data.id = StripQuotes(JsonLoader::detail::ValueOf(src, "id"));
    if (data.id.empty())
    {
        LOG("[StatusEffectRegistry] '%s' has no id.", path.c_str());
        return false;
    }

    data.nameKey = StripQuotes(JsonLoader::detail::ValueOf(src, "nameKey"));
    data.descriptionKey = StripQuotes(JsonLoader::detail::ValueOf(src, "descriptionKey"));
    data.iconId = StripQuotes(JsonLoader::detail::ValueOf(src, "iconId"));
    data.category = ParseCategory(JsonLoader::detail::ValueOf(src, "category"));
    data.stackPolicy = ParseStackPolicy(JsonLoader::detail::ValueOf(src, "stackPolicy"));
    data.durationTurns = JsonLoader::detail::ParseInt(JsonLoader::detail::ValueOf(src, "durationTurns"), 1);
    data.maxStacks = JsonLoader::detail::ParseInt(JsonLoader::detail::ValueOf(src, "maxStacks"), 1);
    data.dispellable = JsonLoader::detail::ParseBool(JsonLoader::detail::ValueOf(src, "dispellable"), true);
    data.tickDamage = JsonLoader::detail::ParseInt(JsonLoader::detail::ValueOf(src, "tickDamage"), 0);
    data.tickDamagePerStack = JsonLoader::detail::ParseInt(JsonLoader::detail::ValueOf(src, "tickDamagePerStack"), 0);

    if (data.durationTurns < 1) data.durationTurns = 1;
    if (data.maxStacks < 1) data.maxStacks = 1;

    const auto modifierObjects = JsonLoader::detail::ExtractObjectsFromArray(src, "modifiers");
    for (const std::string& obj : modifierObjects)
    {
        StatusModifierData mod;
        mod.stat = ParseStatId(JsonLoader::detail::ValueOf(obj, "stat"));
        mod.op = ParseOp(JsonLoader::detail::ValueOf(obj, "op"));
        mod.value = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(obj, "value"), 0.0f);
        data.modifiers.push_back(mod);
    }

    mEffects.push_back(std::move(data));
    return true;
}
