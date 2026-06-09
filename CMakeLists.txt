cmake_minimum_required(VERSION 3.16)
project(kv-store CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-O2 -Wall -Wextra -Wno-unused-parameter)

set(COMMON_SOURCES
    disk/wal.cpp
    disk/sstable.cpp
    disk/sstable_manager.cpp
    memory/skiplist.cpp
    memtable/memtable.cpp
    services/service.cpp
    transport/transport.cpp
    routers/router.cpp
)

# Platform libraries
if(WIN32)
    set(PLATFORM_LIBS ws2_32)
else()
    set(PLATFORM_LIBS pthread)
endif()

add_executable(kv-server server.cpp ${COMMON_SOURCES})
target_link_libraries(kv-server ${PLATFORM_LIBS})

add_executable(kv-client client.cpp transport/transport.cpp)
target_link_libraries(kv-client ${PLATFORM_LIBS})
