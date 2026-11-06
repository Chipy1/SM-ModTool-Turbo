#include "compile_cache.h"
#include "config.h"
#include "log.h"
#include "util.h"
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cwctype>

namespace {

const unsigned long long kFnvOffset = 1469598103934665603ULL;
const unsigned long long kFnvPrime = 1099511628211ULL;

CRITICAL_SECTION g_state_cs;
bool g_state_cs_init = false;
std::wstring g_state_path;

struct FileEntry {
    std::wstring rel;
    std::wstring full;
    unsigned long long size;
};

unsigned long long HashBytes(unsigned long long h, const void* data, size_t len) {
    const unsigned char* p = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < len; ++i) {
        h ^= (unsigned long long)p[i];
        h *= kFnvPrime;
    }
    return h;
}

std::wstring LowerCopy(const std::wstring& s) {
    std::wstring r = s;
    if (!r.empty()) {
        CharLowerBuffW(&r[0], (DWORD)r.size());
    }
    return r;
}

std::wstring NormalizePath(const std::wstring& path) {
    std::wstring p = path;
    for (size_t i = 0; i < p.size(); ++i) {
        if (p[i] == L'/') p[i] = L'\\';
    }
    while (p.size() > 1 && p[p.size() - 1] == L'\\') p.resize(p.size() - 1);
    return LowerCopy(p);
}

unsigned long long HashPath(const std::wstring& path) {
    std::wstring p = NormalizePath(path);
    unsigned long long h = HashBytes(kFnvOffset, "MTT1PATH", 8);
    h = HashBytes(h, p.data(), p.size() * sizeof(wchar_t));
    return h;
}

bool IsSkippedDir(const std::wstring& name) {
    return EqualsNoCase(name, L"Cache") || EqualsNoCase(name, L"Logs") ||
           EqualsNoCase(name, L"Temp") || EqualsNoCase(name, L".git") ||
           EqualsNoCase(name, L".svn");
}

bool CollectFiles(const std::wstring& root, const std::wstring& rel,
                  std::vector<FileEntry>& out, int depth) {
    if (depth > 64) return false;
    std::wstring pattern = JoinPath(JoinPath(root, rel), L"*");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD e = GetLastError();
        return e == ERROR_FILE_NOT_FOUND || e == ERROR_NO_MORE_FILES;
    }
    bool ok = true;
    do {
        std::wstring name = fd.cFileName;
        if (name == L"." || name == L"..") continue;

        std::wstring childRel = rel.empty() ? name : rel + L"\\" + name;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
            if (IsSkippedDir(name)) continue;
            if (!CollectFiles(root, childRel, out, depth + 1)) {
                ok = false;
                break;
            }
            continue;
        }

        if (rel.empty() && EqualsNoCase(name, L"preview.jpg")) continue;
        if (EqualsNoCase(name, L"ModToolTurbo.log") ||
            EqualsNoCase(name, L"ModToolTurbo.ini") ||
            EqualsNoCase(name, L"compile-cache.txt")) continue;

        FileEntry e;
        e.rel = childRel;
        e.full = JoinPath(root, childRel);
        e.size = ((unsigned long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
        out.push_back(e);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return ok;
}

bool StableFileHash(const FileEntry& e, unsigned long long& h) {
    HANDLE file = CreateFileW(e.full.c_str(), GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (file == INVALID_HANDLE_VALUE) return false;

    BY_HANDLE_FILE_INFORMATION before;
    if (!GetFileInformationByHandle(file, &before)) {
        CloseHandle(file);
        return false;
    }
    unsigned long long size = ((unsigned long long)before.nFileSizeHigh << 32) | before.nFileSizeLow;
    h = HashBytes(h, &size, sizeof(size));

    unsigned char buf[65536];
    unsigned long long total = 0;
    bool ok = true;
    for (;;) {
        DWORD read = 0;
        if (!ReadFile(file, buf, (DWORD)sizeof(buf), &read, NULL)) {
            ok = false;
            break;
        }
        if (read == 0) break;
        h = HashBytes(h, buf, read);
        total += read;
    }

    BY_HANDLE_FILE_INFORMATION after;
    if (ok) {
        if (!GetFileInformationByHandle(file, &after)) {
            ok = false;
        } else if (after.ftLastWriteTime.dwLowDateTime != before.ftLastWriteTime.dwLowDateTime ||
                   after.ftLastWriteTime.dwHighDateTime != before.ftLastWriteTime.dwHighDateTime ||
                   ((unsigned long long)after.nFileSizeHigh << 32 | after.nFileSizeLow) != size ||
                   total != size) {
            ok = false; // file changed while hashing
        }
    }
    CloseHandle(file);
    return ok;
}

bool ComputeFingerprintOnce(const std::wstring& root, unsigned long long& out) {
    if (!DirExists(root)) return false;

    std::vector<FileEntry> files;
    if (!CollectFiles(root, L"", files, 0)) return false;

    std::sort(files.begin(), files.end(), [](const FileEntry& a, const FileEntry& b) {
        int c = _wcsicmp(a.rel.c_str(), b.rel.c_str());
        if (c != 0) return c < 0;
        return a.rel < b.rel;
    });

    unsigned long long h = HashBytes(kFnvOffset, "MTT1CONTENT", 11);
    unsigned long long count = (unsigned long long)files.size();
    h = HashBytes(h, &count, sizeof(count));
    for (size_t i = 0; i < files.size(); ++i) {
        std::wstring relLower = LowerCopy(files[i].rel);
        h = HashBytes(h, relLower.data(), relLower.size() * sizeof(wchar_t));
        if (!StableFileHash(files[i], h)) return false;
        h = HashBytes(h, "|", 1);
    }
    out = h;
    return true;
}

bool ComputeContentFingerprint(const std::wstring& root, unsigned long long& out) {
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (ComputeFingerprintOnce(root, out)) return true;
    }
    return false;
}

bool ComputeCompilerSignature(const std::wstring& path, unsigned long long& out) {
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad)) return false;
    if (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) return false;

    unsigned long long h = HashBytes(kFnvOffset, "MTT1COMPILER", 12);
    std::wstring lower = NormalizePath(path);
    h = HashBytes(h, lower.data(), lower.size() * sizeof(wchar_t));
    unsigned long long size = ((unsigned long long)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
    h = HashBytes(h, &size, sizeof(size));
    h = HashBytes(h, &fad.ftLastWriteTime, sizeof(fad.ftLastWriteTime));
    out = h;
    return true;
}

