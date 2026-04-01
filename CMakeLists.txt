cmake_minimum_required(VERSION 3.10)
project(Kache)

set(CMAKE_CXX_STANDARD 17)

# Include headers
include_directories(include)
include_directories(src)

# Source files
set(SOURCES
    src/InMemoryStorage.cpp
    src/LRUPolicy.cpp
    src/main.cpp
)

add_executable(kache ${SOURCES})