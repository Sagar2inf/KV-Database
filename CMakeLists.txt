cmake_minimum_required(VERSION 3.28)

project(kv-store)

add_executable(kv-store 
    server.cpp
    ./routers/router.cpp
    ./disk/wal.cpp
    ./memory/skiplist.cpp
    ./memtable/memtable.cpp
    ./services/service.cpp
    ./transport/transport.cpp
)
