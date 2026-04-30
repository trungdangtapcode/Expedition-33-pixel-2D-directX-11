@echo off
setlocal enabledelayedexpansion
REM ============================================================
REM build_src.bat — Compile the full src/ project.
REM Run from workspace root: D:\lab\vscworkplace\directX\
REM
REM Usage:
REM   build_src.bat          -> Debug build   (default)
REM   build_src.bat Release  -> Release build
REM
REM Debug build:   /MDd + /D_DEBUG + /Zi  (debug CRT, symbols, LOG() active)
REM Release build: /MD  + /DNDEBUG + /O2  (release CRT, optimized, LOG() = no-op)
REM ============================================================

set MSVC_DIR=D:\VisualStudio\2022\BuildTools\VC\Tools\MSVC\14.40.33807
set WINSDK_DIR=C:\Program Files (x86)\Windows Kits\10
set VCPKG_DIR=D:\lab\vscworkplace\directX\vcpkg\installed\x64-windows-static
set OUT_DIR=bin
set OBJ_DIR=bin\obj

REM --- Determine build configuration ---
set BUILD_TYPE=Debug
if /I "%1"=="Release" set BUILD_TYPE=Release

REM /MDd links against msvcrtd.lib (debug CRT) — required when _DEBUG is defined,
REM because STL headers use _CrtDbgReport and _invalid_parameter from that library.
REM /MD  links against msvcrt.lib  (release CRT) — these symbols do not exist there.
REM Mixing /MD with /D_DEBUG is the root cause of LNK2019 on _CrtDbgReport.
if /I "%BUILD_TYPE%"=="Release" (
    set CRT_FLAG=/MT
    set OPT_FLAG=/O2 /DNDEBUG
    REM Release: link against the Release DirectXTK lib
    set DXTK_LIB_DIR=%VCPKG_DIR%\lib
) else (
    set CRT_FLAG=/MTd
    set OPT_FLAG=/Zi /D_DEBUG
    REM Debug: link against the Debug DirectXTK lib (built with /MDd).
    REM Using the Release lib with /MDd causes LNK2001 on static members
    REM that reference debug-CRT internals (e.g. SpriteBatch::MatrixIdentity).
    set DXTK_LIB_DIR=%VCPKG_DIR%\debug\lib
)

REM Find the Windows SDK version
for /f "delims=" %%i in ('dir /b /ad "%WINSDK_DIR%\Include" 2^>nul') do set WINSDK_VER=%%i

set PATH=%MSVC_DIR%\bin\Hostx64\x64;%PATH%

if not exist %OUT_DIR% mkdir %OUT_DIR%
if not exist %OBJ_DIR% mkdir %OBJ_DIR%

set CL_FLAGS=/std:c++17 /EHsc /W3 %CRT_FLAG% %OPT_FLAG% /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /D__CRT_SECURE_NO_WARNINGS /D_CRT_SECURE_NO_WARNINGS /I "%MSVC_DIR%\include" /I "%WINSDK_DIR%\Include\%WINSDK_VER%\um" /I "%WINSDK_DIR%\Include\%WINSDK_VER%\shared" /I "%WINSDK_DIR%\Include\%WINSDK_VER%\ucrt" /I "%WINSDK_DIR%\Include\%WINSDK_VER%\winrt" /I "%VCPKG_DIR%\include" /I "%VCPKG_DIR%\include\directxtk" /I "src"

