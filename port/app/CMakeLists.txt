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
  add_executable(simgolf_port main.cpp window.cpp present.cpp pacing.cpp saves.cpp host.cpp)
  target_link_libraries(simgolf_port PRIVATE portapp_input portsim winmm)
  target_compile_definitions(simgolf_port PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)
  if(MSVC)
    target_compile_options(simgolf_port PRIVATE /W4 /WX /permissive- /EHsc)
  endif()
endif()
