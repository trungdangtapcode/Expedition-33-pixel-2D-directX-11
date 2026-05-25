// ============================================================
// File: ISkill.h
// Responsibility: Pure virtual interface for all battle skills.
//
// Implemented by:
//   AttackSkill   — basic attack with rage generation
//   RageSkill     — consume full rage for heavy damage
//   WeakenSkill   — apply ATK/DEF debuff to one target
//
// Ownership:
//   Skills are owned by PlayerCombatant (unique_ptr).
//   EnemyCombatant shares a static/global skill instance (no ownership).
//
// Execute contract:
//   Returns a list of IAction objects to be pushed into ActionQueue.
//   The skill itself performs NO stat changes — all mutations go through
//   IAction::Execute so the action queue remains the single site of
//   combat state change (deterministic, replayable).
// ============================================================
#pragma once
#include <string>
#include <vector>
#include <memory>
#include "SkillTypes.h"

// Forward declarations — avoid header pulling
class IBattler;
class IAction;
struct BattleContext;

class ISkill
{
public:
    virtual ~ISkill() = default;

    virtual std::string GetName()        const = 0;
    virtual std::string GetDescription() const = 0;
    virtual std::string GetId() const { return GetDebugName(); }
    virtual std::string GetIconId() const { return std::string(); }
    virtual int GetMpCost() const { return 0; }
    virtual SkillResourceKind GetResourceKind() const { return SkillResourceKind::None; }
    virtual SkillTargeting GetTargeting() const { return SkillTargeting::SingleEnemy; }

    // Debug text is intentionally English-only because BattleDebugHUD and
    // LOG() target CLI tools that may not render active-language UTF-8 text.
    virtual std::string GetDebugName() const { return GetName(); }
    virtual std::string GetDebugDescription() const { return GetDescription(); }

    // ------------------------------------------------------------
    // CanUse: return false to grey out the skill in UI.
    //   Examples: RageSkill returns false if caster rage < maxRage.
    //             WeakenSkill returns false if MP < cost.
    //   ctx is provided so future availability rules can depend on
    //   broader state ("only usable while any ally is below 50% HP").
    // ------------------------------------------------------------
    virtual bool CanUse(const IBattler& caster,
                         const BattleContext& ctx) const = 0;

    // ------------------------------------------------------------
    // Execute: build and return the action sequence for this skill.
    //   caster  — the skill user (non-owning)
    //   targets — relevant targets (typically 1; AoE skills take all)
    //   ctx     — live battle context; skills may pass a pointer to it
    //             into any DamageAction / AnimDamageAction they construct
    //             so the calculator sees current state at execution time.
    //   Returned actions are enqueued by BattleManager in order.
    // ------------------------------------------------------------
    virtual std::vector<std::unique_ptr<IAction>> Execute(
        IBattler& caster,
        std::vector<IBattler*>& targets,
        const BattleContext& ctx) const = 0;
};