set CL_SOURCES=src\main.cpp src\Core\GameApp.cpp src\Core\GameTimer.cpp src\Core\Clock.cpp src\Core\TimeSystem.cpp src\Core\InputManager.cpp src\Renderer\D3DContext.cpp src\States\StateManager.cpp src\States\MenuState.cpp src\States\PlayState.cpp src\States\OverworldState.cpp src\States\InventoryState.cpp src\States\InventoryStateDetailPanel.cpp src\States\InventoryStateRender.cpp src\Systems\PartyManager.cpp src\Renderer\CircleRenderer.cpp src\Renderer\IrisTransitionRenderer.cpp src\Renderer\PincushionDistortionFilter.cpp src\Renderer\NineSliceRenderer.cpp src\Renderer\EnvironmentRenderer.cpp src\Renderer\SpriteRenderer.cpp src\Renderer\UIRenderer.cpp src\Renderer\WorldRenderer.cpp src\Renderer\TileMapRenderer.cpp src\Renderer\WorldSpriteRenderer.cpp src\Scene\SceneGraph.cpp src\Entities\ControllableCharacter.cpp src\Entities\OverworldEnemy.cpp src\Events\EventManager.cpp src\Debug\DebugTextureViewer.cpp src\Systems\ZoomPincushionTransitionController.cpp src\Systems\CollisionSystem.cpp src\Battle\WeakenEffect.cpp src\Battle\TimedStatBuffEffect.cpp src\Battle\ItemRegistry.cpp src\Battle\ItemEffectAction.cpp src\Battle\ItemConsumeAction.cpp src\Battle\BuildItemActions.cpp src\Battle\ItemCommand.cpp src\Systems\Inventory.cpp src\Battle\Combatant.cpp src\Battle\PlayerCombatant.cpp src\Battle\EnemyCombatant.cpp src\Battle\AttackSkill.cpp src\Battle\RageSkill.cpp src\Battle\WeakenSkill.cpp src\Battle\DamageAction.cpp src\Battle\StatusEffectAction.cpp src\Battle\LogAction.cpp src\Battle\WaitAction.cpp src\Battle\IActionDecorator.cpp src\Battle\DelayedAction.cpp src\Battle\MoveAction.cpp src\Battle\PlayAnimationAction.cpp src\Battle\AnimDamageAction.cpp src\Battle\QteAnimDamageAction.cpp src\Battle\BulletHellAction.cpp src\Battle\CameraPhaseAction.cpp src\Battle\DefaultDamageCalculator.cpp src\Battle\DamageSteps.cpp src\Battle\StatResolver.cpp src\Battle\BattleInputController.cpp src\Battle\FightCommand.cpp src\Battle\FleeCommand.cpp src\Battle\ActionQueue.cpp src\Battle\BattleManager.cpp src\Battle\CombatantStanceState.cpp src\Battle\BattleCameraController.cpp src\Battle\BattleRenderer.cpp src\States\BattleState.cpp src\UI\HealthBarRenderer.cpp src\UI\ExpBarRenderer.cpp src\UI\BattleTextRenderer.cpp src\UI\EnemyHpBarRenderer.cpp src\UI\PointerRenderer.cpp src\UI\ScrollArrowRenderer.cpp src\UI\BattleDebugHUD.cpp src\UI\TurnQueueUI.cpp src\UI\BattleQTERenderer.cpp src\UI\BattleBulletHellRenderer.cpp src\Audio\AudioManager.cpp

set CL_LINKS=/LIBPATH:"%MSVC_DIR%\lib\x64" /LIBPATH:"%WINSDK_DIR%\Lib\%WINSDK_VER%\um\x64" /LIBPATH:"%WINSDK_DIR%\Lib\%WINSDK_VER%\ucrt\x64" /LIBPATH:"%DXTK_LIB_DIR%" user32.lib gdi32.lib d3d11.lib dxgi.lib d3dcompiler.lib DirectXTK.lib ole32.lib /SUBSYSTEM:WINDOWS


echo ============================================================
echo  Building: My Game - DirectX 11
echo ============================================================
echo.

set CL_OBJS=
for %%f in (%CL_SOURCES%) do (
    set CL_OBJS=!CL_OBJS! %OBJ_DIR%\%%~nf.obj
)

echo Compiling files with multi-core acceleration (/MP)...
cl.exe /c %CL_SOURCES% /Fo:%OBJ_DIR%\ /nologo /MP %CL_FLAGS%

link.exe /nologo !CL_OBJS! /OUT:%OUT_DIR%\game.exe %CL_LINKS%


if %ERRORLEVEL% == 0 (
    echo.
    echo [OK] Build succeeded ^> %OUT_DIR%\game.exe  [%BUILD_TYPE%]
    REM Copy the correct DirectXTK DLL (debug or release) next to the executable.
    REM Debug build links against the debug DLL; using the release DLL with /MDd
    REM will crash at startup with a CRT mismatch assertion.
    set DXTK_DLL_DIR=%VCPKG_DIR%\bin
    if /I "%BUILD_TYPE%"=="Debug" set DXTK_DLL_DIR=%VCPKG_DIR%\debug\bin
    copy "!DXTK_DLL_DIR!\DirectXTK.dll" %OUT_DIR%\ >nul
) else (
    echo.
    echo [ERROR] Build failed. See errors above.
    exit /b 1
)
