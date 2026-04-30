# MSVC Standard Incremental Build System

## 1. Overview
The project avoids external build generators like **CMake, MSBuild, Ninja, or Make**. In order to achieve extremely fast and scalable compilation without bloated project setups, we utilize standard MSVC execution mechanisms implemented natively via Windows Batch scripting inside `build_src_static.bat`.

## 2. Why Not Custom Python or Regex Parsing?
During the prototyping of an incremental build system, an attempt was made to write a Python script that evaluated out-of-date files by manually parsing `#include "..."` patterns using Regex.

From a professional Build Engineering perspective, **this is an anti-pattern for C++**. MSVC compilation paths are highly complex and cannot be perfectly resolved through simple text tracking due to:
*   **Transitive Dependencies**: Indirectly included headers (headers including other headers via macros) are invisible to shallow Regex tracking.
*   **Precompiled Headers (PCH)**: PCH alters the global compilation inclusion graph structurally.
*   **Windows SDK & System Headers**: DirectXTK, ComPtr, and system structures live outside workspace trees and modify local bindings. 

Attempting to track timestamp dependencies manually outside the compiler will inevitably fracture the build cache, generating elusive `undefined behavior` and phantom `LNK2019` errors that only magically resolve upon a "Clean Rebuild."

## 3. The Correct Native Batch Pattern
Rather than reinventing Ninja or MSBuild, we simply let `cl.exe` operate precisely how it was engineered. 
By orchestrating compilation inside a clean object-output `for` loop independently of the linking phase, the MSVC pipeline inherently tracks and isolates Translation Unit configurations flawlessly!

### The Architecture (`build_src_static.bat`)
We define explicit blocks for Flags, Sources, and Likers in strict accordance with the project rules. We then trigger the pure batch pipeline:

```bat
REM 1. Individually Execute compilation isolating object artifacts
for %%f in (%CL_SOURCES%) do (
    cl.exe /c %%f /Fo:%OBJ_DIR%\%%~nf.obj /nologo %CL_FLAGS%
)

REM 2. Inherently link evaluated binaries securely linking cache artifacts directly
link.exe /nologo %OBJ_DIR%\*.obj /OUT:%OUT_DIR%\game.exe %CL_LINKS%
```

**Why this works perfectly:**
*   MSVC controls the Preprocessor Graph perfectly dynamically.
*   DirectXTK linkages resolve natively without parsing interference.
*   The setup is strictly contained in less than 15 lines of raw scripting without launching a single non-native application.

## 4. Usage
To compile the game, just use the terminal inside the root directory and allow the native pipeline caching structure to parse securely:

```powershell
# For Development (Includes Logging and Debug symbols)
.\build_src_static.bat

# For Production (Applies /O2 Optimizations)
.\build_src_static.bat Release
```
