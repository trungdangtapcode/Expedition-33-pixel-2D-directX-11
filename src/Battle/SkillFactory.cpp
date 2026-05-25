// ============================================================
// File: SkillFactory.cpp
// Responsibility: Keep skill construction centralized and data-driven.
// ============================================================
#include "SkillFactory.h"
#include "AttackSkill.h"
#include "DataDrivenSkill.h"
#include "../Utils/JsonLoader.h"
#include "../Utils/Log.h"

std::unique_ptr<ISkill> SkillFactory::CreateFromFile(const std::string& path)
{
    JsonLoader::SkillData data;
    if (!JsonLoader::LoadSkillData(path, data))
    {
        LOG("[SkillFactory] WARNING: failed to load '%s'.", path.c_str());
        return nullptr;
    }

    if (data.id.empty())
    {
        data.id = path;
    }

    if (data.kind == "attack")
    {
        return std::make_unique<AttackSkill>(data);
    }

    return std::make_unique<DataDrivenSkill>(data);
}
