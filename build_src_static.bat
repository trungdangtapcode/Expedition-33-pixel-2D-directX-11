@echo off
setlocal enabledelayedexpansion
REM ============================================================
REM build_src_static.bat - Compile the full src/ project.
REM Run from workspace root: D:\lab\vscworkplace\directX\
REM
REM Usage:
REM   build_src_static.bat          -> Debug build   (default)
REM   build_src_static.bat Release  -> Release build
REM
REM Debug build:   /MTd + /D_DEBUG + /Zi  (debug static CRT, symbols, LOG() active)
REM Release build: /MT  + /DNDEBUG + /O2  (release static CRT, optimized, LOG() = no-op)
REM ============================================================

set MSVC_DIR=D:\VisualStudio\2022\BuildTools\VC\Tools\MSVC\14.40.33807
set WINSDK_DIR=C:\Program Files (x86)\Windows Kits\10
set VCPKG_DIR=D:\lab\vscworkplace\directX\vcpkg\installed\x64-windows-static
set OUT_DIR=bin
set OBJ_DIR=bin\obj
set BUILD_META=%OBJ_DIR%\__build_meta.obj
set HEADER_STAMP=%OBJ_DIR%\__headers_stamp.obj
set COMPILE_RSP=%OBJ_DIR%\__compile_sources.obj
set LINK_RSP=%OBJ_DIR%\__link_objects.obj
set COMPILE_COUNT_FILE=%OBJ_DIR%\__compile_count.obj
set BUILD_REASON_FILE=%OBJ_DIR%\__build_reason.obj

REM --- Determine build configuration ---
set BUILD_TYPE=Debug
if /I "%1"=="Release" set BUILD_TYPE=Release

REM /MTd links against the static debug CRT, required when _DEBUG is defined.
REM /MT links against the static release CRT. Mixing CRT modes with DirectXTK
REM static libraries is a common source of unresolved CRT symbols.
if /I "%BUILD_TYPE%"=="Release" (
    set CRT_FLAG=/MT
    set OPT_FLAG=/O2 /DNDEBUG
    REM Release: link against the Release DirectXTK lib
    set DXTK_LIB_DIR=%VCPKG_DIR%\lib
) else (
    set CRT_FLAG=/MTd
    set OPT_FLAG=/Zi /D_DEBUG
    REM Debug: link against the Debug DirectXTK lib (built with /MTd).
    REM Using the Release lib with /MTd causes LNK2001 on static members
    REM that reference debug-CRT internals (e.g. SpriteBatch::MatrixIdentity).
    set DXTK_LIB_DIR=%VCPKG_DIR%\debug\lib
)

REM Find the Windows SDK version
for /f "delims=" %%i in ('dir /b /ad "%WINSDK_DIR%\Include" 2^>nul') do set WINSDK_VER=%%i

set PATH=%MSVC_DIR%\bin\Hostx64\x64;%PATH%

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"
if not exist "%OBJ_DIR%" mkdir "%OBJ_DIR%"

set CL_FLAGS=/std:c++17 /EHsc /W3 %CRT_FLAG% %OPT_FLAG% /FS /Fd"%OBJ_DIR%\game.pdb" /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /D__CRT_SECURE_NO_WARNINGS /D_CRT_SECURE_NO_WARNINGS /I "%MSVC_DIR%\include" /I "%WINSDK_DIR%\Include\%WINSDK_VER%\um" /I "%WINSDK_DIR%\Include\%WINSDK_VER%\shared" /I "%WINSDK_DIR%\Include\%WINSDK_VER%\ucrt" /I "%WINSDK_DIR%\Include\%WINSDK_VER%\winrt" /I "%VCPKG_DIR%\include" /I "%VCPKG_DIR%\include\directxtk" /I "src"

