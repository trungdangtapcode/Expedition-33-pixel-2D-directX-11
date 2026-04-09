// ============================================================
// File: ItemConsumeAction.cpp
// ============================================================
#include "ItemConsumeAction.h"
#include "../Systems/Inventory.h"
#include "../Utils/Log.h"

ItemConsumeAction::ItemConsumeAction(std::string itemId)
    : mItemId(std::move(itemId))
{}

bool ItemConsumeAction::Execute(float /*dt*/)
{
    const int removed = Inventory::Get().Remove(mItemId, 1);
    if (removed == 0)
    {
        // Inventory was empty for this id by the time the action ran.
        // This is unusual but not fatal — log and move on so the queue
        // can continue draining.
        LOG("[ItemConsumeAction] WARNING: '%s' had count 0 at consume time.",
            mItemId.c_str());
    }
    else
    {
        LOG("[ItemConsumeAction] Consumed 1x '%s' (remaining: %d).",
            mItemId.c_str(), Inventory::Get().GetCount(mItemId));
    }
    return true;
}
