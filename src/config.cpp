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
    g_cfg.parallel_icons = GetPrivateProfileIntW(L"Compile", L"ParallelIcons", 1, ini.c_str()) != 0;
    g_cfg.icon_jobs = GetPrivateProfileIntW(L"Compile", L"IconJobs", 8, ini.c_str());
    if (g_cfg.icon_jobs < 1) g_cfg.icon_jobs = 1;
    if (g_cfg.icon_jobs > 32) g_cfg.icon_jobs = 32;
}
