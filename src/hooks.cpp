#include "hooks.h"
#include "compile_cache.h"
#include "config.h"
#include "log.h"
#include "util.h"
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>

typedef BOOL (WINAPI *CreateProcessW_t)(
    LPCWSTR, LPWSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD,
    LPVOID, LPCWSTR, LPSTARTUPINFOW, LPPROCESS_INFORMATION);
typedef BOOL (WINAPI *CreateProcessA_t)(
    LPCSTR, LPSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD,
    LPVOID, LPCSTR, LPSTARTUPINFOA, LPPROCESS_INFORMATION);
typedef int (WINAPI *MessageBoxW_t)(HWND, LPCWSTR, LPCWSTR, UINT);
typedef int (WINAPI *MessageBoxA_t)(HWND, LPCSTR, LPCSTR, UINT);

static CreateProcessW_t RealCreateProcessW = NULL;
static CreateProcessA_t RealCreateProcessA = NULL;
static MessageBoxW_t RealMessageBoxW = NULL;
static MessageBoxA_t RealMessageBoxA = NULL;

static bool PatchIAT(HMODULE module, const char* function, void* replacement, void** original) {
    if (!module) return false;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)module;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)((BYTE*)module + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

    DWORD importRva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (importRva == 0) return false;

    IMAGE_IMPORT_DESCRIPTOR* desc = (IMAGE_IMPORT_DESCRIPTOR*)((BYTE*)module + importRva);
    bool patched = false;
    for (; desc->Name != 0; ++desc) {
        DWORD thunkRva = desc->OriginalFirstThunk ? desc->OriginalFirstThunk : desc->FirstThunk;
        IMAGE_THUNK_DATA* thunk = (IMAGE_THUNK_DATA*)((BYTE*)module + thunkRva);
        IMAGE_THUNK_DATA* iat = (IMAGE_THUNK_DATA*)((BYTE*)module + desc->FirstThunk);
        for (; thunk->u1.AddressOfData != 0; ++thunk, ++iat) {
            if (IMAGE_SNAP_BY_ORDINAL(thunk->u1.Ordinal)) continue;
            IMAGE_IMPORT_BY_NAME* name = (IMAGE_IMPORT_BY_NAME*)((BYTE*)module + thunk->u1.AddressOfData);
            if (_stricmp((const char*)name->Name, function) != 0) continue;

            if (*original == NULL) {
                *original = (void*)iat->u1.Function;
            }
            DWORD oldProtect = 0;
            if (VirtualProtect(&iat->u1.Function, sizeof(void*), PAGE_READWRITE, &oldProtect)) {
                iat->u1.Function = (ULONGLONG)replacement;
                DWORD ignored = 0;
                VirtualProtect(&iat->u1.Function, sizeof(void*), oldProtect, &ignored);
                patched = true;
            }
        }
    }
    return patched;
}

static int AffirmativeResult(UINT type) {
    switch (type & 0x0000000F) {
        case MB_OK:
        case MB_OKCANCEL:
            return IDOK;
        case MB_YESNO:
        case MB_YESNOCANCEL:
            return IDYES;
        default:
            return IDYES;
    }
}

static bool IsUploadConfirmation(const wchar_t* text) {
    if (!text) return false;
    return wcsstr(text, L"Are you sure you want to upload anyway?") != NULL ||
           wcsstr(text, L"This will cause the mod tool to overwrite the previously uploaded data.") != NULL ||
           wcsstr(text, L"Your mod contain errors and may not function correctly.") != NULL;
}

static int WINAPI HookedMessageBoxW(HWND hwnd, LPCWSTR text, LPCWSTR caption, UINT type) {
    if (g_cfg.auto_confirm_upload && IsUploadConfirmation(text)) {
        int result = AffirmativeResult(type);
        LogLine(L"auto-confirmed upload dialog: %s", text ? text : L"");
        return result;
    }
    if (RealMessageBoxW) return RealMessageBoxW(hwnd, text, caption, type);
    return 0;
}

static int WINAPI HookedMessageBoxA(HWND hwnd, LPCSTR text, LPCSTR caption, UINT type) {
    if (g_cfg.auto_confirm_upload && text) {
        int len = MultiByteToWideChar(CP_ACP, 0, text, -1, NULL, 0);
        if (len > 0) {
            std::vector<wchar_t> wide((size_t)len);
            MultiByteToWideChar(CP_ACP, 0, text, -1, wide.data(), len);
            if (IsUploadConfirmation(wide.data())) {
                LogLine(L"auto-confirmed upload dialog (ANSI): %s", wide.data());
                return AffirmativeResult(type);
            }
        }
    }
    if (RealMessageBoxA) return RealMessageBoxA(hwnd, text, caption, type);
    return 0;
}

