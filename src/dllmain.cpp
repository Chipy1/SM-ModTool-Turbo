#include <windows.h>
#include "config.h"
#include "log.h"
#include "compile_cache.h"
#include "hooks.h"

BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(self);
        ConfigLoad(self);
        LogInit(self);
        CompileCacheInit(self);
        InstallHooks(self);
        LogLine(L"ModTool Turbo loaded");
    } else if (reason == DLL_PROCESS_DETACH) {
        LogLine(L"ModTool Turbo unloading");
        LogClose();
    }
    return TRUE;
}
