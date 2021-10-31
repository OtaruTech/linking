cmake_minimum_required(VERSION 2.8.9)
project(linking)

include_directories(inc)

add_subdirectory(core)
add_subdirectory(external/glog)
add_subdirectory(external/gflags)
add_subdirectory(external/uuid)
add_subdirectory(external/mqtt)

add_subdirectory(service/daemon)
add_subdirectory(service/example)
