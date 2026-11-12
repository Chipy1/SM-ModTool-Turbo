# Reverse-engineering notes

These notes are for maintaining the pattern-based hooks after a Mod Tool
update. They describe the tested build only; the DLL does **not** use these
absolute addresses at runtime.

Tested binary:

- `ModTool.exe` SHA-256
  `853dc33c1a6ee300f4152cfc76564c71928efdc0d34fc7bcd4b3e85cbaa9df4e`
- PE32+ x86-64, MSVC 19.42, image base `0x140000000`.

## Host DLL

The tested Mod Tool imports only two D3D11 entry points
(`D3D11CreateDevice`, `D3D11CreateDeviceAndSwapChain`). Proton sets
`WINEDLLOVERRIDES=d3d11=native`, so an app-local `d3d11.dll` is loaded.
The DLL forwards those two functions to the real D3D11 implementation and
installs the hooks from `DllMain`.

The initial `version.dll` proxy (v1.0.0) did **not** load under Proton:
Wine reported the app-local `VERSION.dll` as `builtin` and used its own
implementation. `d3d11.dll` is the reliable host on both Windows and Proton.

The proxy exports:

- `D3D11CreateDevice`, `D3D11CreateDeviceAndSwapChain` — fully forwarded.
- `D3D11On12CreateDevice` — forwarded if present.
- `D3D11CoreCreateDevice`, `D3D11CoreCreateLayeredDevice`,
  `D3D11CoreGetLayeredDeviceSize`, `D3D11CoreRegisterLayers` — stubs so a
  future Mod Tool that imports them still resolves the proxy. They return
  `E_NOTIMPL`/`0` if called.

## Upload flow

| Address | Role |
| --- | --- |
| `0x1401bd590` | Upload start; calls validation, then shows the "mod contains errors" dialog or continues. |
| `0x1401bde20` | Content validation. Returns `1` when the content is valid, `0` when the errors dialog is needed. |
| `0x1401bdb50` | Version check; shows "out of date" or calls the overwrite dialog. |
| `0x1401bd860` | Overwrite confirmation. |
| `0x1401bad90` | Builds the `ContentCompiler.exe --ugc="…" --cache` command line and runs it. |
| `0x1401bb3c0` | Checks `GetExitCodeProcess` and decides whether the compile succeeded. |
| `0x1401bffb0` | The overwrite dialog's "yes" continuation (the upload). |

### Dialog singleton and vtable

The three dialogs call a virtual method at `[singleton vtable + 0x48]`.
The singleton pointer is a `DAT_140e343f0`-style BSS global that is
initialized by a static constructor. The call site pattern is:

```text
48 8B 3D <disp32>   mov rdi, [rip+disp]   ; global singleton pointer
48 8B 07            mov rax, [rdi]        ; object
48 8B 58 48         mov rbx, [rax+0x48]   ; show message box method
```

The method signature is:

```c
void show_message_box(void *self, std::string *title, std::string *message,
                      std::function<void()> *yes, std::function<void()> *no);
```

MSVC `std::string` layout: data pointer at `+0x00` (or inline buffer),
size at `+0x10`, capacity at `+0x18`.

MSVC `std::_Func_impl_no_alloc<lambda, void>` layout:

```text
+0x00 vtable
+0x08 captured pointer
vtable +0x00/+0x08  deleting destructor
vtable +0x10        operator()
vtable +0x20        _Delete_this
vtable +0x28        _Get_function_impl
```

Verified callback vtables:

| Dialog | `yes` vtable | `operator()` |
| --- | --- | --- |
| mod contains errors | `0x140b4d520` | `0x1401c5d40` → `FUN_1401bdb50` |
| out of date | `0x140b4d558` | `0x1401c5d80` |
| overwrite | `0x140b4d590` | `0x1401c5eb0` → `FUN_1401bffb0` |
| "no" callback | `0x140b4d670` | `0x1401a11a0` (`ret 0`) |

The DLL only relies on `vtable[0] == vtable[1]`, `vtable[2]` being
executable, and calls `vtable[2]` — it does not hardcode any of the
addresses above.

## Content compiler

Observed command line (from the Mod Tool log):

```text
<game>/Release/ContentCompiler.exe --ugc="<mod dir>" --cache
```

It is a full engine process. For the tested custom game it took
`21:52:07` → `21:53:34` (≈87 s), mostly loading the game data library and
revalidating caches, even when only a handful of files were stale.

The Mod Tool checks the compiler process exit code and then proceeds with
the Workshop upload. The DLL replaces the process with
`cmd.exe /d /c exit 0` only when the content and compiler binary match a
previously successful run and the `Cache` directory still exists with at
least one file. The state file is
`%LOCALAPPDATA%\SMModToolTurbo\compile-cache.txt`.

Do **not** add the exact `Cache` file list to the skip decision. The Mod
Tool prunes stale cache entries before each compiler run (observed: the same
five `.mco`/`.tco` files for the tested custom game). A manifest comparison
would therefore never match. The content fingerprint is the source of truth;
the cache is a local, regenerable artifact.

## Rebuilding the patterns for a new Mod Tool

1. Find the sequence
   `48 8B 3D ?? ?? ?? ?? 48 8B 07 48 8B 58 48` in the new `.text`.
   The `mov rbx,[rax+0x48]` part is the key; the global displacement is
   computed at runtime.
2. Confirm the singleton points to a vtable whose `+0x48` function is the
   five-argument message-box method.
3. Confirm the three message strings still exist:
   - `Your mod contain errors and may not function correctly.`
   - `This will cause the mod tool to overwrite the previously uploaded data.`
   - `Your mod is out of date. Would you like to upgrade?`
4. Confirm the callback layout with a small test: the "yes" object's
   vtable `+0x10` is the continuation, and `vtable[0] == vtable[1]`.
5. For the compile cache, confirm the `CreateProcessW` command line still
   contains `ContentCompiler.exe`, `--ugc="…"` and `--cache`.

If a pattern is missing, the affected feature disables itself and logs a
warning; it does not patch random code.


## ContentCompiler icon parallelism

The generic task manager starts `hardware_concurrency` workers (24 on the
tested 24-thread CPU), but the icon renderer runs on the main thread. In the
tested `--cache` log the "Generating IconMap" phase from `23:00:06` to
`23:02:19` has 2294 main-thread log lines and only ~30-40 lines on each
worker; the workers are mostly loading raw caches. Icon rendering is serial.

The ContentCompiler CLI exposes independent icon category flags:

```text
--asset --harvestable --node --voxelmesh --prefab --blueprint
--customization --shape
```

`--cache` is the umbrella pass; `--create-atlas` builds the icon atlas;
`--dont-overwrite-icons` is a global consumed by the icon process functions
(checked at `DAT_1410cd7c0 + 0x28` in the tested build). `ContentCompilerTurbo.exe`
runs each category flag in its own process, then a final
`--cache --dont-overwrite-icons` pass. This gives each category its own main
thread instead of serializing all of them on one. The helper staggers starts
by ~1.1 s because the engine log filename has one-second resolution.

If a category process fails, the helper runs the final cache pass **without**
`--dont-overwrite-icons`, so the stock path regenerates any missing icons.
If a future build removes a category flag, that process fails fast and the
final cache pass still produces the complete result.
