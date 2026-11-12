// ContentCompilerTurbo.exe
//
// The engine's icon renderer uses the main thread for every icon, even though
// the generic task manager starts one worker per CPU. The icon categories are
// independent, so this helper runs each category in its own ContentCompiler
// process (each has its own main thread) and then asks ContentCompiler to
// build the cache/atlas while skipping the already generated icons.
#include <windows.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cstdarg>

namespace {

struct Job {
    std::wstring flag;
    PROCESS_INFORMATION process = {};
    bool started = false;
    DWORD exitCode = 1;
};

const wchar_t* kCategories[] = {
    L"--asset",
    L"--harvestable",
    L"--node",
    L"--voxelmesh",
    L"--prefab",
    L"--blueprint",
    L"--customization",
    L"--shape",
};

std::wstring g_logPath;

void Log(const wchar_t* fmt, ...) {
    if (g_logPath.empty()) return;
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
                       L"[%04u-%02u-%02u %02u:%02u:%02u] %s\r\n",
                       st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, msg);
    if (m < 0) m = (int)((sizeof(line) / sizeof(line[0])) - 1);
    line[m] = 0;

    HANDLE file = CreateFileW(g_logPath.c_str(), FILE_APPEND_DATA,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return;
    char utf8[8192];
    int bytes = WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8, sizeof(utf8), NULL, NULL);
    if (bytes > 0) {
        DWORD written = 0;
        WriteFile(file, utf8, (DWORD)(bytes - 1), &written, NULL);
    }
    CloseHandle(file);
}

std::wstring GetSelfDir() {
    wchar_t path[32768];
    DWORD n = GetModuleFileNameW(NULL, path, (DWORD)(sizeof(path) / sizeof(path[0])));
    if (n == 0) return L".";
    std::wstring p(path, n);
    size_t pos = p.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"." : p.substr(0, pos);
}

std::wstring DirOf(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"." : path.substr(0, pos);
}

std::wstring Quote(const std::wstring& s) {
    return L"\"" + s + L"\"";
}

