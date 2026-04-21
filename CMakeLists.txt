cmake_minimum_required(VERSION 3.10)
project(Kache)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

include_directories(include src)

set(PARSER_SOURCES
    src/service/Buffer.cpp
    src/service/parser/RespParser.cpp
    src/service/parser/RespReply.cpp
    src/service/parser/RespSerializer.cpp
)

add_library(kache_parser ${PARSER_SOURCES})

set(STORAGE_SOURCES
    src/service/PersistenceManager.cpp
    src/service/InMemoryStorage.cpp
    src/service/LFUPolicy.cpp
    src/service/LRUPolicy.cpp
)

add_library(kache_storage ${STORAGE_SOURCES})

set(COMMAND_SOURCES
    src/service/CommandHandler.cpp
)

add_library(kache_command ${COMMAND_SOURCES})
target_link_libraries(kache_command kache_parser kache_storage)

set(SERVER_SOURCES
    src/server/TCPServer.cpp
)

add_library(kache_server_lib ${SERVER_SOURCES})
target_link_libraries(kache_server_lib kache_command)

add_executable(kache src/main.cpp)
target_link_libraries(kache kache_server_lib)

add_executable(kache_server src/main.cpp)
target_link_libraries(kache_server kache_server_lib)

add_executable(kache_tests
    tests/RespParserTests.cpp
)
target_link_libraries(kache_tests kache_command)

add_test(NAME kache_tests COMMAND kache_tests)
