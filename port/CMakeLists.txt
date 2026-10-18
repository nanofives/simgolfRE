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

add_library(portsim STATIC src/portsim.cpp)
target_include_directories(portsim PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${SIMGOLF_MODEL_INCLUDE}")
if(MSVC)
  target_compile_options(portsim PRIVATE /W4 /WX /permissive-)
else()
  target_compile_options(portsim PRIVATE -Wall -Wextra -Werror -pedantic)
endif()

enable_testing()
add_executable(portsim_layout_test test/gamestate_layout_test.cpp)
target_link_libraries(portsim_layout_test PRIVATE portsim)
if(MSVC)
  target_compile_options(portsim_layout_test PRIVATE /W4 /WX /permissive-)
endif()
add_test(NAME gamestate_layout COMMAND portsim_layout_test)
