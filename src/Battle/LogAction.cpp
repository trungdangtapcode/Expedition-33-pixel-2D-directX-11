// ============================================================
// File: LogAction.cpp
// ============================================================
#include "LogAction.h"

LogAction::LogAction(std::vector<std::string>* log, std::string message)
    : mLog(log)
    , mMessage(std::move(message))
{}

bool LogAction::Execute(float /*dt*/)
{
    if (mLog)
    {
        mLog->push_back(mMessage);

        // Cap the log so it doesn't grow unbounded across a long battle.
        // BattleState only renders the last N lines anyway; keep tail only.
        constexpr std::size_t kMaxLogLines = 64;
        if (mLog->size() > kMaxLogLines)
            mLog->erase(mLog->begin());
    }
    return true;
}
