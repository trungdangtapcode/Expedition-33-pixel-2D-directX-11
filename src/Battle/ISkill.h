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

// Forward declarations — avoid header pulling
class IBattler;
class IAction;
struct BattleContext;

class ISkill
{
public:
    virtual ~ISkill() = default;

    virtual const char* GetName()        const = 0;
    virtual const char* GetDescription() const = 0;

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
