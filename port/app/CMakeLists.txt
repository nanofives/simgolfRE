# The Windows host (roadmap S7). S7.2: the input translation library and its checker (no dependency on portsim, no Win32
# window: messages are data). S7.3: the app executable simgolf_port (window, presentation, input capture, saves overlay,
# pacing), which links the portsim library.
#
# Three ways in:
#   - from port/CMakeLists.txt (`add_subdirectory(app)`, log/roadmap/S7/2_merge.md): portsim exists, everything is built;
#   - `cmake -S port/app -B <dir>` on its own (port/test/test_app_input.py): only the translator and its checker;
#   - `cmake -S port/app -B <dir> -DPORTAPP_SHELL=ON` on its own (port/test/test_app_shell.py): port/ is added first (its
#     portsim and portsim_c targets), then the app. port/'s own `add_subdirectory(app)` returns at once in that case.
if(PORTAPP_ADDING_PORT)
  return()  # port/ included this directory while this file was adding port/ (standalone shell build)
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  cmake_minimum_required(VERSION 3.16)
  project(portapp LANGUAGES CXX)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
  option(PORTAPP_SHELL "standalone: also build the app executable (adds port/ for the portsim library)" OFF)
  if(PORTAPP_SHELL)
    set(PORTAPP_ADDING_PORT ON)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/.." portsim)
    unset(PORTAPP_ADDING_PORT)
  endif()
else()
  set(PORTAPP_SHELL ON)
endif()

add_library(portapp_input STATIC input_map.cpp)
target_include_directories(portapp_input PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(simgolf_input_check tools/input_check.cpp)
target_link_libraries(simgolf_input_check PRIVATE portapp_input)

if(MSVC)
  target_compile_options(portapp_input PRIVATE /W4 /WX /permissive-)
  target_compile_options(simgolf_input_check PRIVATE /W4 /WX /permissive-)
else()
  target_compile_options(portapp_input PRIVATE -Wall -Wextra -Werror -pedantic)
  target_compile_options(simgolf_input_check PRIVATE -Wall -Wextra -Werror -pedantic)
endif()

# S7.3: the app. Windows only (the window, GDI presentation and the pacing clock are Win32).
if(PORTAPP_SHELL AND WIN32 AND TARGET portsim)
  # S7.10: audio out (audio_out.*: the shadow audio context on its thread; ds_sink.*: the DirectSound / capture sink). dsound.lib
  # and winmm.lib are Windows system libraries.
  add_library(portapp_audio STATIC audio_out.cpp ds_sink.cpp)
  target_include_directories(portapp_audio PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(portapp_audio PUBLIC portsim dsound winmm)
  target_compile_definitions(portapp_audio PRIVATE _CRT_SECURE_NO_WARNINGS)
  # P9A item 1: the player's simgolf_port.exe is a WIN32-subsystem program (no console window; main() stays the entry
  # through mainCRTStartup; stdout / stderr: stdio_log.h). simgolf_port_console.exe is the same objects linked for the
  # console subsystem (a terminal that wants to wait for it). Both are built by the `simgolf_port` target.
  add_library(simgolf_port_objs OBJECT main.cpp window.cpp present.cpp textres.cpp pacing.cpp saves.cpp host.cpp stdio_log.cpp)
  target_link_libraries(simgolf_port_objs PRIVATE portapp_input portapp_audio portsim)
  target_compile_definitions(simgolf_port_objs PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)
  add_executable(simgolf_port WIN32 $<TARGET_OBJECTS:simgolf_port_objs>)
  target_link_libraries(simgolf_port PRIVATE portapp_input portapp_audio portsim winmm)
  add_executable(simgolf_port_console $<TARGET_OBJECTS:simgolf_port_objs>)
  target_link_libraries(simgolf_port_console PRIVATE portapp_input portapp_audio portsim winmm)
  add_dependencies(simgolf_port simgolf_port_console)
  if(MSVC)
    target_link_options(simgolf_port PRIVATE /ENTRY:mainCRTStartup)
  endif()
  add_executable(simgolf_audio_check tools/audio_check.cpp)
  target_link_libraries(simgolf_audio_check PRIVATE portapp_audio)
  target_compile_definitions(simgolf_audio_check PRIVATE _CRT_SECURE_NO_WARNINGS)
  if(MSVC)
    target_compile_options(simgolf_port_objs PRIVATE /W4 /WX /permissive- /EHsc)
    target_compile_options(portapp_audio PRIVATE /W4 /WX /permissive- /EHsc)
    target_compile_options(simgolf_audio_check PRIVATE /W4 /WX /permissive- /EHsc)
  endif()
endif()