bool CacheHasFiles(const std::wstring& ugc) {
    std::wstring cache = JoinPath(ugc, L"Cache");
    if (!DirExists(cache)) return false;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(JoinPath(cache, L"*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    bool found = false;
    do {
        if (wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0) {
            found = true;
            break;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return found;
}

struct StateEntry {
    unsigned long long path = 0;
    unsigned long long fp = 0;
    unsigned long long comp = 0;
};

std::wstring StateFilePath(HINSTANCE self) {
    wchar_t buf[32768];
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, (DWORD)(sizeof(buf) / sizeof(buf[0])));
    std::wstring base;
    if (n > 0 && n < (DWORD)(sizeof(buf) / sizeof(buf[0]))) {
        base = std::wstring(buf, n);
    } else {
        base = GetSelfDir(self);
    }
    std::wstring dir = JoinPath(base, L"SMModToolTurbo");
    CreateDirectoryW(dir.c_str(), NULL);
    return JoinPath(dir, L"compile-cache.txt");
}

bool ReadAll(const std::wstring& path, std::string& out) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        return GetLastError() == ERROR_FILE_NOT_FOUND;
    }
    char buf[8192];
    bool ok = true;
    for (;;) {
        DWORD read = 0;
        if (!ReadFile(h, buf, sizeof(buf), &read, NULL)) { ok = false; break; }
        if (read == 0) break;
        out.append(buf, read);
        if (out.size() > 4 * 1024 * 1024) { ok = false; break; }
    }
    CloseHandle(h);
    return ok;
}

bool LoadState(std::vector<StateEntry>& out) {
    out.clear();
    std::string data;
    if (!ReadAll(g_state_path, data)) return false;
    size_t pos = 0;
    while (pos < data.size()) {
        size_t end = data.find('\n', pos);
        if (end == std::string::npos) end = data.size();
        std::string line = data.substr(pos, end - pos);
        pos = end + 1;
        if (line.empty() || line[0] == '#') continue;
        unsigned long long p = 0, f = 0, c = 0;
        if (sscanf(line.c_str(), "%llx %llx %llx", &p, &f, &c) == 3) {
            StateEntry e;
            e.path = p; e.fp = f; e.comp = c;
            out.push_back(e);
        }
    }
    return true;
}

bool WriteState(const std::vector<StateEntry>& entries) {
    std::wstring tmp = g_state_path + L".tmp";
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    std::string data = "# SM ModTool Turbo compile cache v1\n";
    char line[128];
    for (size_t i = 0; i < entries.size(); ++i) {
        int n = _snprintf(line, sizeof(line), "%016llx %016llx %016llx\n",
                          entries[i].path, entries[i].fp, entries[i].comp);
        if (n > 0) data.append(line, (size_t)n);
    }
    DWORD written = 0;
    bool ok = WriteFile(h, data.data(), (DWORD)data.size(), &written, NULL) &&
              written == data.size();
    CloseHandle(h);
    if (!ok) {
        DeleteFileW(tmp.c_str());
        return false;
    }
    return MoveFileExW(tmp.c_str(), g_state_path.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

bool LookupState(unsigned long long pathHash, unsigned long long fp, unsigned long long comp) {
    std::vector<StateEntry> entries;
    if (!LoadState(entries)) return false;
    for (size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].path == pathHash && entries[i].fp == fp && entries[i].comp == comp) {
            return true;
        }
    }
    return false;
}

void RememberState(unsigned long long pathHash, unsigned long long fp, unsigned long long comp) {
    if (!g_state_cs_init) return;
    EnterCriticalSection(&g_state_cs);
    std::vector<StateEntry> entries;
    if (LoadState(entries)) {
        bool found = false;
        for (size_t i = 0; i < entries.size(); ++i) {
            if (entries[i].path == pathHash) {
                entries[i].fp = fp;
                entries[i].comp = comp;
                found = true;
                break;
            }
        }
        if (!found) {
            StateEntry e;
            e.path = pathHash; e.fp = fp; e.comp = comp;
            entries.push_back(e);
        }
        if (!WriteState(entries)) {
            LogLine(L"warning: could not write compile cache state");
        }
    }
    LeaveCriticalSection(&g_state_cs);
}

struct WatcherCtx {
    HANDLE process;
    std::wstring ugc;
    unsigned long long fingerprint;
    unsigned long long compiler_sig;
};

DWORD WINAPI WatcherThread(LPVOID param) {
    WatcherCtx* ctx = static_cast<WatcherCtx*>(param);
    if (!ctx) return 0;
    WaitForSingleObject(ctx->process, INFINITE);
    DWORD code = 1;
    if (GetExitCodeProcess(ctx->process, &code) && code == 0) {
        unsigned long long fp = 0;
        if (ComputeContentFingerprint(ctx->ugc, fp) && fp == ctx->fingerprint) {
            RememberState(HashPath(ctx->ugc), fp, ctx->compiler_sig);
            LogLine(L"content compile succeeded; cache recorded for %s", ctx->ugc.c_str());
        } else {
            LogLine(L"content changed during compile; cache not recorded for %s", ctx->ugc.c_str());
        }
    } else {
        LogLine(L"content compile failed (exit %lu); cache not recorded", code);
    }
    CloseHandle(ctx->process);
    delete ctx;
    return 0;
}

std::wstring FirstToken(const std::wstring& cmdline) {
    size_t i = 0;
    while (i < cmdline.size() && iswspace(cmdline[i])) ++i;
    if (i >= cmdline.size()) return std::wstring();
    if (cmdline[i] == L'"') {
        size_t start = ++i;
        while (i < cmdline.size() && cmdline[i] != L'"') ++i;
        return cmdline.substr(start, i - start);
    }
    size_t start = i;
    while (i < cmdline.size() && !iswspace(cmdline[i])) ++i;
    return cmdline.substr(start, i - start);
}

bool HasToken(const std::wstring& cmdline, const wchar_t* token) {
    size_t i = 0;
    while (i < cmdline.size()) {
        while (i < cmdline.size() && iswspace(cmdline[i])) ++i;
        if (i >= cmdline.size()) break;
        bool quoted = false;
        if (cmdline[i] == L'"') { quoted = true; ++i; }
        size_t start = i;
        while (i < cmdline.size()) {
            if (quoted) {
                if (cmdline[i] == L'"') { ++i; break; }
            } else if (iswspace(cmdline[i])) {
                break;
            }
            ++i;
        }
        std::wstring t = cmdline.substr(start, i - start);
        if (_wcsicmp(t.c_str(), token) == 0) return true;
        if (quoted && i < cmdline.size() && cmdline[i] == L'"') ++i;
    }
    return false;
}

bool ExtractUgcPath(const std::wstring& cmdline, std::wstring& out) {
    std::wstring lower = LowerCopy(cmdline);
    size_t pos = lower.find(L"--ugc");
    while (pos != std::wstring::npos) {
        size_t p = pos + 5;
        while (p < lower.size() && iswspace(lower[p])) ++p;
        if (p < lower.size() && lower[p] == L'=') {
            ++p;
            while (p < lower.size() && iswspace(lower[p])) ++p;
        }
        if (p < lower.size()) {
            bool quoted = false;
            if (lower[p] == L'"') { quoted = true; ++p; }
            size_t start = p;
            if (quoted) {
                while (p < lower.size() && lower[p] != L'"') ++p;
            } else {
                while (p < lower.size() && !iswspace(lower[p])) ++p;
            }
            if (p > start && (!quoted || (p < lower.size() && lower[p] == L'"'))) {
                out = cmdline.substr(start, p - start);
                return !out.empty();
            }
        }
        pos = lower.find(L"--ugc", pos + 1);
    }
    return false;
}

} // namespace

