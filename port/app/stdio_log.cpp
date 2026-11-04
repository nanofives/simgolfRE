// P9A item 1: see stdio_log.h.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <fcntl.h>
#include <io.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <thread>

#include "stdio_log.h"

namespace fs = std::filesystem;

namespace portapp {
namespace {

// what the process was given as a standard handle: a redirection the caller reads (a pipe, a file, NUL), a console, or nothing
enum class StdKind { None, Redirected, Console };
StdKind std_kind(DWORD which) {
  HANDLE h = GetStdHandle(which);
  if (h == nullptr || h == INVALID_HANDLE_VALUE) return StdKind::None;
  const DWORD t = GetFileType(h);
  if (t == FILE_TYPE_PIPE || t == FILE_TYPE_DISK) return StdKind::Redirected;
  if (t == FILE_TYPE_CHAR) {
    DWORD mode = 0;
    return GetConsoleMode(h, &mode) ? StdKind::Console : StdKind::Redirected;  // NUL is a character device too
  }
  return StdKind::None;
}

struct Tee {
  HANDLE read = nullptr;
  HANDLE file = INVALID_HANDLE_VALUE;
  HANDLE con = INVALID_HANDLE_VALUE;
  std::thread copier;
};
Tee *g_tee = nullptr;

void copy_loop(Tee *t) {
  char buf[4096];
  DWORD n = 0;
  while (ReadFile(t->read, buf, sizeof buf, &n, nullptr) && n > 0) {
    DWORD w = 0;
    if (t->file != INVALID_HANDLE_VALUE) {
      WriteFile(t->file, buf, n, &w, nullptr);
      FlushFileBuffers(t->file);
    }
    if (t->con != INVALID_HANDLE_VALUE) WriteFile(t->con, buf, n, &w, nullptr);
  }
}

void finish() {
  if (!g_tee) return;
  std::fflush(stdout);
  std::fflush(stderr);
  // every write end of the pipe is a CRT descriptor (1 and 2): closing them ends the copier's ReadFile with EOF
  _close(_fileno(stdout));
  _close(_fileno(stderr));
  if (g_tee->copier.joinable()) g_tee->copier.join();
  if (g_tee->file != INVALID_HANDLE_VALUE) CloseHandle(g_tee->file);
  CloseHandle(g_tee->read);
  g_tee = nullptr;
}

}  // namespace

StdioLogResult setup_stdio_log(const std::string &data_dir) {
  StdioLogResult r;
  if (const char *e = std::getenv("SG_APP_STDIO"))
    if (!std::strcmp(e, "keep")) return r;
  const StdKind kind = std_kind(STD_OUTPUT_HANDLE);
  if (kind == StdKind::Redirected) return r;  // a pipe or a file the caller reads (tests, tools): kept as it is

  static Tee tee;
  fs::path dir = data_dir.empty() ? fs::path() : fs::path(data_dir);
  if (dir.empty()) {
    wchar_t me[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, me, MAX_PATH);
    dir = fs::path(me).parent_path();
  }
  std::error_code ec;
  fs::create_directories(dir, ec);
  const fs::path log = dir / "simgolf_port.log";
  const fs::path prev = dir / "simgolf_port.prev.log";
  if (fs::exists(log, ec)) {
    fs::remove(prev, ec);
    fs::rename(log, prev, ec);
  }
  tee.file = CreateFileW(log.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (tee.file != INVALID_HANDLE_VALUE) r.log_path = log.string();
  // a console: the one the process was given (the console build in a terminal), else the parent's (the WIN32 build started
  // from a terminal gets no handles)
  if (kind == StdKind::Console || AttachConsole(ATTACH_PARENT_PROCESS)) {
    tee.con = CreateFileW(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    r.console = tee.con != INVALID_HANDLE_VALUE;
  }
  if (tee.file == INVALID_HANDLE_VALUE && tee.con == INVALID_HANDLE_VALUE) return r;

  HANDLE rd = nullptr, wr = nullptr;
  if (!CreatePipe(&rd, &wr, nullptr, 0)) return r;
  const int fd = _open_osfhandle(reinterpret_cast<intptr_t>(wr), _O_WRONLY | _O_TEXT);
  if (fd < 0) {
    CloseHandle(rd);
    CloseHandle(wr);
    return r;
  }
  // a WIN32-subsystem process starts with streams the CRT cannot write (no handle behind descriptors 1 and 2): reopen both
  // on NUL first so the streams are valid, then point their descriptors at the pipe
  FILE *dummy = nullptr;
  freopen_s(&dummy, "NUL", "w", stdout);
  freopen_s(&dummy, "NUL", "w", stderr);
  _dup2(fd, _fileno(stdout));
  _dup2(fd, _fileno(stderr));
  if (fd != _fileno(stdout) && fd != _fileno(stderr)) _close(fd);
  SetStdHandle(STD_OUTPUT_HANDLE, reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stdout))));
  SetStdHandle(STD_ERROR_HANDLE, reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stderr))));
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::setvbuf(stderr, nullptr, _IONBF, 0);
  tee.read = rd;
  tee.copier = std::thread(copy_loop, &tee);
  g_tee = &tee;
  std::atexit(finish);
  r.redirected = true;
  return r;
}

}  // namespace portapp
