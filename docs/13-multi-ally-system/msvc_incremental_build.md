# MSVC Incremental Build Cache

## Overview

`build_src_static.bat` keeps the project on the manual MSVC toolchain while avoiding a full compile on every edit. The source list remains explicit in the batch file, but a small PowerShell helper prepares response files for `cl.exe` and `link.exe`.

The build now has three layers:

1. Keep one cached `.obj` per `.cpp` under `bin/obj`.
2. Compile only stale `.cpp` files when source files change.
3. Rebuild all `.cpp` files when headers or compiler/linker settings change.

## Why Header Changes Rebuild Everything

The project does not yet emit compiler dependency files. Rather than guessing the C++ include graph with fragile text parsing, the script uses a conservative rule:

- `.cpp` newer than its `.obj` -> compile that one translation unit.
- Any `src/**/*.h`, `src/**/*.hpp`, or `src/**/*.inl` newer than the header stamp -> full C++ rebuild.
- Build type, compiler flags, library paths, or linker inputs changed -> full C++ rebuild.

This keeps incremental builds fast for normal implementation edits while avoiding stale objects after shared interface changes.

## Files

- `build_src_static.bat` owns the MSVC paths, flags, source list, and linker libraries.
- `tools/prepare_incremental_build.ps1` owns stale-source detection and response-file generation.
- `bin/obj/__compile_sources.obj` lists only sources that need recompilation. It is a response file with an ignored extension.
- `bin/obj/__link_objects.obj` lists the complete object set for the linker. It is a response file with an ignored extension.
- `bin/obj/__build_meta.obj` stores the current build signature.
- `bin/obj/__headers_stamp.obj` stores the last successful header-validation timestamp.

`bin/obj` is build output and should remain untracked.

## Expected Behavior

After a clean checkout or build-setting change, the first build compiles every source file. The next build should usually print:

```text
[compile] No C++ source changes detected; reusing cached .obj files.
Linking cached object set...
[OK] Build succeeded > bin\game.exe  [Debug]
```

After editing one `.cpp`, the script compiles only that file and then links the full cached object set.

After editing a header, the script performs a full rebuild because any translation unit may depend on that header.

## Common Mistakes

1. Adding a new `.cpp` without adding it to `CL_SOURCES` means it will never be compiled or linked.
2. Creating two `.cpp` files with the same basename creates an object-name collision because objects are still named `%%~nf.obj`.
3. Deleting `bin/obj/__build_meta.obj` or `bin/obj/__headers_stamp.obj` forces a full cache refresh on the next build.
4. Switching Debug and Release uses the same object directory, so the build signature intentionally forces a full rebuild when the configuration changes.
