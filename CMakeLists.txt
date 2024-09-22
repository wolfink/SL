cmake_minimum_required(VERSION 3.10)
project(StructuredLanguage C)

include(CTest)

option(TEST_ALL "Run all tests" ON)

set(SL_VERSION "0.0.1")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_FLAGS_DEBUG "-ggdb -g3")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
#include_directories(src)

file(GLOB SL_FILES ${SRC_DIR}/SL/*.c)

add_library(sllib OBJECT ${SL_FILES})

add_executable(slc ${SRC_DIR}/slc.c)
target_link_libraries(slc sllib)

if(TEST_ALL)
  add_subdirectory(test)
endif()
