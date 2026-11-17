// P33D fix 2: the app's last-chance handler (crash.h).
#include "crash.h"

#include <windows.h>
// dbghelp.h after windows.h
#include <dbghelp.h>

#include <process.h>

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <mutex>

#include "stdio_log.h"

#pragma comment(lib, "dbghelp.lib")

namespace portapp {
namespace {
namespace fs = std::filesystem;

// everything the handlers read is prepared at install time: a crashed process may have a broken heap
wchar_t g_dir[MAX_PATH] = {0};      // <data>\crashes
char g_dir_a[MAX_PATH * 3] = {0};
char g_context[1024] = {0};
std::mutex g_context_mu;
std::atomic<int> g_entered{0};
std::atomic<int> g_dumps{0};
bool g_installed = false;

// "<module file name>+0x<offset>" of an address, or "0x<address>"
void where_of(const void *addr, char *out, size_t n) {
  HMODULE m = nullptr;
  if (addr && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                 static_cast<LPCWSTR>(addr), &m) && m) {
    char path[MAX_PATH] = {0};
    GetModuleFileNameA(m, path, MAX_PATH);
    const char *base = path;
    for (const char *p = path; *p; ++p)
      if (*p == '\\' || *p == '/') base = p + 1;
    std::snprintf(out, n, "%s+0x%llx", base,
                  static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(addr) - reinterpret_cast<uintptr_t>(m)));
    return;
  }
  std::snprintf(out, n, "0x%llx", static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(addr)));
}

// writes the dump; returns its path (UTF-8, in `path`) or "" with the reason in `why`
bool write_dump(EXCEPTION_POINTERS *ep, char *path, size_t n, char *why, size_t wn) {
  path[0] = 0;
  if (!g_dir[0]) {
    std::snprintf(why, wn, "no crash directory");
    return false;
  }
  CreateDirectoryW(g_dir, nullptr);
  SYSTEMTIME t;
  GetLocalTime(&t);
  wchar_t file[MAX_PATH * 2];
  std::swprintf(file, sizeof file / sizeof file[0], L"%ls\\simgolf_port_%04u%02u%02u-%02u%02u%02u_%lu.dmp", g_dir, t.wYear, t.wMonth,
                t.wDay, t.wHour, t.wMinute, t.wSecond, GetCurrentProcessId());
  HANDLE h = CreateFileW(file, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    std::snprintf(why, wn, "cannot create the dump file (error %lu)", GetLastError());
    return false;
  }
  MINIDUMP_EXCEPTION_INFORMATION mei;
  mei.ThreadId = GetCurrentThreadId();
  mei.ExceptionPointers = ep;
  mei.ClientPointers = FALSE;
  const auto type = static_cast<MINIDUMP_TYPE>(MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory | MiniDumpWithThreadInfo |
                                               MiniDumpWithUnloadedModules | MiniDumpWithHandleData);
  const BOOL ok = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), h, type, ep ? &mei : nullptr, nullptr, nullptr);
  const DWORD err = ok ? 0 : GetLastError();
  CloseHandle(h);
  WideCharToMultiByte(CP_UTF8, 0, file, -1, path, static_cast<int>(n), nullptr, nullptr);
  if (!ok) {
    std::snprintf(why, wn, "MiniDumpWriteDump failed (error 0x%lx)", err);
    DeleteFileW(file);
    path[0] = 0;
    return false;
  }
  g_dumps.fetch_add(1);
  return true;
}

void report(const char *what, EXCEPTION_POINTERS *ep) {
  char path[MAX_PATH * 3], why[256] = {0};
  const bool ok = write_dump(ep, path, sizeof path, why, sizeof why);
  char ctx[sizeof g_context];
  std::memcpy(ctx, g_context, sizeof ctx);   // no lock: the holder may be the thread that faulted
  ctx[sizeof ctx - 1] = 0;
  char line[4096];
  std::snprintf(line, sizeof line, "CRASH: %s (thread %lu); %s%s%s%s", what, GetCurrentThreadId(), ok ? "minidump " : "no minidump: ",
                ok ? path : why, ctx[0] ? "; " : "", ctx);
  crash_log_line(line);
}

LONG WINAPI on_unhandled(EXCEPTION_POINTERS *ep) {
  if (g_entered.fetch_add(1) == 0 && ep && ep->ExceptionRecord) {
    const EXCEPTION_RECORD &r = *ep->ExceptionRecord;
    char at[MAX_PATH + 32];
    where_of(r.ExceptionAddress, at, sizeof at);
    char what[MAX_PATH + 160];
    if (r.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && r.NumberParameters >= 2)
      std::snprintf(what, sizeof what, "exception 0x%08lx (access violation, %s 0x%llx) at %s", r.ExceptionCode,
                    r.ExceptionInformation[0] == 0 ? "read of" : r.ExceptionInformation[0] == 1 ? "write to" : "execute at",
                    static_cast<unsigned long long>(r.ExceptionInformation[1]), at);
    else
      std::snprintf(what, sizeof what, "exception 0x%08lx at %s", r.ExceptionCode, at);
    report(what, ep);
  }
  return EXCEPTION_CONTINUE_SEARCH;  // Windows ends the process as it did before P33D
}

// abort(): std::terminate (an exception that left a thread's function, a noexcept function that threw) ends there. MSVC keeps
// set_terminate per thread, SIGABRT's handler is the process's, so this covers threads started later too.
void on_abort(int) {
  if (g_entered.fetch_add(1) == 0) report("abort (std::terminate: an exception left a thread, or a noexcept function threw)", nullptr);
  _exit(3);  // abort's own end (the CRT's fail-fast report) would put up Windows' error dialog
}
}  // namespace

void crash_log_line(const std::string &line) {
  const std::string s = line + "\n";
  if (!stdio_log_direct(s.data(), s.size())) {
    std::fwrite(s.data(), 1, s.size(), stderr);
    std::fflush(stderr);
  }
}

void crash_set_context(const std::string &line) {
  std::lock_guard<std::mutex> g(g_context_mu);
  std::snprintf(g_context, sizeof g_context, "%s", line.c_str());
}

std::string crash_dir() { return g_dir_a; }
int crash_dumps_written() { return g_dumps.load(); }

void crash_install(const std::string &data_dir) {
  if (g_installed) return;
  if (const char *e = std::getenv("SG_APP_CRASHDUMP"))
    if (!std::strcmp(e, "off")) return;
  fs::path dir = data_dir.empty() ? fs::path() : fs::u8path(data_dir);
  if (dir.empty()) {
    wchar_t me[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, me, MAX_PATH);
    dir = fs::path(me).parent_path();
  }
  dir /= "crashes";
  std::wcsncpy(g_dir, dir.wstring().c_str(), MAX_PATH - 1);
  std::snprintf(g_dir_a, sizeof g_dir_a, "%s", dir.u8string().c_str());
  // dbghelp loaded now, not inside the handler
  LoadLibraryW(L"dbghelp.dll");
  SetErrorMode(GetErrorMode() | SEM_NOGPFAULTERRORBOX);  // the dump and the log line, never Windows' "stopped working" dialog
  SetUnhandledExceptionFilter(on_unhandled);
  std::signal(SIGABRT, on_abort);
  g_installed = true;
}

}  // namespace portapp
