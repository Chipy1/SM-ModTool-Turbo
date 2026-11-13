# SM ModTool Turbo

A drop-in helper for the **Scrap Mechanic Mod Tool** that:

- **Auto-confirms Workshop upload warnings** — the three "are you sure?"
  dialogs (content errors, out-of-date item, overwrite existing item) no
  longer stop an upload.
- **Skips redundant content compilation** — if the mod content and the
  `ContentCompiler.exe` build are unchanged and the generated `Cache` still
  exists, the compiler process is skipped. The first run is unchanged; later
  uploads of the same content are much faster.
- Leaves `ModTool.exe`, the game and all mod files untouched. It is a
  `d3d11.dll` proxy loaded by the Mod Tool itself.

## Tested build

- `ModTool.exe` SHA-256:
  `853dc33c1a6ee300f4152cfc76564c71928efdc0d34fc7bcd4b3e85cbaa9df4e`
- Scrap Mechanic 1.0.6.889 (the Mod Tool shipped with that build).
- `dist/d3d11.dll` (v1.0.5) SHA-256:
  `b7b02d152e3421d7699efdd1619425f71c8c7209a76d2e521119bc6b7212c4dd`

The tool is deliberately **not tied to fixed addresses**. It uses the
Mod Tool's import table and byte patterns, so a new Mod Tool build will
either keep working or disable the affected feature and write a warning to
`ModToolTurbo.log`. It will not crash on an unknown build.

## Install

1. Close the Scrap Mechanic Mod Tool.
2. Copy these two files into the same folder as `ModTool.exe`
   (normally `...\Scrap Mechanic Mod Tool\Release`):
   - `d3d11.dll`
   - `ModToolTurbo.ini`
3. Start the Mod Tool. Check `ModToolTurbo.log` in the same folder.

If you used v1.0.0, delete its `version.dll` first; that proxy is bypassed
by Wine/Proton. The `d3d11.dll` build replaces it.

To uninstall, delete both files. Do not leave a stale `d3d11.dll` from
another tool in that folder; back it up first if one already exists.

### Linux / Proton

The Mod Tool runs through Proton. Install the files into the Windows-side
Mod Tool folder (the folder that contains `ModTool.exe`), for example:

```text
.../steamapps/common/Scrap Mechanic Mod Tool/Release
```

Proton sets `d3d11=native`, so the app-local `d3d11.dll` is loaded. The
proxy forwards the two D3D11 entry points to the real implementation and
loads the compile cache state under the prefix's
`%LOCALAPPDATA%\SMModToolTurbo`.

## Configuration

`ModToolTurbo.ini` next to `d3d11.dll`:

```ini
[General]
Log=1

[Upload]
AutoConfirm=1

[Compile]
SkipUnchanged=1
Force=0
PriorityBoost=1
```

| Key | Default | Meaning |
| --- | --- | --- |
| `General/Log` | `1` | Write `ModToolTurbo.log`. |
| `Upload/AutoConfirm` | `1` | Auto-accept the three upload confirmation dialogs. |
| `Compile/SkipUnchanged` | `1` | Skip `ContentCompiler.exe` when content and cache are unchanged. |
| `Compile/Force` | `0` | Always run `ContentCompiler.exe` (useful after changing the game build). |
| `Compile/PriorityBoost` | `1` | Run the compiler above normal priority. |
| `Compile/SkipCompiler` | `0` | Skip ContentCompiler entirely and publish directly. |
| `Compile/ReuseIcons` | `0` | Pass `--dont-overwrite-icons`; reuse existing icons. |

If a file was removed or edited, the next upload compiles normally.
Deleting `%LOCALAPPDATA%\SMModToolTurbo\compile-cache.txt` also forces one
fresh compile.

`SkipCompiler=1` bypasses the compiler completely. `ReuseIcons=1` keeps the
single compiler process but passes `--dont-overwrite-icons`, so existing
stock icons are not rendered again; only missing icons are generated. Set it
back to `0` if a mod's generated icons need to be refreshed.

## How it works

- The Mod Tool imports `d3d11.dll` (only `D3D11CreateDevice` and
  `D3D11CreateDeviceAndSwapChain`). The shipped DLL forwards those two
  entry points to the real implementation and installs the hooks while the
  Mod Tool starts. Proton's `d3d11=native` override makes the app-local
  proxy load; the earlier `version.dll` approach was bypassed by Wine's
  builtin version DLL.
- **Upload dialogs:** the Mod Tool builds MyGUI message boxes through a
  virtual method at `[singleton vtable + 0x48]`. The DLL finds the singleton
  through a byte pattern (`mov reg,[rip+…]; mov rax,[reg]; mov rbx,[rax+0x48]`),
  replaces that vtable slot and invokes the dialog's "yes"
  `std::function<void()>` callback for the three upload messages. A native
  `MessageBoxW` fallback with the same texts is included.
- **Compile cache:** `CreateProcessW` is intercepted. The Mod Tool normally
  runs
  `ContentCompiler.exe --ugc="<mod>" --cache` before every upload (about
  87 s for the tested custom game, mostly engine startup and revalidation).
  The DLL fingerprints:
  - the mod files (excluding `Cache`, `Logs`, `Temp`, `preview.jpg`),
  - `ContentCompiler.exe` (size and timestamp).

  The `Cache` directory must still exist and contain files, but its exact
  file list is not compared: the Mod Tool itself prunes stale cache entries
  before every compile, so a manifest comparison would always fail on some
  mods. A missing or empty `Cache` still forces a compile.

  If both match a previous successful exit code `0`, the compiler is
  replaced by a tiny `cmd.exe /d /c exit 0` process, so the Mod Tool sees a
  successful run while the existing cache stays in place. If anything
  changed, the real compiler runs and the state is updated after a
  successful exit.

## Files

```text
dist/d3d11.dll         built proxy DLL
dist/ModToolTurbo.ini  default configuration
src/                   source code
build.sh               MinGW-w64 cross build
docs/ANALYSIS.md       reverse-engineering notes for future Mod Tool builds
install/               small install helpers
```

## Build

```sh
./build.sh
```

Requires `x86_64-w64-mingw32-g++`. The DLL is statically linked, so it has
no MinGW runtime dependencies.

## Verification

The d3d11 forwarding, native confirmation fallback, compile-skip logic and
cache-deletion invalidation were exercised with a Wine test harness. The
MyGUI vtable hook was validated against the exact byte pattern, vtable slot
and `std::function` layout of the tested `ModTool.exe`. The real Mod Tool was
launched through Proton with the proxy installed: it loaded `d3d11.dll`,
installed the `CreateProcessW`/`MessageBoxW` IAT hooks and the MyGUI
confirmation hook. The actual Workshop upload click was not triggered
end-to-end here.

## Limitations

- The Mod Tool must still run its normal validation and Workshop upload.
  This tool only removes the confirmation click and the redundant compile.
- The d3d11 proxy exports the standard D3D11 entry points and safe stubs
  for the undocumented Core* exports, so a future Mod Tool that imports a
  different subset still loads. Only `D3D11CreateDevice` and
  `D3D11CreateDeviceAndSwapChain` are fully forwarded; the Core* stubs
  return `E_NOTIMPL` if called.
- Pattern-based hooks are best-effort on future builds. Check
  `ModToolTurbo.log` after a Mod Tool update; if a feature is reported as
  unavailable, use the vanilla upload once or update the patterns in
  `src/hooks.cpp`.
- Windows may warn about a DLL that hooks a process. The full source is in
  this repository.
- Not affiliated with Axolot Games. Workshop uploads and mod content are
  your responsibility.

## License

MIT. See `LICENSE`.
