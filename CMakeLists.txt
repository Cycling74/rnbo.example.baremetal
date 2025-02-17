cmake_minimum_required(VERSION 3.17)
# we only use C++17 for the AudioFile lib, RNBO would be happy with C++11
set(CMAKE_CXX_STANDARD 17)

project(baremetal)
add_executable(baremetal main.cpp tlsf.c)
include_directories(export/common/)