set CL_SOURCES=src\main.cpp src\Core\GameApp.cpp src\Core\GameTimer.cpp src\Core\Clock.cpp src\Core\TimeSystem.cpp src\Core\InputManager.cpp src\Renderer\D3DContext.cpp src\States\StateManager.cpp src\States\MenuState.cpp src\States\PlayState.cpp src\States\OverworldState.cpp src\States\PauseState.cpp src\States\CampfireState.cpp src\States\MemoryArchiveState.cpp src\States\ExpeditionJournalState.cpp src\States\InventoryState.cpp src\States\InventoryStateDetailPanel.cpp src\States\InventoryStateRender.cpp src\States\LineupState.cpp src\States\LineupStateRender.cpp src\Systems\PartyManager.cpp src\Systems\GameProgress.cpp src\Systems\SaveManager.cpp src\Systems\SettingsManager.cpp src\Systems\LocalizationManager.cpp src\Systems\OverworldThemeManager.cpp src\Systems\StoryDirector.cpp src\Systems\ObjectiveDirector.cpp src\Systems\Wallet.cpp src\Renderer\CircleRenderer.cpp src\Renderer\IrisTransitionRenderer.cpp src\Renderer\PincushionDistortionFilter.cpp src\Renderer\ColorGradeFilter.cpp src\Renderer\NineSliceRenderer.cpp src\Renderer\ItemIconRenderer.cpp src\Renderer\EnvironmentRenderer.cpp src\Renderer\BattleAmbientParticleRenderer.cpp src\Renderer\SpriteRenderer.cpp src\Renderer\UIRenderer.cpp src\Renderer\WorldRenderer.cpp src\Renderer\TileMapRenderer.cpp src\Renderer\WorldSpriteRenderer.cpp src\Scene\SceneGraph.cpp src\Entities\ControllableCharacter.cpp src\Entities\OverworldEnemy.cpp src\Entities\CheckpointCampfire.cpp src\Entities\OverworldStaticProp.cpp src\Entities\OverworldMemoryShard.cpp src\Events\EventManager.cpp src\Debug\DebugTextureViewer.cpp src\Systems\ZoomPincushionTransitionController.cpp src\Systems\CollisionSystem.cpp src\Battle\WeakenEffect.cpp src\Battle\TimedStatBuffEffect.cpp src\Battle\ItemRegistry.cpp src\Battle\ItemIconCache.cpp src\Battle\ItemEffectAction.cpp src\Battle\ItemConsumeAction.cpp src\Battle\BuildItemActions.cpp src\Battle\ItemCommand.cpp src\Systems\Inventory.cpp src\Battle\Combatant.cpp src\Battle\PlayerCombatant.cpp src\Battle\EnemyCombatant.cpp src\Battle\AttackSkill.cpp src\Battle\RageSkill.cpp src\Battle\WeakenSkill.cpp src\Battle\DamageAction.cpp src\Battle\StatusEffectAction.cpp src\Battle\LogAction.cpp src\Battle\WaitAction.cpp src\Battle\IActionDecorator.cpp src\Battle\DelayedAction.cpp src\Battle\MoveAction.cpp src\Battle\PlayAnimationAction.cpp src\Battle\AnimDamageAction.cpp src\Battle\QteAnimDamageAction.cpp src\Battle\RandomEdgeSpawner.cpp src\Battle\SpiralSpawner.cpp src\Battle\SineSpawner.cpp src\Battle\BulletHellAction.cpp src\Battle\CameraPhaseAction.cpp src\Battle\DefaultDamageCalculator.cpp src\Battle\DamageSteps.cpp src\Battle\StatResolver.cpp src\Battle\BattleInputController.cpp src\Battle\FightCommand.cpp src\Battle\FleeCommand.cpp src\Battle\ActionQueue.cpp src\Battle\ParallelAction.cpp src\Battle\BattleResultTracker.cpp src\Battle\BattleManager.cpp src\Battle\CombatantStanceState.cpp src\Battle\BattleCameraController.cpp src\Battle\BattleRenderer.cpp src\States\BattleState.cpp src\UI\TitleMenuRenderer.cpp src\UI\PauseMenuRenderer.cpp src\UI\HealthBarRenderer.cpp src\UI\ExpBarRenderer.cpp src\UI\BattleTextRenderer.cpp src\UI\CurrencyHudRenderer.cpp src\UI\EnemyHpBarRenderer.cpp src\UI\PointerRenderer.cpp src\UI\ScrollArrowRenderer.cpp src\UI\BattleDebugHUD.cpp src\UI\TurnQueueUI.cpp src\UI\BattleQTERenderer.cpp src\UI\BattleBulletHellRenderer.cpp src\UI\BattleResultRenderer.cpp src\Audio\AudioManager.cpp src\Audio\WavLoader.cpp src\Audio\SfxPlayer.cpp src\Audio\MediaLoader.cpp

