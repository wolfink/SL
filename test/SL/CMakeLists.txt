add_executable(libtest tester.c)
target_link_libraries(libtest sllib)
include_directories(${SRC_DIR})

add_test(NAME test-vector COMMAND libtest vector)
add_test(NAME test-arena COMMAND libtest arena)
add_test(NAME test-string COMMAND libtest string)
