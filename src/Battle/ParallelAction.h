// ============================================================
// File: ParallelAction.h
// Responsibility: Executes multiple inner actions concurrently.
// Returns true only when ALL inner actions return true.
// ============================================================
#pragma once
#include "IAction.h"
#include <vector>
#include <memory>

class ParallelAction : public IAction
{
public:
    ParallelAction();
    ~ParallelAction() override = default;

    void AddAction(std::unique_ptr<IAction> action);
    bool Execute(float dt) override;

private:
    std::vector<std::unique_ptr<IAction>> mActions;
};
