// ============================================================
// File: ActionQueue.cpp
// ============================================================
#include "ActionQueue.h"

void ActionQueue::Enqueue(std::unique_ptr<IAction> action)
{
    mQueue.push_back(std::move(action));
}

// ------------------------------------------------------------
// Update: execute the front action once.
//   If it returns true (done), pop it so the next action begins
//   on the following frame (not the same frame — safer for complex chains).
// ------------------------------------------------------------
void ActionQueue::Update(float dt)
{
    if (mQueue.empty()) return;

    const bool done = mQueue.front()->Execute(dt);
    if (done)
    {
        mQueue.pop_front();
    }
}

bool ActionQueue::IsEmpty() const
{
    return mQueue.empty();
}

void ActionQueue::Clear()
{
    mQueue.clear();
}
