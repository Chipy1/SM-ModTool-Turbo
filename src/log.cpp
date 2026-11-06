#include "log.h"
#include "config.h"
#include "util.h"
#include <cstdarg>
#include <cstdio>

static HANDLE g_file = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION g_cs;
static bool g_csInit = false;
static bool g_logEnabled = false;

void LogInit(HINSTANCE self) {
    if (!g_cfg.log_enabled) return;
    InitializeCriticalSection(&g_cs);
    g_csInit = true;
    std::wstring path = JoinPath(GetSelfDir(self), L"ModToolTurbo.log");
    g_file = CreateFileW(path.c_str(), FILE_APPEND_DATA,
                         FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                         OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    g_logEnabled = true;
}

void LogClose() {
    if (g_file != INVALID_HANDLE_VALUE) {
        CloseHandle(g_file);
        g_file = INVALID_HANDLE_VALUE;
    }
    g_logEnabled = false;
    if (g_csInit) {
        DeleteCriticalSection(&g_cs);
        g_csInit = false;
    }
}

void LogLine(const wchar_t* fmt, ...) {
    if (!g_logEnabled || g_file == INVALID_HANDLE_VALUE) return;

    wchar_t msg[2048];
    va_list ap;
    va_start(ap, fmt);
    int n = _vsnwprintf(msg, (sizeof(msg) / sizeof(msg[0])) - 1, fmt, ap);
    va_end(ap);
    if (n < 0) n = (int)((sizeof(msg) / sizeof(msg[0])) - 1);
    msg[n] = 0;

    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t line[2304];
    int m = _snwprintf(line, (sizeof(line) / sizeof(line[0])) - 1,
                       L"[%04u-%02u-%02u %02u:%02u:%02u pid=%lu] %s\r\n",
                       st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
                       GetCurrentProcessId(), msg);
    if (m < 0) m = (int)((sizeof(line) / sizeof(line[0])) - 1);
    line[m] = 0;

    OutputDebugStringW(line);

    char utf8[8192];
    int bytes = WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8, sizeof(utf8), NULL, NULL);
    if (bytes <= 0) {
        bytes = WideCharToMultiByte(CP_ACP, 0, line, -1, utf8, sizeof(utf8), NULL, NULL);
    }
    if (bytes <= 0) return;

    if (g_csInit) EnterCriticalSection(&g_cs);
    DWORD written = 0;
    WriteFile(g_file, utf8, (DWORD)(bytes - 1), &written, NULL);
    if (g_csInit) LeaveCriticalSection(&g_cs);
}