void CompileCacheInit(HINSTANCE self) {
    InitializeCriticalSection(&g_state_cs);
    g_state_cs_init = true;
    g_state_path = StateFilePath(self);
}

bool ParseContentCompilerInvocation(const wchar_t* application_name,
                                    const wchar_t* command_line,
                                    CompileRequest& out) {
    if (!command_line || !*command_line) return false;

    std::wstring cmd = command_line;
    std::wstring lower = LowerCopy(cmd);
    if (lower.find(L"contentcompiler.exe") == std::wstring::npos) return false;
    if (!HasToken(cmd, L"--cache")) return false;

    std::wstring ugc;
    if (!ExtractUgcPath(cmd, ugc)) return false;

    std::wstring commandExe = FirstToken(cmd);
    if (!commandExe.empty() && commandExe.size() >= 2 &&
        commandExe[0] == L'"' && commandExe[commandExe.size() - 1] == L'"') {
        commandExe = commandExe.substr(1, commandExe.size() - 2);
    }

    std::wstring exe;
    if (!commandExe.empty() &&
        LowerCopy(GetBaseName(commandExe)).find(L"contentcompiler") != std::wstring::npos) {
        exe = commandExe;
    } else if (application_name && *application_name) {
        exe = application_name;
    } else {
        exe = commandExe;
    }
    if (exe.empty()) return false;

    wchar_t full[32768];
    DWORD n = GetFullPathNameW(exe.c_str(), (DWORD)(sizeof(full) / sizeof(full[0])), full, NULL);
    if (n > 0 && n < (DWORD)(sizeof(full) / sizeof(full[0]))) {
        exe = std::wstring(full, n);
    }

    out.command_line = cmd;
    out.compiler_path = exe;
    out.ugc_path = ugc;
    while (out.ugc_path.size() > 1 &&
           (out.ugc_path[out.ugc_path.size() - 1] == L'\\' || out.ugc_path[out.ugc_path.size() - 1] == L'/')) {
        out.ugc_path.resize(out.ugc_path.size() - 1);
    }
    return !out.ugc_path.empty();
}

