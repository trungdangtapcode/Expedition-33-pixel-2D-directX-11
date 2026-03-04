// ============================================================
// File: LogAction.h
// Responsibility: Atomic action — write one line to the battle log.
//
// BattleManager maintains a vector<string> battle log that BattleState
// reads each frame to render the scrolling combat text.
// LogAction holds a pointer to that vector and appends on Execute().
// Completes instantly.
// ============================================================
#pragma once
#include "IAction.h"
#include <string>
#include <vector>

class LogAction : public IAction
{
public:
    // log — pointer to BattleManager's mBattleLog (non-owning).
    LogAction(std::vector<std::string>* log, std::string message);

    bool Execute(float dt) override;

    // Accessor used by BattleManager to re-create a LogAction with the
    // correct log pointer (skills produce LogActions with nullptr log).
    // Named GetText() to avoid collision with Win32 GetMessage/GetMessageW macro.
    const std::string& GetText() const { return mMessage; }

private:
    std::vector<std::string>* mLog;     // non-owning observer
    std::string               mMessage;
};
