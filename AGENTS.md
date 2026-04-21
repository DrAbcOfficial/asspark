# asspark

## Project Overview

`asspark` is a **Metamod plugin** for the game **Sven Co-op**. It implements an **AngelScript profiler** (codenamed "SPARK") that measures script execution time by hooking into the AngelScript engine's context request/return cycle.

The plugin is a standalone C++20 DLL project that sits at the repository root, separate from the `metamod/` submodule which contains the modified Metamod runtime and related plugins (`asext`, `fallguys`, `ascurl`, `asqcvar`, `asusermsg`).

### What it does

- Hooks `asIScriptEngine::RequestContext` (vtable index 95) and `asIScriptEngine::ReturnContext` (vtable index 96) via inline hooks provided by Metamod.
- Records per-script-section, per-line, per-column timing statistics using `std::chrono::high_resolution_clock`.
- Exposes server console commands for dumping and clearing profiler data:
  - `spark_dump_all` — dumps aggregated timing per (section, line, column).
  - `spark_dump_timepoints` — dumps currently active (unreturned) timepoints.
  - `spark_clear_all` — clears aggregated statistics.
  - `spark_clear_timepoints` — clears the active timepoint stack.
- Registers a cvar `spark_on` (currently defined but not actively gating the hooks).

### Hard dependency

`asspark` **requires** the `asext` plugin (`asext.dll` / `asext.so`) to be loaded first, because it imports the `ASEXT_GetServerManager` and inline-hook utility APIs at runtime. `asext` is part of the `metamod/` submodule.

## Technology Stack

- **Language**: C++20 (MSVC `/std:c++20`), C17 for C compilation units.
- **Platform**: Windows x86 (Win32) primary target. The code uses `__fastcall` conventions matching Sven Co-op's server binaries on Windows (`SC_SERVER_DECL`).
- **Build System**: Visual Studio 2022 `.vcxproj` / `.slnx` (MSBuild). There is **no** CMake or Makefile for this specific plugin at the root level.
- **Runtime**: Metamod plugin loaded into the Half-Life / GoldSrc engine.
- **Dependencies**:
  - Half-Life SDK headers (`metamod/hlsdk/{common,dlls,pm_shared,engine}`)
  - Metamod headers (`metamod/metamod/`)
  - AngelScript SDK headers (`metamod/thirdparty/angelscript-sdk/angelscript/include`)
  - `asext` API headers (`metamod/asext/include` — specifically `asext_api.h` and `std_string.h`)

## Project Structure

```
asspark/                          <- Repository root
├── asspark.slnx                  <- Visual Studio solution (x86 only)
├── asspark.vcxproj               <- Main project file (Win32 DLL)
├── asspark.vcxproj.user          <- User-specific VS settings
│
├── dllapi.cpp                    <- Core plugin logic (profiler hooks, console commands)
├── meta_api.cpp                  <- Metamod plugin interface (Meta_Query, Meta_Attach, Meta_Detach)
├── h_export.cpp                  <- Engine function table receipt (GiveFnptrsToDll)
├── dllmain.cpp                   <- Windows DLL entry point (no-op)
├── sdk_util.cpp                  <- Minimal SDK utility (UTIL_LogPrintf)
├── dlldef.h                      <- Empty placeholder header
├── signatures.h                  <- Convenience macro: FILL_AND_HOOK
│
├── metamod/                      <- Git submodule (https://github.com/hzqst/metamod-fallguys)
│   ├── metamod/                  <- Modified Metamod runtime source
│   ├── asext/                    <- AngelScript extension plugin (required dependency)
│   ├── fallguys/                 <- Another Metamod plugin (unrelated to asspark)
│   ├── ascurl/                   <- HTTP request plugin for AngelScript
│   ├── asqcvar/                  <- Client cvar retrieval plugin
│   ├── asusermsg/                <- UserMsg hook plugin
│   ├── hlsdk/                    <- Half-Life SDK headers
│   ├── thirdparty/               <- AngelScript SDK, Bullet3, Capstone, etc.
│   ├── scripts/                  <- Build batch files / shell scripts
│   └── build/                    <- Staging output for binaries
│
├── Debug/                        <- MSBuild debug output (ignored by .gitignore)
└── .gitignore                    <- Ignores VS intermediates, build outputs
```

## Build Instructions

### Prerequisites (Windows)

- Visual Studio 2022 with **Desktop development with C++** workload.
- VC143 (or compatible) toolset targeting **Win32** (x86).
- Windows 10/11 SDK.
- The `metamod/` submodule must be initialized and present:
  ```powershell
  git submodule update --init --recursive
  ```

