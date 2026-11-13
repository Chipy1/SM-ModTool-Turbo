#include "config.h"
#include "util.h"

TurboConfig g_cfg;

void ConfigLoad(HINSTANCE self) {
    std::wstring ini = JoinPath(GetSelfDir(self), L"ModToolTurbo.ini");
    if (!FileExists(ini)) return;

    g_cfg.auto_confirm_upload = GetPrivateProfileIntW(L"Upload", L"AutoConfirm", 1, ini.c_str()) != 0;
    g_cfg.skip_unchanged_compile = GetPrivateProfileIntW(L"Compile", L"SkipUnchanged", 1, ini.c_str()) != 0;
    g_cfg.force_compile = GetPrivateProfileIntW(L"Compile", L"Force", 0, ini.c_str()) != 0;
    g_cfg.log_enabled = GetPrivateProfileIntW(L"General", L"Log", 1, ini.c_str()) != 0;
    g_cfg.priority_boost = GetPrivateProfileIntW(L"Compile", L"PriorityBoost", 1, ini.c_str()) != 0;
    g_cfg.skip_compiler = GetPrivateProfileIntW(L"Compile", L"SkipCompiler", 0, ini.c_str()) != 0;
    g_cfg.reuse_icons = GetPrivateProfileIntW(L"Compile", L"ReuseIcons", 0, ini.c_str()) != 0;
}
