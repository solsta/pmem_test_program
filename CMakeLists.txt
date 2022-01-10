cmake_minimum_required(VERSION 3.21)
project(simple_pmem_program C)

set(CMAKE_C_STANDARD 11)
link_libraries(pmemobj criu jemalloc)

add_executable(simple_pmem_program main.c)