### Build Steps

1. Open `asspark.slnx` in Visual Studio, or build from the command line:
   ```powershell
   msbuild asspark.slnx /p:Configuration=Debug /p:Platform=Win32
   # or
   msbuild asspark.slnx /p:Configuration=Release /p:Platform=Win32
   ```

2. The project has a **Post-Build Event** in Release configuration that copies the resulting DLL to:
   ```
   $(ProjectDir)/../build/addons/metamod/dlls/
   ```
   This path is relative to the repository root.

### Output Artifacts

- `Debug/asspark.dll` / `Release/asspark.dll`
- Corresponding `.pdb`, `.lib`, `.exp` files.

> **Note**: There is no Linux Makefile or CMakeLists.txt for `asspark` at the root level. Cross-platform builds would need to be added manually if required.

## Code Style and Conventions

- **File header style**: Many files inherited from Metamod / HL SDK use `// vi: set ts=4 sw=4 :` and classic C-style block comments for copyright.
- **Naming**: The plugin code uses a mix of:
  - `s_` prefix for file-scope static globals (e.g., `s_All`, `s_TimeList`).
  - `SC_SERVER_DECL` / `SC_SERVER_DUMMYARG` macros for calling-convention compatibility with the game server.
  - `asSpark` prefix for profiler-specific classes (`asSparkContextItem`, `asSparkTimePoint`, `asSparkItem`).
- **Braces**: K&R / Java-style (opening brace on the same line) is used in the plugin-specific code.
- **Modern C++**: The plugin uses `std::unique_ptr`, `std::unordered_map`, `std::stack`, `std::format`, and `std::chrono`.
- **Preprocessor definitions**: The `.vcxproj` currently defines `FALLGUYS_EXPORTS` and uses `RootNamespace=fallguys` — these are legacy artifacts from the project template and do not affect functionality.

## Runtime Architecture

### Hook Installation

1. `Meta_Attach` loads `asext.dll` dynamically via Metamod's `LOAD_PLUGIN` macro and imports all `ASEXT_*` APIs.
2. `ServerActivate` (DLL API, called after map load) performs the actual inline hooks:
   - Resolves the `asIScriptEngine` vtable.
   - Hooks vtable index 95 (`RequestContext`) and 96 (`ReturnContext`) using `gpMetaUtilFuncs->pfnInlineHook`.
3. The hooks push/pop `asSparkTimePoint` objects onto a thread-local stack (`s_TimeList`) and aggregate elapsed time into `s_All`.

### Console Commands

Registration happens in `GameInitPost` (DLL API post-hook) via `g_engfuncs.pfnAddServerCommand`.

### Data Structures

- `s_TimeList` — `std::stack<std::unique_ptr<asSparkTimePoint>>` representing nested/in-flight script calls.
- `s_All` — `std::unordered_map<size_t, std::unique_ptr<asSparkItem>>` keyed by hash of (section, line, column).

## Testing and Debugging

- **No automated test suite** exists for this plugin. Validation is done by:
  1. Building the DLL.
  2. Placing it in `build/addons/metamod/dlls/`.
  3. Loading it via Metamod's `plugins.ini` **after** `asext`.
  4. Running Sven Co-op and using the `spark_*` console commands.
- Use the generated `.pdb` for source-level debugging in Visual Studio.
- The `debug-helper-AIO.bat` and `debug-SvenCoop.bat` scripts inside `metamod/scripts/` can help attach a debugger to the dedicated server.

## Deployment

`asspark` is deployed as a standard Metamod plugin:

1. Ensure `metamod.dll` (from the submodule build) and `asext.dll` are present in `addons/metamod/dlls/`.
2. Copy `asspark.dll` to the same `addons/metamod/dlls/` directory.
3. Add `asspark` to `addons/metamod/plugins.ini` **below** `asext`.

Example `plugins.ini`:
```ini
linux addons/metamod/dlls/asext.so
linux addons/metamod/dlls/asspark.so
win32 addons/metamod/dlls/asext.dll
win32 addons/metamod/dlls/asspark.dll
```

## Important Caveats

- **Load order matters**: If `asext` is not loaded before `asspark`, `Meta_Attach` will fail and log an error.
- **Vtable indices are hardcoded**: Indices 95 and 96 correspond to the specific AngelScript build shipped with Sven Co-op. If the game updates its AngelScript version, these indices may shift and require recalibration.
- **No unhook on detach**: `Meta_Detach` is a no-op; the inline hooks are not removed on plugin unload.
- **x86 only**: The solution is configured strictly for Win32. There is no x64 or ARM build configuration.
