// P9A item 1: the player's simgolf_port.exe is a WIN32-subsystem program (no console window opens when it is started from a
// shortcut or Explorer). Its stdout / stderr still reach someone:
//   - stdout is a pipe, a file or NUL (every test and tool starts the app with subprocess pipes): nothing changes, the CRT
//     already writes there;
//   - otherwise (no handle: Explorer, a shortcut, a terminal starting the WIN32 build; or a console: the console build in a
//     terminal) both go to <data dir>/simgolf_port.log (the previous run's log is kept as simgolf_port.prev.log), and also to
//     the console: the one the process was given, else the parent's (AttachConsole(ATTACH_PARENT_PROCESS)). The two outputs
//     are fed by one pipe and a copy thread; the atexit handler flushes, closes the pipe's write ends and joins the thread.
// simgolf_port_console.exe is the same program linked for the console subsystem (a terminal that waits for it).
#pragma once

#include <string>

namespace portapp {

struct StdioLogResult {
  bool redirected = false;   // stdout / stderr were set up by this call (no redirection)
  bool console = false;      // the parent's console was attached
  std::string log_path;      // the log file (empty when not redirected or the file could not be opened)
};

// Call once, early in main (after the data directory is known). `data_dir` may be empty: then the log goes beside the exe.
// SG_APP_STDIO=keep disables it (the inherited / absent handles stay as they are).
StdioLogResult setup_stdio_log(const std::string &data_dir);

}  // namespace portapp
