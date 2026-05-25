// ============================================================
// File: LogAction.cpp
// Responsibility: Append localized and English debug battle log lines.
// ============================================================
#include "LogAction.h"
#include <utility>

namespace
{
    constexpr std::size_t kMaxLogLines = 64;

    void PushCapped(std::vector<std::string>* log, const std::string& line)
    {
        if (!log) return;

        log->push_back(line);
        if (log->size() > kMaxLogLines)
        {
            log->erase(log->begin());
        }
    }
}

LogAction::LogAction(std::vector<std::string>* log,
                     std::string message,
                     std::vector<std::string>* debugLog,
                     std::string debugMessage)
    : mLog(log)
    , mDebugLog(debugLog)
    , mMessage(std::move(message))
    , mDebugMessage(std::move(debugMessage))
{}

const std::string& LogAction::GetDebugText() const
{
    return mDebugMessage.empty() ? mMessage : mDebugMessage;
}

bool LogAction::Execute(float /*dt*/)
{
    PushCapped(mLog, mMessage);
    PushCapped(mDebugLog, GetDebugText());
    return true;
}
