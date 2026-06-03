# Executable Icon

## Purpose

The game executable uses `assets/game.ico` as its Windows icon. The icon is
embedded into `bin/game.exe` through a Win32 resource script, and `GameApp`
loads the same resource for the window class icon.

## Files

```text
assets/game.ico
src/resource.h
src/game.rc
build_src_static.bat
src/Core/GameApp.cpp
```

`src/resource.h` owns the stable `IDI_APP_ICON` resource id. `src/game.rc`
binds that id to `assets/game.ico`.

## Build Flow

`build_src_static.bat` compiles the resource script with the Windows SDK
resource compiler:

```bat
rc.exe /nologo /I "src" /fo "bin\obj\game_icon.res" src\game.rc
```

The generated `.res` file is passed to `link.exe` with the cached object list.
This embeds the icon into the executable itself, so Windows Explorer and the
taskbar can display the game icon.

## Runtime Flow

`GameApp::InitWindow()` loads `IDI_APP_ICON` from the executable resource and
assigns it to both `WNDCLASSEX::hIcon` and `WNDCLASSEX::hIconSm`. If the
resource cannot be loaded, the code falls back to the default Windows
application icon instead of failing startup.

## Update Rule

To change the icon, replace `assets/game.ico` with another valid `.ico` file and
re-run:

```bat
.\build_src_static.bat 2>&1
```
