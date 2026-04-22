// ============================================================
// File: CombatantAnim.h
// Responsibility: Define the standard animation roles shared by all
//                combatants (players and enemies alike).
//
// Why an enum instead of raw strings?
//   Raw strings scatter "die", "idle", "attack-1" literals across every
//   system that drives animations.  A typo in one call site silently produces
//   the wrong behaviour (WorldSpriteRenderer logs a warning, then no-ops).
//   The enum makes the vocabulary explicit, type-safe, and centrally
//   documented.  Renaming a role requires one edit here, not a grep.
//
// Fallback contract:
//   If a character's sprite sheet does not contain the clip named by
//   DefaultClipName(role), WorldSpriteRenderer::PlayClip logs a warning
//   and no-ops silently.  The caller never crashes; the character simply
//   freezes on its current frame for that missing animation.
//
// Adding a new role:
//   1. Add the enum value before kCount.
//   2. Add a case to DefaultClipName().
//   3. Optionally add an override field to EnemySlotData if characters
//      commonly use non-default names for the new role.
// ============================================================
#pragma once
#include <string>

// ------------------------------------------------------------
// CombatantAnim: standard animation roles every combatant should support.
//
// Not every character is required to have all roles.  Missing clips are
// handled gracefully: WorldSpriteRenderer warns and freezes on the last frame.
//
// kCount is a sentinel used for array sizing — never request it as a clip.
// ------------------------------------------------------------
enum class CombatantAnim
{
    Idle,    // Standing still — plays when no other action is in progress.
    Attack,  // Attack windup + strike — played by AttackAction.
    Walk,    // Movement cycle — played when navigating the overworld.
    Die,     // Death collapse — triggered the frame HP reaches 0.
    Hurt,    // Hit-reaction — optionally triggered when taking damage.
    Ready,   // Pre-combat transition from Idle to FightState.
    FightState, // Active combat stance while selecting skills or acting.
    BattleMove, // Character advancing towards enemy target.
    BattleUnmove, // Character returning to starting formation.
    Unready, // Post-combat transition from FightState back to Idle.
    kCount
};

static constexpr int kCombatantAnimCount = static_cast<int>(CombatantAnim::kCount);

// ------------------------------------------------------------
// DefaultClipName: returns the conventional sprite-sheet clip name
// for a given role.
//
// These names must match the "name" field in the character's *.json
// sprite-sheet descriptor.  If a character uses a different name
// (e.g. "death" instead of "die"), override it via
// EnemySlotData::dieClip (or the equivalent player data field).
// ------------------------------------------------------------
inline const char* DefaultClipName(CombatantAnim anim)
{
    switch (anim)
    {
        case CombatantAnim::Idle:   return "idle";
        case CombatantAnim::Attack: return "attack-1";
        case CombatantAnim::Walk:   return "walk";
        case CombatantAnim::Die:    return "die";
        case CombatantAnim::Hurt:   return "hurt";
        case CombatantAnim::Ready:       return "ready";
        case CombatantAnim::FightState:  return "fight-state";
        case CombatantAnim::BattleMove:  return "battle-move";
        case CombatantAnim::BattleUnmove:return "battle-unmove";
        case CombatantAnim::Unready:     return "unready";
        default:                    return "idle";
    }
}
