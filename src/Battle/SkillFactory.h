// ============================================================
// File: SkillFactory.h
// Responsibility: Convert a skill JSON path into an ISkill instance.
// ============================================================
#pragma once
#include "ISkill.h"
#include <memory>
#include <string>

class SkillFactory
{
public:
    static std::unique_ptr<ISkill> CreateFromFile(const std::string& path);
};
