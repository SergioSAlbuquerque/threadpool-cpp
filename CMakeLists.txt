cmake_minimum_required(VERSION 3.16)
project(threadpool_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(Threads REQUIRED)

# Biblioteca header-only
add_library(threadpool INTERFACE)
target_include_directories(threadpool INTERFACE include)
target_link_libraries(threadpool INTERFACE Threads::Threads)

add_executable(benchmark examples/benchmark.cpp)
target_link_libraries(benchmark PRIVATE threadpool)

enable_testing()
add_executable(pool_tests tests/test_main.cpp)
target_link_libraries(pool_tests PRIVATE threadpool)
add_test(NAME pool_tests COMMAND pool_tests)
