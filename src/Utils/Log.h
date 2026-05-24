// ============================================================
// File: Log.h
// Responsibility: Lightweight logging macro for debug builds.
//
// Routes log output to three debug destinations:
//   1. OutputDebugStringA, visible in DebugView / DebugView++
//   2. printf, visible in the console allocated by GameApp
//   3. log_output.txt, useful when the console scrollback is gone
//
// Output format:
//   [HH:MM:SS.mmm] <message>
//
// Important:
//   - LOG is active only in Debug builds.
//   - CLI output is sanitized to printable ASCII after formatting.
//     Gameplay UI can use UTF-8 localization, but logs must remain readable
//     in consoles that do not render the active language correctly.
// ============================================================
#pragma once
#include <windows.h>  // OutputDebugStringA, GetLocalTime, SYSTEMTIME
#include <cstdio>     // snprintf, printf

#ifdef _DEBUG

#define LOG_BUFFER_SIZE 1024

inline void LogSanitizeCliBuffer(char* text)
{
    if (!text) return;

    for (char* cursor = text; *cursor != '\0'; ++cursor)
    {
        const unsigned char ch = static_cast<unsigned char>(*cursor);
        if (ch == '\n' || ch == '\r' || ch == '\t')
        {
            continue;
        }
        if (ch < 32 || ch > 126)
        {
            *cursor = '?';
        }
    }
}

// ------------------------------------------------------------
// Macro: LOG
// Purpose:
//   Prepend a wall-clock timestamp to every debug message, sanitize the
//   result for CLI surfaces, then send it to OutputDebugStringA, stdout,
//   and log_output.txt.
// Caveats:
//   - Total buffer is 1024 chars including timestamp.
//   - Not thread-safe; call from the main game thread.
// ------------------------------------------------------------
#define LOG(fmt, ...)                                                              \
    do {                                                                           \
        SYSTEMTIME _log_st;                                                        \
        GetLocalTime(&_log_st);                                                    \
                                                                                   \
        char _log_buf[LOG_BUFFER_SIZE];                                            \
        _log_buf[0] = '\0';                                                        \
        int _log_prefix = snprintf(_log_buf, LOG_BUFFER_SIZE,                      \
            "[%02d:%02d:%02d.%03d] ",                                              \
            _log_st.wHour, _log_st.wMinute, _log_st.wSecond,                       \
            _log_st.wMilliseconds);                                                \
        if (_log_prefix > 0 && _log_prefix < LOG_BUFFER_SIZE) {                    \
            snprintf(_log_buf + _log_prefix,                                       \
                     LOG_BUFFER_SIZE - _log_prefix,                                \
                     fmt "\n", ##__VA_ARGS__);                                    \
        }                                                                          \
                                                                                   \
        LogSanitizeCliBuffer(_log_buf);                                            \
        OutputDebugStringA(_log_buf);                                              \
        printf("%s", _log_buf);                                                    \
        FILE* _log_file = fopen("log_output.txt", "a");                           \
        if (_log_file) {                                                           \
            fputs(_log_buf, _log_file);                                            \
            fclose(_log_file);                                                     \
        }                                                                          \
    } while (0)

#else
    #define LOG(fmt, ...) do {} while (0)
#endif