set CL_SOURCES=%CL_SOURCES% src\States\DialogueState.cpp src\Systems\DialogueManager.cpp src\Entities\OverworldNpc.cpp src\UI\DialogueRenderer.cpp

set CL_SOURCES=%CL_SOURCES% src\Battle\ShieldWallSpawner.cpp

set CL_SOURCES=%CL_SOURCES% src\Battle\BattleResourceRules.cpp src\Battle\ConsumeMpAction.cpp src\Battle\HealAction.cpp src\Battle\RestoreMpAction.cpp src\Battle\RestoreMpPercentAction.cpp src\Battle\RageGainAction.cpp src\Battle\RageSpendAction.cpp src\Battle\ReactionDefenseAction.cpp src\Battle\ReviveAction.cpp src\Battle\CleanseAction.cpp src\Battle\DataDrivenSkill.cpp src\Battle\DataDrivenStatusEffect.cpp src\Battle\SkillFactory.cpp src\Battle\StatusDamageAction.cpp src\Battle\StatusEffectRegistry.cpp src\UI\StatusIconRenderer.cpp src\UI\BattleSkillMenuRenderer.cpp src\UI\ObjectiveBeaconRenderer.cpp src\UI\ObjectiveTrackerRenderer.cpp


set CL_LINKS=/LIBPATH:"%MSVC_DIR%\lib\x64" /LIBPATH:"%WINSDK_DIR%\Lib\%WINSDK_VER%\um\x64" /LIBPATH:"%WINSDK_DIR%\Lib\%WINSDK_VER%\ucrt\x64" /LIBPATH:"%DXTK_LIB_DIR%" user32.lib gdi32.lib d3d11.lib dxgi.lib d3dcompiler.lib DirectXTK.lib ole32.lib mfplat.lib mfreadwrite.lib mfuuid.lib /SUBSYSTEM:WINDOWS
set BUILD_SIGNATURE=%BUILD_TYPE%^|%CRT_FLAG%^|%OPT_FLAG%^|%DXTK_LIB_DIR%^|%CL_FLAGS%^|%CL_LINKS%


echo ============================================================
echo  Building: My Game - DirectX 11
echo ============================================================
echo.

echo Preparing incremental compile cache...
powershell -NoProfile -ExecutionPolicy Bypass -File tools\prepare_incremental_build.ps1 Prepare
if errorlevel 1 goto BuildFailed

set /p COMPILE_COUNT=<"%COMPILE_COUNT_FILE%"
set /p BUILD_REASON=<"%BUILD_REASON_FILE%"

if "!COMPILE_COUNT!"=="0" (
    echo [compile] No C++ source changes detected; reusing cached .obj files.
) else (
    echo [compile] !COMPILE_COUNT! files require compilation: !BUILD_REASON!
    echo Compiling changed files with multi-core acceleration using /MP4...
    cl.exe /c @"%COMPILE_RSP%" /Fo:%OBJ_DIR%\ /nologo /MP4 %CL_FLAGS%
    if errorlevel 1 goto BuildFailed
)

echo Linking cached object set...
link.exe /nologo @"%LINK_RSP%" /OUT:%OUT_DIR%\game.exe %CL_LINKS%
if errorlevel 1 goto BuildFailed

powershell -NoProfile -ExecutionPolicy Bypass -File tools\prepare_incremental_build.ps1 Commit
if errorlevel 1 goto BuildFailed

echo.
echo [OK] Build succeeded ^> %OUT_DIR%\game.exe  [%BUILD_TYPE%]
REM Copy DirectXTK.dll only when the local package provides one.
REM The static build usually links DirectXTK.lib only, so a missing DLL
REM should not turn a successful link into a failed batch exit code.
set DXTK_DLL_DIR=%VCPKG_DIR%\bin
if /I "%BUILD_TYPE%"=="Debug" set DXTK_DLL_DIR=%VCPKG_DIR%\debug\bin
if exist "!DXTK_DLL_DIR!\DirectXTK.dll" copy "!DXTK_DLL_DIR!\DirectXTK.dll" %OUT_DIR%\ >nul
exit /b 0

:BuildFailed
echo.
echo [ERROR] Build failed. See errors above.
exit /b 1
