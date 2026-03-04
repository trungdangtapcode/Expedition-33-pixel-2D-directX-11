// ============================================================
// File: HrCheck.h
// Responsibility: Macros for catching silent HRESULT failures in D3D11 calls.
//
// --- Why this exists ---
//   Without the D3D11 Debug Layer (requires "Graphics Tools" optional feature),
//   failed D3D11 calls return a non-S_OK HRESULT but produce NO visible output.
//   The GPU silently drops the draw call, the screen stays dark, and there is
//   no error message to follow.  These macros convert silent failures into
//   immediate, loud failures with file+line information so the exact bad call
//   is identified on the first run.
//
// --- How to enable the Debug Layer ---
//   Windows Settings > System > Optional Features > Add a feature
//   → search "Graphics Tools" → Install
//   Once installed, D3DContext will no longer log the
//   "WARNING: Debug layer unavailable" message, and the D3D11 runtime will
//   print detailed ERROR/WARNING lines to OutputDebugStringA for every
//   state mismatch, resource leak, or invalid call.
//
// --- Macros ---
//
//   CHECK_HR(hr, msg)
//     If hr is a failure code, logs the HRESULT + msg with file/line,
//     then triggers __debugbreak() (pauses in the debugger if attached,
//     otherwise terminates).  Use for calls that must not fail.
//
//   LOG_HR(hr, msg)
//     Same logging, but NO __debugbreak() — continues execution.
//     Use for calls where you want to report failure but gracefully recover.
//
// Usage:
//   HRESULT hr = device->CreateBuffer(&desc, nullptr, buf.GetAddressOf());
//   CHECK_HR(hr, "CreateBuffer for vertex buffer");
//
// Lifetime: Header-only.  No .cpp, no link dependency.
// ============================================================
#pragma once
#include "Log.h"

// ------------------------------------------------------------
// Macro: CHECK_HR
// Purpose:
//   Assert that an HRESULT is S_OK.  On failure: log file+line+message,
//   then __debugbreak() so the debugger halts exactly at the bad call.
// Why __debugbreak() instead of assert():
//   assert() prints to stderr (not captured by OutputDebugStringA) and
//   calls abort() which produces a crash dialog with no D3D context.
//   __debugbreak() pauses execution inside the debugger — you can inspect
//   the call stack and all variables at the exact point of failure.
// In release builds:
//   CHECK_HR expands to a plain (void)hr — zero overhead, no strings.
// ------------------------------------------------------------
#ifdef _DEBUG
    #define CHECK_HR(hr, msg)                                                      \
        do {                                                                       \
            if (FAILED(hr)) {                                                      \
                LOG("[CHECK_HR] FAILED 0x%08X — %s  (%s:%d)",                    \
                    (unsigned)(hr), (msg), __FILE__, __LINE__);                   \
                __debugbreak();                                                    \
            }                                                                      \
        } while (0)

    // ------------------------------------------------------------
    // Macro: LOG_HR
    // Purpose:
    //   Log an HRESULT failure without halting execution.
    //   Use when you want to report a recoverable failure and continue.
    // ------------------------------------------------------------
    #define LOG_HR(hr, msg)                                                        \
        do {                                                                       \
            if (FAILED(hr)) {                                                      \
                LOG("[LOG_HR] FAILED 0x%08X — %s  (%s:%d)",                      \
                    (unsigned)(hr), (msg), __FILE__, __LINE__);                   \
            }                                                                      \
        } while (0)
#else
    #define CHECK_HR(hr, msg) do { (void)(hr); } while (0)
    #define LOG_HR(hr, msg)   do { (void)(hr); } while (0)
#endif // _DEBUG
