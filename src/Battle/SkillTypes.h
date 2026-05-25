// ============================================================
// File: SkillTypes.h
// Responsibility: Shared enums for data-driven battle skill behavior.
// ============================================================
#pragma once

enum class SkillTargeting
{
    SingleEnemy,
    SingleAlly,
    AllEnemies,
    AllAllies,
    Self
};

enum class SkillResourceKind
{
    None,
    MP,
    Rage
};
