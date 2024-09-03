cmake_minimum_required(VERSION 3.10)
project(StructuredLanguage C)

include(CTest)

option(TEST_ALL "Run all tests" ON)

set(SASS_VERSION "0.0.1")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_FLAGS_DEBUG "-ggdb -g3")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(src)

add_executable(sl ${SRC_DIR}/sl.c)

if(TEST_ALL)
  add_subdirectory(test)
endif()
