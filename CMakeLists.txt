cmake_minimum_required(VERSION 3.10)

project(Kache)

# Use C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create executable
add_executable(kache
    src/main.cpp
    src/InMemoryStorage.cpp
)

target_include_directories(kache PRIVATE include)