static std::wstring GetCommandInterpreter() {
    wchar_t buf[32768];
    DWORD n = GetEnvironmentVariableW(L"COMSPEC", buf, (DWORD)(sizeof(buf) / sizeof(buf[0])));
    if (n > 0 && n < (DWORD)(sizeof(buf) / sizeof(buf[0]))) {
        std::wstring p(buf, n);
        if (FileExists(p)) return p;
    }
    wchar_t sys[MAX_PATH];
    UINT m = GetSystemDirectoryW(sys, MAX_PATH);
    if (m > 0 && m < MAX_PATH) {
        return JoinPath(std::wstring(sys, m), L"cmd.exe");
    }
    return L"C:\\Windows\\System32\\cmd.exe";
}

static HMODULE g_selfModule = NULL;

static BOOL WINAPI HookedCreateProcessW(
    LPCWSTR applicationName, LPWSTR commandLine,
    LPSECURITY_ATTRIBUTES processAttributes, LPSECURITY_ATTRIBUTES threadAttributes,
    BOOL inheritHandles, DWORD creationFlags, LPVOID environment,
    LPCWSTR currentDirectory, LPSTARTUPINFOW startupInfo, LPPROCESS_INFORMATION processInformation) {

    if (!RealCreateProcessW) {
        RealCreateProcessW = (CreateProcessW_t)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "CreateProcessW");
        if (!RealCreateProcessW) return FALSE;
    }

    CompileRequest req;
    if (ParseContentCompilerInvocation(applicationName, commandLine, req)) {
        if (ShouldSkipCompile(req)) {
            std::wstring interpreter = GetCommandInterpreter();
            std::wstring alt = L"\"" + interpreter + L"\" /d /c exit 0";
            std::vector<wchar_t> altLine(alt.begin(), alt.end());
            altLine.push_back(0);

            DWORD flags = (creationFlags | CREATE_NO_WINDOW) & ~CREATE_NEW_CONSOLE;
            LogLine(L"launching no-op instead of content compiler: %s", commandLine);
            BOOL ok = RealCreateProcessW(interpreter.c_str(), altLine.data(),
                                         processAttributes, threadAttributes, inheritHandles,
                                         flags, environment, currentDirectory, startupInfo,
                                         processInformation);
            if (!ok) {
                LogLine(L"no-op compiler launch failed: %lu", GetLastError());
            }
            return ok;
        }

        if (g_cfg.parallel_icons && g_selfModule) {
            std::wstring helper = JoinPath(GetSelfDir(g_selfModule), L"ContentCompilerTurbo.exe");
            if (FileExists(helper)) {
                std::wstring hc = L"\"" + helper + L"\" --compiler=\"" + req.compiler_path +
                                  L"\" --ugc=\"" + req.ugc_path + L"\"";
                if (wcsstr(req.command_line.c_str(), L"--dont-overwrite-icons")) {
                    hc += L" --dont-overwrite-icons";
                }
                hc += L" --jobs=" + std::to_wstring(g_cfg.icon_jobs);
                std::vector<wchar_t> hb(hc.begin(), hc.end());
                hb.push_back(0);
                DWORD hflags = (creationFlags | CREATE_NO_WINDOW) & ~CREATE_NEW_CONSOLE;
                LogLine(L"launching parallel icon helper: %s", hc.c_str());
                BOOL hok = RealCreateProcessW(helper.c_str(), hb.data(),
                                              processAttributes, threadAttributes, inheritHandles,
                                              hflags, environment, currentDirectory, startupInfo,
                                              processInformation);
                if (hok) {
                    if (processInformation && processInformation->hProcess) {
                        if (g_cfg.priority_boost) {
                            SetPriorityClass(processInformation->hProcess, ABOVE_NORMAL_PRIORITY_CLASS);
                        }
                        NoteCompileStarted(processInformation->hProcess, req);
                    }
                    return hok;
                }
                LogLine(L"parallel icon helper failed to start: %lu; falling back", GetLastError());
            }
        }

        BOOL ok = RealCreateProcessW(applicationName, commandLine,
                                     processAttributes, threadAttributes, inheritHandles,
                                     creationFlags, environment, currentDirectory, startupInfo,
                                     processInformation);
        if (ok && processInformation && processInformation->hProcess) {
            if (g_cfg.priority_boost) {
                SetPriorityClass(processInformation->hProcess, ABOVE_NORMAL_PRIORITY_CLASS);
            }
            NoteCompileStarted(processInformation->hProcess, req);
        }
        return ok;
    }

    return RealCreateProcessW(applicationName, commandLine,
                              processAttributes, threadAttributes, inheritHandles,
                              creationFlags, environment, currentDirectory, startupInfo,
                              processInformation);
}

