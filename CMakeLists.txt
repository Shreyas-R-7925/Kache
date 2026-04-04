cmake_minimum_required(VERSION 3.10)
project(Kache)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


include_directories(include)

set(PARSER_SOURCES
    src/service/parser/ArrayParser.cpp
    src/service/parser/BulkStringParser.cpp
    src/service/parser/RespParser.cpp
    src/service/parser/RespValue.cpp
    src/service/Buffer.cpp
)

add_library(kache_parser ${PARSER_SOURCES})


set(STORAGE_SOURCES
    src/service/InMemoryStorage.cpp
    src/service/LRUPolicy.cpp
)

add_library(kache_storage ${STORAGE_SOURCES})

set(SERVER_SOURCES
    src/server/TCPServer.cpp
)

add_library(kache_server_lib ${SERVER_SOURCES})

add_executable(kache src/main.cpp)

target_link_libraries(kache
    kache_parser
    kache_storage
)

add_executable(kache_server src/main.cpp)

target_link_libraries(kache_server
    kache_parser
    kache_storage
    kache_server_lib
)