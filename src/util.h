#pragma once
#include <windows.h>
#include <string>

std::wstring GetSelfPath(HINSTANCE self);
std::wstring GetSelfDir(HINSTANCE self);
std::wstring JoinPath(const std::wstring& a, const std::wstring& b);
std::wstring GetBaseName(const std::wstring& path);
bool FileExists(const std::wstring& path);
bool DirExists(const std::wstring& path);
bool EqualsNoCase(const std::wstring& a, const wchar_t* b);