static BOOL WINAPI HookedCreateProcessA(
    LPCSTR applicationName, LPSTR commandLine,
    LPSECURITY_ATTRIBUTES processAttributes, LPSECURITY_ATTRIBUTES threadAttributes,
    BOOL inheritHandles, DWORD creationFlags, LPVOID environment,
    LPCSTR currentDirectory, LPSTARTUPINFOA startupInfo, LPPROCESS_INFORMATION processInformation) {
    if (RealCreateProcessA) {
        return RealCreateProcessA(applicationName, commandLine, processAttributes, threadAttributes,
                                  inheritHandles, creationFlags, environment, currentDirectory,
                                  startupInfo, processInformation);
    }
    return FALSE;
}


// ---------------------------------------------------------------------------
// MyGUI upload confirmations
//
// The Mod Tool asks for confirmation through a virtual method at
// [singleton vtable + 0x48].  The method receives the message body and two
// std::function<void()> callbacks (yes, no).  We replace the vtable slot and
// invoke the "yes" callback for the three upload confirmations.  The slot is
// found by scanning the executable for the singleton load sequence
//     mov reg, [rip+disp] ; mov rax, [reg] ; mov rbx, [rax+0x48]
// so no file offsets or build-specific addresses are hardcoded.
// ---------------------------------------------------------------------------

typedef void (*ShowMessageBox_t)(void* self, void* title, void* message, void* yes, void* no);
static ShowMessageBox_t RealShowMessageBox = NULL;

static bool IsExecutableAddress(const void* ptr) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(ptr, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT) return false;
    DWORD p = mbi.Protect;
    return (p & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
}

static bool IsReadableAddress(const void* ptr) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(ptr, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT) return false;
    return (mbi.Protect & PAGE_NOACCESS) == 0;
}

