// P33D fix 2 (Mariano's fifteenth play-test, item 5): the app's last-chance handler. Until P33D a fault on any thread ended
// the process with nothing in <data>/simgolf_port.log, and a sim stop (a portsim exception the sim thread catches: `missing:
// <callee>`, a fault in the sim's address space, a divide error) closed the window like a normal quit, exit code 0, so neither
// the log nor Windows' error reporting saw anything.
//   - crash_install: an unhandled-exception filter (every thread) and a SIGABRT handler (std::terminate -> abort). Each writes a minidump to
//     <data>/crashes/simgolf_port_<yyyymmdd-hhmmss>_<pid>.dmp (thread list, stacks, the memory they point at; not the whole
//     heap) and one line to the log: `CRASH: ...` with the exception code, module + offset of the faulting instruction, the
//     thread, the dump's path and the context line (the recording). No dialog: the filter then lets Windows go on as before.
//   - crash_log_line: one line straight to the log file and console (stdio_log.h), past the pipe the stdio streams use, so a
//     line written while the process dies is not lost in the pipe.
// SG_APP_CRASHDUMP=off: no handler (the process ends as before P33D). --negctl crash-av / crash-terminate (main.cpp) fault on
// purpose after READY, for port/test/test_app_crash.py.
#pragma once

#include <string>

namespace portapp {

void crash_install(const std::string &data_dir);
void crash_set_context(const std::string &line);   // e.g. "record <dir>" (copied; read by the handlers)
void crash_log_line(const std::string &line);       // a newline is added
std::string crash_dir();                            // <data>/crashes ("" before crash_install)
int crash_dumps_written();

}  // namespace portapp
