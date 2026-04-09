// ============================================================
// File: BuildItemActions.h
// Responsibility: Free function that takes one ItemData + one chosen
//                 primary target and returns the IAction sequence the
//                 ActionQueue should execute.
//
// Why a free function instead of an IItem::Execute method:
//   Items are pure data — there is no per-item polymorphism.  A free
//   function keeps the dispatch logic in ONE place and avoids creating
//   N tiny IItem subclasses that would all delegate to the same body.
//   This mirrors how WeakenSkill / AttackSkill build their action lists,
//   except the input is data, not a class.
//
// Target expansion rules:
//   SelfOnly        — ignore primaryTarget; use the user
//   SingleAlly      — primaryTarget if alive; otherwise empty
//   SingleAllyAny   — primaryTarget regardless of alive (revive support)
//   SingleEnemy     — primaryTarget if alive; otherwise empty
//   AllAllies       — every alive player from BattleManager
//   AllEnemies      — every alive enemy from BattleManager
//
// Sequence the function builds:
//   [LogAction]            "<user> uses <item>!"
//   [ItemEffectAction…]    one per resolved target
//   [ItemConsumeAction]    one decrement, regardless of target count
// ============================================================
#pragma once
#include <vector>
#include <memory>

class IAction;
class IBattler;
class BattleManager;
struct ItemData;

namespace BuildItemActions
{
    // ------------------------------------------------------------
    // Build
    // Parameters:
    //   user           — the combatant invoking the item (owns the use)
    //   item           — fully populated ItemData (copy or registry pointer-deref)
    //   primaryTarget  — what the player picked in TARGET_SELECT (may be nullptr
    //                    for self/AoE items where targeting is implicit)
    //   battle         — needed to enumerate AllAllies / AllEnemies for AoE
    // Returns:
    //   A non-empty vector on success.
    //   An empty vector if targeting cannot be resolved (e.g. SingleEnemy
    //   with a dead primaryTarget) — the caller should log and refund.
    // ------------------------------------------------------------
    std::vector<std::unique_ptr<IAction>> Build(
        IBattler& user,
        const ItemData& item,
        IBattler* primaryTarget,
        BattleManager& battle);
}
