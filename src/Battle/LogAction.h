// ============================================================
// File: LogAction.h
// Responsibility: Atomic action that writes one line to the battle logs.
//
// BattleManager owns two log streams:
//   - localized gameplay text for the on-screen battle log
//   - English debug text for CLI and OutputDebugStringA
//
// LogAction only observes those vectors. It never owns the log storage.
// The action completes instantly.
// ============================================================
#pragma once
#include "IAction.h"
#include <string>
#include <vector>

class LogAction : public IAction
{
public:
    // Both log pointers are non-owning and may be null while skills build
    // their action lists. BattleManager injects the live pointers before
    // enqueueing actions.
    LogAction(std::vector<std::string>* log,
              std::string message,
              std::vector<std::string>* debugLog = nullptr,
              std::string debugMessage = std::string());

    bool Execute(float dt) override;

    // Named GetText() to avoid collision with Win32 GetMessage macros.
    const std::string& GetText() const { return mMessage; }
    const std::string& GetDebugText() const;

private:
    std::vector<std::string>* mLog;       // non-owning localized log
    std::vector<std::string>* mDebugLog;  // non-owning English debug log
    std::string               mMessage;
    std::string               mDebugMessage;
};
