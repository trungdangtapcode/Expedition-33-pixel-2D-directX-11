// ============================================================
// File: Log.h
// Responsibility: Lightweight logging macro for debug builds.
//
// Routes log output to TWO destinations simultaneously:
//   1. OutputDebugStringA  — visible in DebugView / DebugView++
//   2. printf              — visible in the console window attached
//                            by GameApp::Initialize via AllocConsole()
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
#include <windows.h>  // OutputDebugStringA
#include <cstdio>     // snprintf, printf

#ifdef _DEBUG

// ------------------------------------------------------------
// Macro: LOG
// Purpose:
//   Format a message and send it to both the console window and
//   the Windows debug output channel in a single call.
// Why two destinations:
//   - OutputDebugStringA: readable by DebugView++ when running
//     the executable without a console.
//   - printf:             readable in the AllocConsole() window
//     attached by GameApp, which is more convenient during rapid
//     iteration (no external tool required).
// Parameters:
//   fmt  — printf-style format string literal (must be a string literal
//           so the compiler can type-check arguments).
//   ...  — variadic arguments matching the format.
// Caveats:
//   - Buffer is capped at 1024 characters; longer messages are silently
//     truncated. Increase LOG_BUFFER_SIZE if needed.
//   - Not thread-safe; do not call from multiple threads simultaneously.
// ------------------------------------------------------------
#define LOG_BUFFER_SIZE 1024

#define LOG(fmt, ...)                                                         \
    do {                                                                      \
        /* Format the message into a fixed-size stack buffer.               */\
        /* snprintf guarantees null-termination even on truncation.         */\
        char _log_buf[LOG_BUFFER_SIZE];                                       \
        snprintf(_log_buf, LOG_BUFFER_SIZE, fmt "\n", ##__VA_ARGS__);        \
                                                                              \
        /* Send to OutputDebugStringA so DebugView / DebugView++ picks it. */\
        OutputDebugStringA(_log_buf);                                         \
                                                                              \
        /* Also send to stdout so it appears in the AllocConsole window.   */\
        printf("%s", _log_buf);                                               \
    } while (0)

#else
    // In release builds, LOG() expands to nothing.
    // The compiler eliminates all format strings and arguments entirely,
    // so there is zero runtime cost and no strings are embedded in the binary.
    #define LOG(fmt, ...) do {} while (0)
#endif // _DEBUG
