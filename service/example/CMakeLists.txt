cmake_minimum_required(VERSION 2.8.9)
project(example)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories(
    ../../core/inc
    ../../build/external/glog
    ../../external/glog/src/
    ../../external/json/include/
    ../../external/mqtt/include
    ../../external/mqtt/lib/cpp
    ../../external/uuid/src
)

set(PROJECT_LINK_LIBS
    liblinking_core.so
    libglog.so
    libmosquittopp.so
    pthread
    libuuid.so
)
link_directories(
    ../../build/core/
    ../../build/external/glog
    ../../build/lib
    ../../build/external/mqtt/lib
    ../../build/external/mqtt/lib/cpp
    ../../build/external/uuid/src
)

set(SOURCES main.cpp)
add_executable(example ${SOURCES})
target_link_libraries(example ${PROJECT_LINK_LIBS})

project(server)
set(SOURCES server.cpp)
add_executable(server ${SOURCES})
target_link_libraries(server ${PROJECT_LINK_LIBS})

project(client)
set(SOURCES client.cpp)
add_executable(client ${SOURCES})
target_link_libraries(client ${PROJECT_LINK_LIBS})
