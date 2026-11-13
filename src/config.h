#pragma once
#include <windows.h>

struct TurboConfig {
    bool auto_confirm_upload = true;
    bool skip_unchanged_compile = true;
    bool force_compile = false;
    bool log_enabled = true;
    bool priority_boost = true;
    bool skip_compiler = false;
    bool reuse_icons = false;
};

extern TurboConfig g_cfg;

void ConfigLoad(HINSTANCE self);
