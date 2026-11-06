#include "util.h"

std::wstring GetSelfPath(HINSTANCE self) {
    wchar_t buf[32768];
    DWORD n = GetModuleFileNameW(self, buf, (DWORD)(sizeof(buf) / sizeof(buf[0])));
    if (n == 0) return std::wstring();
    buf[(sizeof(buf) / sizeof(buf[0])) - 1] = 0;
    return std::wstring(buf, n);
}

std::wstring GetSelfDir(HINSTANCE self) {
    std::wstring p = GetSelfPath(self);
    size_t pos = p.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return L".";
    return p.substr(0, pos);
}

std::wstring JoinPath(const std::wstring& a, const std::wstring& b) {
    if (a.empty()) return b;
    wchar_t last = a[a.size() - 1];
    if (last == L'\\' || last == L'/') return a + b;
    return a + L"\\" + b;
}

std::wstring GetBaseName(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return path;
    return path.substr(pos + 1);
}

bool FileExists(const std::wstring& path) {
    DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool DirExists(const std::wstring& path) {
    DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

bool EqualsNoCase(const std::wstring& a, const wchar_t* b) {
    return _wcsicmp(a.c_str(), b) == 0;
}
