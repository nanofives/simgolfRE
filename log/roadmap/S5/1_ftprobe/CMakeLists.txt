cmake_minimum_required(VERSION 3.16)
project(ftprobe LANGUAGES CXX)
find_package(freetype CONFIG REQUIRED HINTS "$ENV{USERPROFILE}/vcpkg/installed/x64-windows")
add_executable(ftprobe main.cpp)
target_link_libraries(ftprobe PRIVATE Freetype::Freetype)
add_custom_command(TARGET ftprobe POST_BUILD COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:freetype>"
                   "$<TARGET_FILE_DIR:ftprobe>")
