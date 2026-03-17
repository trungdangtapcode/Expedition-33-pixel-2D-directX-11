// ============================================================
// File: Log.h
// Responsibility: Lightweight logging macro for debug builds.
//
// Routes log output to TWO destinations simultaneously:
//   1. OutputDebugStringA  — visible in DebugView / DebugView++
//   2. printf              — visible in the console window attached
//                            by GameApp::Initialize via AllocConsole()
//
// Output format:
//   [HH:MM:SS.mmm] <message>
//   Timestamp is wall-clock time via GetLocalTime() — millisecond precision.
//   Example: [14:03:57.412] [PlayState] OnEnter
//
// Usage:
//   LOG("[StateManager] PushState: %s", state->GetName());
//
// Lifetime: Header-only. No .cpp, no link dependency, no shutdown needed.
//
// Important:
//   - Only active in DEBUG builds (_DEBUG defined by /MTd or /MDd, or
//     explicitly via /D_DEBUG). In release builds LOG() compiles to nothing.
//   - The format string and all arguments are evaluated at most once,
//     so there is no double-evaluation risk.
// ============================================================
#pragma once
#include <windows.h>  // OutputDebugStringA, GetLocalTime, SYSTEMTIME
#include <cstdio>     // snprintf, printf

#ifdef _DEBUG

// ------------------------------------------------------------
// Macro: LOG
// Purpose:
//   Prepend a wall-clock timestamp ([HH:MM:SS.mmm]) to every message,
//   then send it to both the console window and OutputDebugStringA.
// Why timestamp:
//   High-frequency log lines (e.g. per-frame Draw() calls) are impossible
//   to sequence without a time reference.  Millisecond precision is enough
//   to correlate log output with frame numbers and spot frame-rate stalls.
// Why GetLocalTime() instead of QueryPerformanceCounter():
//   GetLocalTime() produces human-readable HH:MM:SS — easy to cross-reference
//   with external tools (DebugView++, task manager, video capture).
//   QPC is more precise but requires a reference epoch to be readable.
// Why two destinations:
//   - OutputDebugStringA: readable by DebugView++ when running without console.
//   - printf:             readable in the AllocConsole() window attached by
//                         GameApp — no external tool required during iteration.
// Parameters:
//   fmt  — printf-style format string literal.
//   ...  — variadic arguments matching the format.
// Caveats:
//   - Total buffer is 1024 chars (timestamp prefix + message).  Long messages
//     are silently truncated.  Increase LOG_BUFFER_SIZE if needed.
//   - Not thread-safe; do not call from multiple threads simultaneously.
// 
// New feature: write into log files 
// ------------------------------------------------------------
#define LOG_BUFFER_SIZE 1024

#define LOG(fmt, ...)                                                              \
    do {                                                                           \
        /* Capture wall-clock time for the timestamp prefix. */                   \
        SYSTEMTIME _log_st;                                                        \
        GetLocalTime(&_log_st);                                                    \
                                                                                   \
        /* Build the final string: "[HH:MM:SS.mmm] <message>\n" */                \
        char _log_buf[LOG_BUFFER_SIZE];                                            \
        int _log_prefix = snprintf(_log_buf, LOG_BUFFER_SIZE,                     \
            "[%02d:%02d:%02d.%03d] ",                                             \
            _log_st.wHour, _log_st.wMinute, _log_st.wSecond,                     \
            _log_st.wMilliseconds);                                                \
        if (_log_prefix > 0 && _log_prefix < LOG_BUFFER_SIZE) {                  \
            snprintf(_log_buf + _log_prefix,                                      \
                     LOG_BUFFER_SIZE - _log_prefix,                               \
                     fmt "\n", ##__VA_ARGS__);                                    \
        }                                                                          \
                                                                                   \
        /* Send to OutputDebugStringA so DebugView / DebugView++ picks it up. */ \
        OutputDebugStringA(_log_buf);                                              \
                                                                                   \
        /* Also print to stdout (AllocConsole window). */                         \
        printf("%s", _log_buf);                                                    \
        FILE* _log_file = fopen("log_output.txt", "a");                        \
        if (_log_file) {                                                       \
            fputs(_log_buf, _log_file);                                        \
            fclose(_log_file);                                                 \
        }                                                                      \
    } while (0)

#else
    // In release builds, LOG() expands to nothing.
    // The compiler eliminates all format strings and arguments entirely,
    // so there is zero runtime cost and no strings are embedded in the binary.
    #define LOG(fmt, ...) do {} while (0)
#endif // _DEBUG
