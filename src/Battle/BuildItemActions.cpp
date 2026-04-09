// ============================================================
// File: BuildItemActions.cpp
// Responsibility: Resolve item targeting + emit the action sequence.
// ============================================================
#include "BuildItemActions.h"
#include "ItemData.h"
#include "ItemEffectAction.h"
#include "ItemConsumeAction.h"
#include "LogAction.h"
#include "IBattler.h"
#include "BattleManager.h"
#include "../Utils/Log.h"

namespace
{
    // ------------------------------------------------------------
    // ResolveTargets: turn (targeting rule, primary pick) into the
    // exact list of battlers the effect should apply to.
    //
    // Returns an empty vector when targeting fails — the caller treats
    // that as "skill cannot execute" and refunds the player's turn.
    // ------------------------------------------------------------
    std::vector<IBattler*> ResolveTargets(IBattler& user,
                                            const ItemData& item,
                                            IBattler* primary,
                                            BattleManager& battle)
    {
        std::vector<IBattler*> out;

        switch (item.targeting)
        {
        case ItemTargeting::SelfOnly:
            out.push_back(&user);
            break;

        case ItemTargeting::SingleAlly:
            if (primary && primary->IsAlive()) out.push_back(primary);
            break;

        case ItemTargeting::SingleAllyAny:
            // Revive items target dead allies — IsAlive check is intentionally
            // skipped, but a null primary is still rejected.
            if (primary) out.push_back(primary);
            break;

        case ItemTargeting::SingleEnemy:
            if (primary && primary->IsAlive()) out.push_back(primary);
            break;

        case ItemTargeting::AllAllies:
            for (IBattler* b : battle.GetAlivePlayers()) out.push_back(b);
            break;

        case ItemTargeting::AllEnemies:
            for (IBattler* b : battle.GetAliveEnemies()) out.push_back(b);
            break;
        }

        return out;
    }
}

// ------------------------------------------------------------
// Build
//   Returns one LogAction + N ItemEffectAction + one ItemConsumeAction.
//   Empty result on targeting failure.
// ------------------------------------------------------------
std::vector<std::unique_ptr<IAction>> BuildItemActions::Build(
    IBattler& user,
    const ItemData& item,
    IBattler* primaryTarget,
    BattleManager& battle)
{
    std::vector<std::unique_ptr<IAction>> actions;

    auto targets = ResolveTargets(user, item, primaryTarget, battle);
    if (targets.empty())
    {
        LOG("[BuildItemActions] '%s' has no valid targets — aborting.",
            item.id.c_str());
        return actions;
    }

    // 1. Battle log line.  BattleManager replaces the null log pointer
    //    with its live address inside EnqueueSkillActions, the same way
    //    skill-built LogActions are upgraded.
    actions.push_back(std::make_unique<LogAction>(
        nullptr,
        user.GetName() + " uses " + item.name + "!"));

    // 2. One ItemEffectAction per resolved target.  ItemData is value-copied
    //    into each action so the action is self-contained — safe even if
    //    the registry is reloaded mid-battle (it never is, today).
    for (IBattler* t : targets)
    {
        actions.push_back(std::make_unique<ItemEffectAction>(t, item));
    }

    // 3. Single inventory decrement, regardless of how many targets there were.
    actions.push_back(std::make_unique<ItemConsumeAction>(item.id));

    return actions;
}
