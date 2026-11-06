#pragma once
#include <windows.h>
#include <string>

struct CompileRequest {
    std::wstring command_line;
    std::wstring compiler_path;
    std::wstring ugc_path;
    unsigned long long fingerprint = 0;
    unsigned long long compiler_sig = 0;
    bool has_fingerprint = false;
};

void CompileCacheInit(HINSTANCE self);

// True when the command line is exactly a ContentCompiler cache pass
// (ContentCompiler.exe ... --ugc="..." --cache).
bool ParseContentCompilerInvocation(const wchar_t* application_name,
                                    const wchar_t* command_line,
                                    CompileRequest& out);

// Returns true when the previous successful cache pass for this content is
// still valid; in that case the caller should launch a no-op process instead.
bool ShouldSkipCompile(CompileRequest& req);

// Watches a real ContentCompiler process and records a successful exit code.
void NoteCompileStarted(HANDLE process, const CompileRequest& req);
