cmake_minimum_required(VERSION 3.10)
project(Kache)

set(CMAKE_CXX_STANDARD 17)

# Include headers
include_directories(include)
include_directories(src)

# Source files
set(SOURCES
    src/service/InMemoryStorage.cpp
    src/service/LRUPolicy.cpp
    src/main.cpp
)

add_executable(kache ${SOURCES})

set(SERVER_SOURCES
    src/server/TCPServer.cpp
)

add_executable(kache_server ${SERVER_SOURCES})