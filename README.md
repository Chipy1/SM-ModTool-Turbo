# SM ModTool Turbo

A drop-in helper for the **Scrap Mechanic Mod Tool** that:

- **Auto-confirms Workshop upload warnings** — the three "are you sure?"
  dialogs (content errors, out-of-date item, overwrite existing item) no
  longer stop an upload.
- **Skips redundant content compilation** — if the mod content, the
  `ContentCompiler.exe` build and the generated `Cache` are unchanged since
  the last successful compile, the compiler process is skipped. The first
  run is unchanged; later uploads of the same content are much faster.
- Leaves `ModTool.exe`, the game and all mod files untouched. It is a
  `version.dll` proxy loaded by the Mod Tool itself.

## Tested build

- `ModTool.exe` SHA-256:
  `853dc33c1a6ee300f4152cfc76564c71928efdc0d34fc7bcd4b3e85cbaa9df4e`
- Scrap Mechanic 1.0.6.889 (the Mod Tool shipped with that build).
- `dist/version.dll` (v1.0.0) SHA-256:
  `9b90908e25c0b0a360173ff7579c010e1e69bf74ab5eaf1466966d2735916b88`

The tool is deliberately **not tied to fixed addresses**. It uses the
Mod Tool's import table and byte patterns, so a new Mod Tool build will
either keep working or disable the affected feature and write a warning to
`ModToolTurbo.log`. It will not crash on an unknown build.

## Install

1. Close the Scrap Mechanic Mod Tool.
2. Copy these two files into the same folder as `ModTool.exe`
   (normally `...\Scrap Mechanic Mod Tool\Release`):
   - `version.dll`
   - `ModToolTurbo.ini`
3. Start the Mod Tool. Check `ModToolTurbo.log` in the same folder.

To uninstall, delete both files. Do not leave a stale `version.dll` from
another tool in that folder; back it up first if one already exists.

### Linux / Proton

The Mod Tool runs through Proton. Install the files into the Windows-side
Mod Tool folder (the folder that contains `ModTool.exe`), for example:

```text
.../steamapps/common/Scrap Mechanic Mod Tool/Release
```

The proxy runs inside the Proton prefix and loads the real `version.dll`
from the prefix's `C:\Windows\System32`. The compile cache state is stored
under the prefix's `%LOCALAPPDATA%\SMModToolTurbo`.

## Configuration

`ModToolTurbo.ini` next to `version.dll`:

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

If a file was removed or edited, the next upload compiles normally.
Deleting `%LOCALAPPDATA%\SMModToolTurbo\compile-cache.txt` also forces one
fresh compile.

## How it works

- The Mod Tool imports `version.dll`. The shipped DLL forwards every
  `version.dll` export to the real system DLL and installs two hooks while
  the Mod Tool starts.
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
  - `ContentCompiler.exe` (size and timestamp),
  - the generated `Cache` directory (file list, sizes and timestamps).

  If all three match a previous successful exit code `0`, the compiler is
  replaced by a tiny `cmd.exe /d /c exit 0` process, so the Mod Tool sees a
  successful run while the existing cache stays in place. If anything
  changed, the real compiler runs and the state is updated after a
  successful exit.

## Files

```text
dist/version.dll       built proxy DLL
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

The proxy, version forwarding, native confirmation fallback, compile-skip
logic and cache invalidation were exercised with a Wine test harness. The
MyGUI vtable hook was validated against the exact byte pattern, vtable slot
and `std::function` layout of the tested `ModTool.exe` using a synthetic
harness. The real upload dialog flow needs a Steam/Windows client, so it was
not triggered end-to-end here.

## Limitations

- The Mod Tool must still run its normal validation and Workshop upload.
  This tool only removes the confirmation click and the redundant compile.
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
