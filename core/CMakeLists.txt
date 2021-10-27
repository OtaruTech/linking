cmake_minimum_required(VERSION 2.8.9)
project(linking_core)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(PROJECT_LINK_LIBS
    libglog.so
    libmosquittopp.so
    pthread
)
link_directories(
    ../build/external/glog
    ../build/lib
    ../build/external/mqtt/lib
    ../build/external/mqtt/lib/cpp
)

include_directories(
    inc
    ../build/external/glog
    ../external/glog/src/
    ../external/json/include/
    ../external/mqtt/include
    ../external/mqtt/lib/cpp
)

set(SOURCES
    src/osutils.cpp
    src/threadmanager.cpp
    src/message.cpp
    src/base64.cpp
    src/looper.cpp
    src/refbase.cpp
    src/sharedbuffer.cpp
    src/static.cpp
    src/stopwatch.cpp
    src/string16.cpp
    src/string8.cpp
    src/threads.cpp
    src/timers.cpp
    src/unicode.cpp
    src/vectorimpl.cpp
    src/messagehub.cpp
)

add_library(linking_core SHARED ${SOURCES})

target_link_libraries(linking_core)
