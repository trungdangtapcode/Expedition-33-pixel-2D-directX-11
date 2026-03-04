// ============================================================
// File: ActionQueue.h
// Responsibility: Sequential FIFO queue of IAction objects.
//
// Only the front action executes each frame.  When it returns true
// (complete), it is dequeued and the next begins on the same or next frame.
//
// This is the single site where combat state mutations happen —
// all IAction::Execute calls feed through here.
//
// Owned by: BattleManager (value member, not heap-allocated)
// ============================================================
#pragma once
#include "IAction.h"
#include <deque>
#include <memory>

class ActionQueue
{
public:
    // Append an action to the back of the queue.
    void Enqueue(std::unique_ptr<IAction> action);

    // Process the front action for one frame.  Call every BattleManager::Update.
    void Update(float dt);

    // True when no actions are pending — BattleManager advances the turn.
    bool IsEmpty() const;

    // Discard all pending actions (e.g., on battle end or state change).
    void Clear();

private:
    std::deque<std::unique_ptr<IAction>> mQueue;
};
