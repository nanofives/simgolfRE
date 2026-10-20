# portsim: the portable reimplementation of SimGolf's simulation (re/PORT_PLAN.md, re/PHASE23_PLAN.md decisions 1-5).
# Phase 2.2 skeleton: an (almost) empty library and one test that checks the generated GameState layout.
#
#   cmake -S port -B port/build [-A Win32 | -A x64]
#   cmake --build port/build --config Release
#   ctest --test-dir port/build -C Release
#
# Architecture: x86 and x64 both build. The data model stores the original 32-bit process's pointers as uint32_t and
# GameState is #pragma pack(1) with an explicit offset per member, so its layout is the same on both (the static_asserts
# in include/gamestate.h hold for both; tests/test_gamestate_gen.py compiles it with MSVC x86 and x64). Native x64 is the
# default target; x86 (-A Win32) is kept buildable for side-by-side runs with the original.
cmake_minimum_required(VERSION 3.16)
project(portsim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The data model header (re/model/include/simgolf_model.h, clean C99, no original addresses) is used in place.
set(SIMGOLF_MODEL_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../re/model/include")

# Phase 2.6: the registry (src/registry.cpp, modules listed in src/sim/modules.inc), the object table and sgh64
# (src/state_hash.cpp) and every port module file src/sim/*.cpp (picked up by the glob: phase 3 children add a file and
# one line in modules.inc, nothing here). PORTSIM_NEGCTL=OFF compiles the negative-control bodies away.
option(PORTSIM_NEGCTL "compile the negative-control (deliberately wrong) bodies, switched at run time" ON)
file(GLOB PORTSIM_SIM_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/sim/*.cpp")
# Phase 3.0 core: src/ptr.cpp (Ptr<T> string / copy idioms), src/execonst.cpp (ExeConstants), src/gamefiles.cpp.
add_library(portsim STATIC src/portsim.cpp src/registry.cpp src/state_hash.cpp src/ptr.cpp src/execonst.cpp
            src/gamefiles.cpp ${PORTSIM_SIM_SOURCES})
# MSBuild does not see the #include of modules.inc in registry.cpp: rebuild it whenever the module list changes.
set_source_files_properties(src/registry.cpp PROPERTIES
                            OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/sim/modules.inc")
target_include_directories(portsim PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${SIMGOLF_MODEL_INCLUDE}"
                           PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
if(PORTSIM_NEGCTL)
  target_compile_definitions(portsim PUBLIC PORTSIM_NEGCTL=1)
else()
  target_compile_definitions(portsim PUBLIC PORTSIM_NEGCTL=0)
endif()
if(MSVC)
  target_compile_options(portsim PRIVATE /W4 /WX /permissive-)
else()
  target_compile_options(portsim PRIVATE -Wall -Wextra -Werror -pedantic)
endif()

# Phase 2.6: the C ABI (include/portsim_c.h) as a DLL for the Python harness port/test/ (ctypes). Build it for the
# Python's architecture (x64): `~/bin/py -3.12 port/test/build.py`.
add_library(portsim_c SHARED src/portsim_c.cpp)
target_link_libraries(portsim_c PRIVATE portsim)
target_compile_definitions(portsim_c PRIVATE PORTSIM_C_BUILD)
if(MSVC)
  target_compile_options(portsim_c PRIVATE /W4 /WX /permissive-)
else()
  target_compile_options(portsim_c PRIVATE -Wall -Wextra -Werror -pedantic)
endif()

enable_testing()
add_executable(portsim_layout_test test/gamestate_layout_test.cpp)
target_link_libraries(portsim_layout_test PRIVATE portsim)
if(MSVC)
  target_compile_options(portsim_layout_test PRIVATE /W4 /WX /permissive-)
endif()
add_test(NAME gamestate_layout COMMAND portsim_layout_test)

# Phase 3.0: unit tests of the core helpers (arith, Ptr, string idioms, GameFiles, ExeConstants). The exe checks run
# when SG_EXE_PATH names the user's golf_clean.exe (port/test/test_core.py passes it); otherwise they are skipped.
add_executable(portsim_core_test test/core_test.cpp)
target_link_libraries(portsim_core_test PRIVATE portsim)
if(MSVC)
  target_compile_options(portsim_core_test PRIVATE /W4 /WX /permissive-)
endif()
add_test(NAME portsim_core COMMAND portsim_core_test)

# Phase 3.R: the Win32 runner (test/runner32): serves the C ABI of the Win32 portsim_c.dll to the 64-bit Python harness
# over stdin / stdout, so x87 code runs with real fldcw. Built only for -A Win32 (port/test/build.py --arch Win32).
if(CMAKE_SIZEOF_VOID_P EQUAL 4 AND MSVC)
  add_executable(portsim_runner32 test/runner32/runner32.cpp)
  target_include_directories(portsim_runner32 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
  add_dependencies(portsim_runner32 portsim_c)
  target_compile_options(portsim_runner32 PRIVATE /W4 /WX /permissive-)
endif()
