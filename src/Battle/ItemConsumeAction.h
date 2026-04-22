// ============================================================
// File: ItemConsumeAction.h
// Responsibility: Decrement the player's inventory by exactly one
//                 item id, as the LAST step of an item-use sequence.
//
// Why a separate action instead of consuming inside ItemEffectAction:
//   - AoE items expand into N ItemEffectActions (one per target).  We
//     must NOT remove N copies from the inventory — only one.  Putting
//     the consume in its own action keeps the count exactly right.
//   - It also lets us cancel/replace effects mid-queue (future) without
//     refunding inventory state.
//
// Order in the queue (built by BuildItemActions):
//   [LogAction]            "Verso uses Potion!"
//   [ItemEffectAction…]    one per target
//   [ItemConsumeAction]    -1 from Inventory["potion_small"]
// ============================================================
#pragma once
#include "IAction.h"
#include <string>

class ItemConsumeAction : public IAction
{
public:
    explicit ItemConsumeAction(std::string itemId);

    // Calls Inventory::Get().Remove(mItemId, 1).  Completes in one frame.
    bool Execute(float dt) override;

private:
    std::string mItemId;
};
