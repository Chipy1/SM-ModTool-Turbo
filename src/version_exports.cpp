#include <windows.h>
#include <string>

static HMODULE g_real = NULL;
static INIT_ONCE g_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK LoadRealVersion(PINIT_ONCE, PVOID, PVOID*) {
    wchar_t sys[MAX_PATH];
    UINT n = GetSystemDirectoryW(sys, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return FALSE;
    std::wstring path = std::wstring(sys, n) + L"\\version.dll";
    g_real = LoadLibraryExW(path.c_str(), NULL, 0);
    return TRUE;
}

static FARPROC GetReal(const char* name) {
    InitOnceExecuteOnce(&g_once, LoadRealVersion, NULL, NULL);
    return g_real ? GetProcAddress(g_real, name) : NULL;
}

extern "C" {

BOOL WINAPI GetFileVersionInfoA(LPCSTR filename, DWORD handle, DWORD len, LPVOID data) {
    typedef BOOL (WINAPI *F)(LPCSTR, DWORD, DWORD, LPVOID);
    static F f = (F)GetReal("GetFileVersionInfoA");
    return f ? f(filename, handle, len, data) : FALSE;
}

BOOL WINAPI GetFileVersionInfoW(LPCWSTR filename, DWORD handle, DWORD len, LPVOID data) {
    typedef BOOL (WINAPI *F)(LPCWSTR, DWORD, DWORD, LPVOID);
    static F f = (F)GetReal("GetFileVersionInfoW");
    return f ? f(filename, handle, len, data) : FALSE;
}

BOOL WINAPI GetFileVersionInfoExA(DWORD flags, LPCSTR filename, DWORD handle, DWORD len, LPVOID data) {
    typedef BOOL (WINAPI *F)(DWORD, LPCSTR, DWORD, DWORD, LPVOID);
    static F f = (F)GetReal("GetFileVersionInfoExA");
    return f ? f(flags, filename, handle, len, data) : FALSE;
}

BOOL WINAPI GetFileVersionInfoExW(DWORD flags, LPCWSTR filename, DWORD handle, DWORD len, LPVOID data) {
    typedef BOOL (WINAPI *F)(DWORD, LPCWSTR, DWORD, DWORD, LPVOID);
    static F f = (F)GetReal("GetFileVersionInfoExW");
    return f ? f(flags, filename, handle, len, data) : FALSE;
}

DWORD WINAPI GetFileVersionInfoSizeA(LPCSTR filename, LPDWORD handle) {
    typedef DWORD (WINAPI *F)(LPCSTR, LPDWORD);
    static F f = (F)GetReal("GetFileVersionInfoSizeA");
    return f ? f(filename, handle) : 0;
}

DWORD WINAPI GetFileVersionInfoSizeW(LPCWSTR filename, LPDWORD handle) {
    typedef DWORD (WINAPI *F)(LPCWSTR, LPDWORD);
    static F f = (F)GetReal("GetFileVersionInfoSizeW");
    return f ? f(filename, handle) : 0;
}

DWORD WINAPI GetFileVersionInfoSizeExA(DWORD flags, LPCSTR filename, LPDWORD handle) {
    typedef DWORD (WINAPI *F)(DWORD, LPCSTR, LPDWORD);
    static F f = (F)GetReal("GetFileVersionInfoSizeExA");
    return f ? f(flags, filename, handle) : 0;
}

DWORD WINAPI GetFileVersionInfoSizeExW(DWORD flags, LPCWSTR filename, LPDWORD handle) {
    typedef DWORD (WINAPI *F)(DWORD, LPCWSTR, LPDWORD);
    static F f = (F)GetReal("GetFileVersionInfoSizeExW");
    return f ? f(flags, filename, handle) : 0;
}

DWORD WINAPI VerFindFileA(DWORD flags, LPSTR filename, LPSTR winDir, LPSTR appDir,
                          LPSTR curDir, PUINT curDirLen, LPSTR destDir, PUINT destDirLen) {
    typedef DWORD (WINAPI *F)(DWORD, LPSTR, LPSTR, LPSTR, LPSTR, PUINT, LPSTR, PUINT);
    static F f = (F)GetReal("VerFindFileA");
    return f ? f(flags, filename, winDir, appDir, curDir, curDirLen, destDir, destDirLen) : 0;
}

DWORD WINAPI VerFindFileW(DWORD flags, LPWSTR filename, LPWSTR winDir, LPWSTR appDir,
                          LPWSTR curDir, PUINT curDirLen, LPWSTR destDir, PUINT destDirLen) {
    typedef DWORD (WINAPI *F)(DWORD, LPWSTR, LPWSTR, LPWSTR, LPWSTR, PUINT, LPWSTR, PUINT);
    static F f = (F)GetReal("VerFindFileW");
    return f ? f(flags, filename, winDir, appDir, curDir, curDirLen, destDir, destDirLen) : 0;
}

DWORD WINAPI VerInstallFileA(DWORD flags, LPSTR srcFileName, LPSTR destFileName,
                             LPSTR srcDir, LPSTR destDir, LPSTR curDir, LPSTR tmpFile, PUINT tmpFileLen) {
    typedef DWORD (WINAPI *F)(DWORD, LPSTR, LPSTR, LPSTR, LPSTR, LPSTR, LPSTR, PUINT);
    static F f = (F)GetReal("VerInstallFileA");
    return f ? f(flags, srcFileName, destFileName, srcDir, destDir, curDir, tmpFile, tmpFileLen) : 0;
}

DWORD WINAPI VerInstallFileW(DWORD flags, LPWSTR srcFileName, LPWSTR destFileName,
                             LPWSTR srcDir, LPWSTR destDir, LPWSTR curDir, LPWSTR tmpFile, PUINT tmpFileLen) {
    typedef DWORD (WINAPI *F)(DWORD, LPWSTR, LPWSTR, LPWSTR, LPWSTR, LPWSTR, LPWSTR, PUINT);
    static F f = (F)GetReal("VerInstallFileW");
    return f ? f(flags, srcFileName, destFileName, srcDir, destDir, curDir, tmpFile, tmpFileLen) : 0;
}

DWORD WINAPI VerLanguageNameA(DWORD lang, LPSTR name, DWORD size) {
    typedef DWORD (WINAPI *F)(DWORD, LPSTR, DWORD);
    static F f = (F)GetReal("VerLanguageNameA");
    return f ? f(lang, name, size) : 0;
}

DWORD WINAPI VerLanguageNameW(DWORD lang, LPWSTR name, DWORD size) {
    typedef DWORD (WINAPI *F)(DWORD, LPWSTR, DWORD);
    static F f = (F)GetReal("VerLanguageNameW");
    return f ? f(lang, name, size) : 0;
}

BOOL WINAPI VerQueryValueA(LPCVOID block, LPCSTR subBlock, LPVOID* buffer, PUINT len) {
    typedef BOOL (WINAPI *F)(LPCVOID, LPCSTR, LPVOID*, PUINT);
    static F f = (F)GetReal("VerQueryValueA");
    return f ? f(block, subBlock, buffer, len) : FALSE;
}

BOOL WINAPI VerQueryValueW(LPCVOID block, LPCWSTR subBlock, LPVOID* buffer, PUINT len) {
    typedef BOOL (WINAPI *F)(LPCVOID, LPCWSTR, LPVOID*, PUINT);
    static F f = (F)GetReal("VerQueryValueW");
    return f ? f(block, subBlock, buffer, len) : FALSE;
}

} // extern "C"
