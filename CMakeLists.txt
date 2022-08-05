cmake_minimum_required(VERSION 3.20)

project (node)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

set(BUILD_SHARED_LIBS ON)

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

#definitions
add_compile_definitions(BOOST_STACKTRACE_USE_ADDR2LINE)
add_compile_definitions(USE_IROHA_ED25519)

# lib
FILE(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_SOURCE_DIR} src/*.cpp src/*.hpp src/*.c src/*.h)
list(REMOVE_ITEM SOURCES "src/main.cpp")

add_library(${PROJECT_NAME}_lib STATIC ${SOURCES})

include_directories(${CMAKE_SOURCE_DIR}/src)
target_precompile_headers(${PROJECT_NAME}_lib PUBLIC "src/pch.h")

# dependencies
find_path(CPPCODEC_INCLUDE_DIRS "cppcodec/base64_rfc4648.hpp")
target_include_directories(${PROJECT_NAME}_lib PUBLIC ${CPPCODEC_INCLUDE_DIRS})

find_library(USOCKETS_LIBLARY uSockets REQUIRED)
target_link_libraries(${PROJECT_NAME}_lib PUBLIC ${USOCKETS_LIBLARY})	

find_path(UWEBSOCKETS_INCLUDE_DIRS "uwebsockets/App.h")
include_directories(${UWEBSOCKETS_INCLUDE_DIRS})

find_package(OpenSSL REQUIRED)
target_link_libraries(${PROJECT_NAME}_lib PUBLIC OpenSSL::SSL OpenSSL::Crypto)

find_package(ed25519 CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME}_lib PUBLIC iroha::ed25519)	
	
find_package(nlohmann_json REQUIRED)
target_link_libraries(${PROJECT_NAME}_lib PUBLIC nlohmann_json nlohmann_json::nlohmann_json)

find_package(Boost 1.68.0 REQUIRED COMPONENTS system date_time filesystem regex log_setup log program_options unit_test_framework)
include_directories(${Boost_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME}_lib PUBLIC ${Boost_LIBRARIES})
	
find_path(BOOST_ALGORITHM_INCLUDE_DIRS "boost/algorithm/algorithm.hpp")
include_directories(${BOOST_ALGORITHM_INCLUDE_DIRS})

find_path(BOOST_ASIO_INCLUDE_DIRS "boost/asio.hpp")
include_directories(${BOOST_ASIO_INCLUDE_DIRS})

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME}_lib PUBLIC ${ZLIB_LIBRARIES})	

find_package(RocksDB CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME}_lib PUBLIC RocksDB::rocksdb)

target_link_libraries(${PROJECT_NAME}_lib PUBLIC dl)

# main binary
add_executable(${PROJECT_NAME} "src/main.cpp")
target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}_lib)
target_precompile_headers(${PROJECT_NAME} REUSE_FROM ${PROJECT_NAME}_lib)

# tests
FILE(GLOB_RECURSE TEST_SOURCES RELATIVE ${CMAKE_SOURCE_DIR} tests/*.cpp tests/*.hpp tests/*.c tests/*.h)
add_executable(${PROJECT_NAME}_tests ${TEST_SOURCES})
target_link_libraries(${PROJECT_NAME}_tests ${PROJECT_NAME}_lib)
target_precompile_headers(${PROJECT_NAME}_tests REUSE_FROM ${PROJECT_NAME}_lib)
target_include_directories(${PROJECT_NAME}_tests PUBLIC ${CMAKE_SOURCE_DIR}/tests)

# benchmarks
FILE(GLOB_RECURSE BENCHMARKS_SOURCES RELATIVE ${CMAKE_SOURCE_DIR} benchmarks/*.cpp benchmarks/*.hpp benchmarks/*.c benchmarks/*.h)
add_executable(${PROJECT_NAME}_benchmarks ${BENCHMARKS_SOURCES})
target_link_libraries(${PROJECT_NAME}_benchmarks ${PROJECT_NAME}_lib)
target_precompile_headers(${PROJECT_NAME}_benchmarks REUSE_FROM ${PROJECT_NAME}_lib)
target_include_directories(${PROJECT_NAME}_benchmarks PUBLIC ${CMAKE_SOURCE_DIR}/tests)