bool FileExists(const std::wstring& path) {
    DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring ArgValue(const std::wstring& arg, const wchar_t* prefix) {
    size_t n = wcslen(prefix);
    if (arg.size() > n && _wcsnicmp(arg.c_str(), prefix, n) == 0) {
        std::wstring v = arg.substr(n);
        if (!v.empty() && v[0] == L'=') v = v.substr(1);
        if (v.size() >= 2 && v[0] == L'"' && v[v.size() - 1] == L'"') {
            v = v.substr(1, v.size() - 2);
        }
        return v;
    }
    return std::wstring();
}

void CloseJob(Job& job) {
    if (job.process.hThread) CloseHandle(job.process.hThread);
    if (job.process.hProcess) CloseHandle(job.process.hProcess);
    job.process.hThread = NULL;
    job.process.hProcess = NULL;
}

bool StartCompiler(const std::wstring& compiler, const std::wstring& compilerDir,
                   const std::wstring& commandLine, DWORD flags, Job& job,
                   const wchar_t* label) {
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    std::vector<wchar_t> mutableLine(commandLine.begin(), commandLine.end());
    mutableLine.push_back(0);

    Log(L"starting %s: %s", label, commandLine.c_str());
    if (!CreateProcessW(compiler.c_str(), mutableLine.data(), NULL, NULL, FALSE,
                        flags, NULL, compilerDir.c_str(), &si, &job.process)) {
        Log(L"  failed to start %s: %lu", label, GetLastError());
        return false;
    }
    job.started = true;
    job.exitCode = 1;
    return true;
}

bool WaitJob(Job& job, const wchar_t* label) {
    if (!job.started) return false;
    WaitForSingleObject(job.process.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(job.process.hProcess, &code);
    job.exitCode = code;
    Log(L"finished %s: exit %lu", label, code);
    return code == 0;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    std::wstring compiler;
    std::wstring ugc;
    bool dontOverwrite = false;
    int jobs = 8;

    for (int i = 1; i < argc; ++i) {
        std::wstring a = argv[i];
        if (a == L"--compiler" && i + 1 < argc) {
            compiler = argv[++i];
        } else if (a == L"--ugc" && i + 1 < argc) {
            ugc = argv[++i];
        } else if (a == L"--jobs" && i + 1 < argc) {
            jobs = _wtoi(argv[++i]);
        } else if (a == L"--dont-overwrite-icons") {
            dontOverwrite = true;
        } else {
            std::wstring v = ArgValue(a, L"--compiler");
            if (!v.empty()) { compiler = v; continue; }
            v = ArgValue(a, L"--ugc");
            if (!v.empty()) { ugc = v; continue; }
            v = ArgValue(a, L"--jobs");
            if (!v.empty()) { jobs = _wtoi(v.c_str()); continue; }
        }
    }

    g_logPath = GetSelfDir() + L"\\ContentCompilerTurbo.log";
    if (compiler.empty() || ugc.empty()) {
        Log(L"usage: ContentCompilerTurbo.exe --compiler=<ContentCompiler.exe> --ugc=<mod> [--dont-overwrite-icons] [--jobs=N]");
        return 2;
    }
    if (!FileExists(compiler)) {
        Log(L"compiler not found: %s", compiler.c_str());
        return 2;
    }
    if (jobs < 1) jobs = 1;
    if (jobs > (int)(sizeof(kCategories) / sizeof(kCategories[0]))) {
        jobs = (int)(sizeof(kCategories) / sizeof(kCategories[0]));
    }

    std::wstring compilerDir = DirOf(compiler);
    std::wstring base = Quote(compiler) + L" --ugc=" + Quote(ugc);
    DWORD startFlags = CREATE_NO_WINDOW;
    LARGE_INTEGER freq, start, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    Log(L"compiler=%s", compiler.c_str());
    Log(L"ugc=%s", ugc.c_str());
    Log(L"jobs=%d dontOverwrite=%d", jobs, dontOverwrite ? 1 : 0);

    // Run independent icon categories concurrently, one process per category.
    std::vector<Job> running;
    int nextCategory = 0;
    bool allCategoriesOk = true;
    while (nextCategory < (int)(sizeof(kCategories) / sizeof(kCategories[0])) ||
           !running.empty()) {
        while (nextCategory < (int)(sizeof(kCategories) / sizeof(kCategories[0])) &&
               (int)running.size() < jobs) {
            Job job;
            job.flag = kCategories[nextCategory++];
            std::wstring cmd = base + L" " + job.flag;
            if (dontOverwrite) cmd += L" --dont-overwrite-icons";
            if (!StartCompiler(compiler, compilerDir, cmd, startFlags, job, job.flag.c_str())) {
                CloseJob(job);
                continue;
            }
            running.push_back(job);
            // Avoid all child processes opening the same timestamped log file.
            Sleep(1100);
        }
        if (!running.empty()) {
            Job& job = running.front();
            if (!WaitJob(job, job.flag.c_str())) allCategoriesOk = false;
            CloseJob(job);
            running.erase(running.begin());
        }
    }

    // The cache pass is authoritative. It must run after the icon producers so
    // the atlas sees every icon. --dont-overwrite-icons keeps it from redoing
    // the parallel work; if a future build drops that flag, retry without it.
    Log(L"all icon categories ok=%d", allCategoriesOk ? 1 : 0);
    bool skipGeneratedIcons = dontOverwrite || allCategoriesOk;
    Job cacheJob;
    std::wstring cacheCmd = base + L" --cache";
    if (skipGeneratedIcons) cacheCmd += L" --dont-overwrite-icons";
    bool cacheOk = StartCompiler(compiler, compilerDir, cacheCmd, startFlags, cacheJob, L"--cache") &&
                   WaitJob(cacheJob, L"--cache");
    DWORD cacheCode = cacheJob.exitCode;
    CloseJob(cacheJob);
    if (!cacheOk && skipGeneratedIcons && !dontOverwrite) {
        Log(L"cache pass failed with --dont-overwrite-icons; retrying without it");
        std::wstring retryCmd = base + L" --cache";
        Job retry;
        if (StartCompiler(compiler, compilerDir, retryCmd, startFlags, retry, L"--cache retry")) {
            cacheOk = WaitJob(retry, L"--cache retry");
            cacheCode = retry.exitCode;
            CloseJob(retry);
        }
    }

    // The atlas is small; run it after the icons. A failure here is not fatal
    // because the cache pass may already have produced the atlas.
    Job atlasJob;
    if (StartCompiler(compiler, compilerDir, base + L" --create-atlas", startFlags,
                      atlasJob, L"--create-atlas")) {
        WaitJob(atlasJob, L"--create-atlas");
        CloseJob(atlasJob);
    }

    QueryPerformanceCounter(&now);
    double seconds = (double)(now.QuadPart - start.QuadPart) / (double)freq.QuadPart;
    Log(L"total %.1f s; cache exit %lu", seconds, cacheCode);
    return cacheOk ? 0 : (int)cacheCode;
}
