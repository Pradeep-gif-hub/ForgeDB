cmake_minimum_required(VERSION 3.20)
project(forgedb VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler warnings
if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Sanitizer options
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

if(ENABLE_ASAN)
  message(STATUS "Enabling AddressSanitizer")
  add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address)
endif()

if(ENABLE_UBSAN)
  message(STATUS "Enabling UndefinedBehaviorSanitizer")
  add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=undefined)
endif()

if(ENABLE_TSAN)
  message(STATUS "Enabling ThreadSanitizer")
  add_compile_options(-fsanitize=thread)
  add_link_options(-fsanitize=thread)
endif()

include(FetchContent)

option(FORGEDB_BUILD_TESTS "Build ForgeDB test suite" ON)
option(FORGEDB_BUILD_BENCHMARKS "Build ForgeDB benchmark suite" ON)

if(FORGEDB_BUILD_TESTS)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  # For Windows: Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
  enable_testing()
endif()

if(FORGEDB_BUILD_BENCHMARKS)
  FetchContent_Declare(
    googlebenchmark
    URL https://github.com/google/benchmark/archive/refs/tags/v1.8.3.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googlebenchmark)
endif()

# Core static library
add_library(forgedb_core STATIC
  src/core/types.cpp
  src/storage/page.cpp
  src/storage/disk_manager.cpp
  src/storage/record.cpp
  src/index/b_plus_tree_page.cpp
  src/index/b_plus_tree.cpp
  src/buffer/lru_replacer.cpp
  src/buffer/buffer_pool_manager.cpp
  src/recovery/log_record.cpp
  src/recovery/log_manager.cpp
  src/recovery/recovery_manager.cpp
  src/transaction/transaction.cpp
  src/transaction/transaction_manager.cpp
  src/concurrency/thread_pool.cpp
  src/engine/stats.cpp
  src/engine/storage_engine.cpp
  src/network/parser.cpp
  src/network/server.cpp
)

target_include_directories(forgedb_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Executables
add_executable(forgedb-server src/server_main.cpp)
target_link_libraries(forgedb-server PRIVATE forgedb_core)

add_executable(forgedb-client src/client_main.cpp)
target_link_libraries(forgedb-client PRIVATE forgedb_core)

# Tests
if(FORGEDB_BUILD_TESTS)
  add_executable(forgedb_tests
    tests/core_types_test.cpp
    tests/page_test.cpp
    tests/disk_manager_test.cpp
    tests/record_test.cpp
    tests/b_plus_tree_test.cpp
    tests/buffer_pool_test.cpp
    tests/engine_test.cpp
    tests/wal_test.cpp
    tests/recovery_test.cpp
    tests/transaction_test.cpp
    tests/thread_pool_test.cpp
    tests/concurrency_test.cpp
    tests/network_test.cpp
  )
  target_link_libraries(forgedb_tests PRIVATE forgedb_core gtest_main)
  include(GoogleTest)
  gtest_discover_tests(forgedb_tests)
endif()

# Benchmarks
if(FORGEDB_BUILD_BENCHMARKS)
  add_executable(forgedb_benchmarks
    benchmarks/storage_benchmarks.cpp
  )
  target_link_libraries(forgedb_benchmarks PRIVATE forgedb_core benchmark::benchmark_main)
endif()