bool ShouldSkipCompile(CompileRequest& req) {
    if (!g_cfg.skip_unchanged_compile || g_cfg.force_compile) return false;
    if (req.ugc_path.empty() || req.compiler_path.empty()) return false;

    if (!req.has_fingerprint) {
        if (!ComputeContentFingerprint(req.ugc_path, req.fingerprint)) {
            LogLine(L"could not fingerprint %s; compile will run", req.ugc_path.c_str());
            return false;
        }
        req.has_fingerprint = true;
    }
    if (req.compiler_sig == 0) {
        if (!ComputeCompilerSignature(req.compiler_path, req.compiler_sig)) {
            LogLine(L"could not identify %s; compile will run", req.compiler_path.c_str());
            return false;
        }
    }
    if (!CacheHasFiles(req.ugc_path)) {
        LogLine(L"no existing cache for %s; compile will run", req.ugc_path.c_str());
        return false;
    }
    if (LookupState(HashPath(req.ugc_path), req.fingerprint, req.compiler_sig)) {
        LogLine(L"skipping unchanged content compile for %s", req.ugc_path.c_str());
        return true;
    }
    LogLine(L"content changed for %s; compile will run", req.ugc_path.c_str());
    return false;
}

void NoteCompileStarted(HANDLE process, const CompileRequest& req) {
    if (process == NULL || process == INVALID_HANDLE_VALUE) return;
    if (!req.has_fingerprint || req.compiler_sig == 0) return;

    HANDLE dup = NULL;
    if (!DuplicateHandle(GetCurrentProcess(), process, GetCurrentProcess(), &dup,
                         PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, 0)) {
        return;
    }
    WatcherCtx* ctx = new (std::nothrow) WatcherCtx;
    if (!ctx) {
        CloseHandle(dup);
        return;
    }
    ctx->process = dup;
    ctx->ugc = req.ugc_path;
    ctx->fingerprint = req.fingerprint;
    ctx->compiler_sig = req.compiler_sig;
    HANDLE th = CreateThread(NULL, 0, WatcherThread, ctx, 0, NULL);
    if (th) {
        CloseHandle(th);
    } else {
        CloseHandle(dup);
        delete ctx;
    }
}
