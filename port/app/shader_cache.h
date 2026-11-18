// P35A item 6 (Mariano's play-test of 31344e57: "when opening the game there's a long black window without nothing until the
// loading screen starts"; log/roadmap/P35/P35A_notes.md): the app compiles its HLSL at every start (D3DCompile at optimisation
// level 3: 11 of the 12 s the window stayed black on LIVING-PC, 10 s for one shader set alone on MARIANO-PC). The compiled
// bytecode is kept in a folder of the data directory, one file per (source, defines, entry, target, flags, compiler DLL), and a
// later start reads it back instead of compiling. A file that does not check out is compiled again and rewritten.
#pragma once

#include <string>

namespace portapp {

// The folder (created on the first write); empty = no cache (the wrapper below then answers the compiler's own pointer).
void shader_cache_set_dir(const std::string &dir);
// `d3d_compile` is d3dcompiler_47.dll's D3DCompile and `module` that DLL's handle; the answer has D3DCompile's signature and
// either reads the cached bytecode or compiles and stores it. Safe from several threads.
void *shader_cache_wrap(void *d3d_compile, void *module);
// {"dir":..,"hits":n,"compiled":n,"compile_ms":x,"read_ms":x,"bad":n}
std::string shader_cache_json();
// one line for the log: hits, compiled, milliseconds
std::string shader_cache_line();

}  // namespace portapp
