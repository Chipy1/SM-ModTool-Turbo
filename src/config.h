#pragma once
#include <windows.h>

struct TurboConfig {
    bool auto_confirm_upload = true;
    bool skip_unchanged_compile = true;
    bool force_compile = false;
    bool log_enabled = true;
    bool priority_boost = true;
    bool parallel_icons = true;
    int icon_jobs = 8;
};

extern TurboConfig g_cfg;

void ConfigLoad(HINSTANCE self);
