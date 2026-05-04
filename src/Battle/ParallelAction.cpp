// ============================================================
// File: ParallelAction.cpp
// ============================================================
#include "ParallelAction.h"

ParallelAction::ParallelAction() = default;

void ParallelAction::AddAction(std::unique_ptr<IAction> action)
{
    if (action) {
        mActions.push_back(std::move(action));
    }
}

bool ParallelAction::Execute(float dt)
{
    bool allComplete = true;
    for (auto it = mActions.begin(); it != mActions.end(); )
    {
        if ((*it)->Execute(dt)) {
            it = mActions.erase(it);
        } else {
            allComplete = false;
            ++it;
        }
    }
    return mActions.empty();
}