static void** FindMessageBoxGlobal(HMODULE module) {
    if (!module) return NULL;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)module;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return NULL;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)((BYTE*)module + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return NULL;

    IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        if (!(sec->Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        const unsigned char* p = (const unsigned char*)module + sec->VirtualAddress;
        size_t n = sec->Misc.VirtualSize;
        if (n < 16) continue;
        for (size_t j = 10; j + 4 <= n; ++j) {
            if (p[j] != 0x48 || p[j + 1] != 0x8B || p[j + 2] != 0x58 || p[j + 3] != 0x48) continue;
            if (p[j - 10] != 0x48 || p[j - 9] != 0x8B || p[j - 8] != 0x3D) continue;
            if (p[j - 3] != 0x48 || p[j - 2] != 0x8B || p[j - 1] != 0x07) continue;
            int32_t disp = 0;
            memcpy(&disp, p + j - 7, sizeof(disp));
            uintptr_t next = (uintptr_t)(p + j - 3);
            uintptr_t global = next + (intptr_t)disp;
            if (global < (uintptr_t)module || global + sizeof(void*) > (uintptr_t)module + nt->OptionalHeader.SizeOfImage) continue;
            if (!IsReadableAddress((void*)global)) continue;
            return (void**)global;
        }
    }
    return NULL;
}

static bool ReadMsvcString(const void* object, std::string& out) {
    if (!object || !IsReadableAddress(object)) return false;
    const unsigned char* p = (const unsigned char*)object;
    size_t size = 0;
    memcpy(&size, p + 16, sizeof(size));
    if (size > (size_t)(1 << 20)) return false;
    if (size < 16) {
        out.assign((const char*)p, size);
        return true;
    }
    const char* data = NULL;
    memcpy(&data, p, sizeof(data));
    if (!data || !IsReadableAddress(data)) return false;
    out.assign(data, size);
    return true;
}

static bool IsMyGuiUploadConfirmation(const std::string& text) {
    return text.find("Your mod contain errors and may not function correctly.") != std::string::npos ||
           text.find("This will cause the mod tool to overwrite the previously uploaded data.") != std::string::npos ||
           text.find("Are you sure you want to upload anyway?") != std::string::npos ||
           text.find("Your mod is out of date. Would you like to upgrade?") != std::string::npos;
}

static bool InvokeVoidCallback(void* callback) {
    if (!callback || !IsReadableAddress(callback)) return false;
    void** object = (void**)callback;
    void** vtable = (void**)*object;
    if (!vtable || !IsReadableAddress(vtable)) return false;
    // MSVC std::_Func_impl_no_alloc<void>: [0]=~, [1]=~, [2]=operator(), [4]=_Delete_this.
    if (vtable[0] != vtable[1]) return false;
    if (!vtable[2] || !IsExecutableAddress(vtable[2])) return false;
    typedef void (*Invoke)(void*);
    Invoke fn = (Invoke)vtable[2];
    fn(callback);
    return true;
}

static void HookedShowMessageBox(void* self, void* title, void* message, void* yes, void* no) {
    if (g_cfg.auto_confirm_upload && message) {
        std::string text;
        if (ReadMsvcString(message, text) && IsMyGuiUploadConfirmation(text)) {
            if (InvokeVoidCallback(yes)) {
                LogLine(L"auto-confirmed MyGUI upload dialog");
                return;
            }
            LogLine(L"upload dialog matched but callback layout was unexpected; showing dialog");
        }
    }
    if (RealShowMessageBox) {
        RealShowMessageBox(self, title, message, yes, no);
    }
}

static bool IsModToolProcess() {
    wchar_t path[32768];
    DWORD n = GetModuleFileNameW(NULL, path, (DWORD)(sizeof(path) / sizeof(path[0])));
    if (n == 0) return false;
    std::wstring name = path;
    size_t pos = name.find_last_of(L"\\/");
    if (pos != std::wstring::npos) name = name.substr(pos + 1);
    for (size_t i = 0; i < name.size(); ++i) name[i] = (wchar_t)towlower(name[i]);
    return name.find(L"modtool") != std::wstring::npos;
}

static DWORD WINAPI ConfirmationHookThread(LPVOID) {
    if (!g_cfg.auto_confirm_upload) return 0;
    if (!IsModToolProcess()) {
        LogLine(L"host is not ModTool; upload confirmation hook skipped");
        return 0;
    }
    HMODULE module = GetModuleHandleW(NULL);
    void** slot = FindMessageBoxGlobal(module);
    if (!slot) {
        LogLine(L"upload confirmation hook: singleton pattern not found");
        return 0;
    }
    for (int i = 0; i < 6000; ++i) {
        void* singleton = *slot;
        if (singleton) {
            void** vtable = (void**)*(void**)singleton;
            if (vtable && IsReadableAddress(vtable) && IsExecutableAddress(vtable[0x48 / sizeof(void*)])) {
                RealShowMessageBox = (ShowMessageBox_t)vtable[0x48 / sizeof(void*)];
                DWORD oldProtect = 0;
                if (VirtualProtect(&vtable[0x48 / sizeof(void*)], sizeof(void*),
                                   PAGE_EXECUTE_READWRITE, &oldProtect)) {
                    vtable[0x48 / sizeof(void*)] = (void*)HookedShowMessageBox;
                    DWORD ignored = 0;
                    VirtualProtect(&vtable[0x48 / sizeof(void*)], sizeof(void*), oldProtect, &ignored);
                    LogLine(L"upload confirmation hook installed");
                    return 0;
                }
            }
        }
        Sleep(100);
    }
    LogLine(L"upload confirmation hook timed out waiting for the UI singleton (10 min)");
    return 0;
}

void InstallHooks(HINSTANCE self) {
    g_selfModule = (HMODULE)self;
    HMODULE module = GetModuleHandleW(NULL);
    if (!module) return;

    if (!PatchIAT(module, "CreateProcessW", (void*)HookedCreateProcessW, (void**)&RealCreateProcessW)) {
        LogLine(L"CreateProcessW import not found; compile cache disabled");
    }
    if (!PatchIAT(module, "CreateProcessA", (void*)HookedCreateProcessA, (void**)&RealCreateProcessA)) {
        LogLine(L"CreateProcessA import not found");
    }
    if (!PatchIAT(module, "MessageBoxW", (void*)HookedMessageBoxW, (void**)&RealMessageBoxW)) {
        LogLine(L"MessageBoxW import not found");
    }
    if (!PatchIAT(module, "MessageBoxA", (void*)HookedMessageBoxA, (void**)&RealMessageBoxA)) {
        LogLine(L"MessageBoxA import not found");
    }
    LogLine(L"IAT hooks installed");
    HANDLE th = CreateThread(NULL, 0, ConfirmationHookThread, NULL, 0, NULL);
    if (th) CloseHandle(th);
}
