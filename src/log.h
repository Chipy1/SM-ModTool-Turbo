#pragma once
#include <windows.h>

void LogInit(HINSTANCE self);
void LogClose();
void LogLine(const wchar_t* fmt, ...);
