cmake_minimum_required (VERSION 2.6)
project (leptjson_test C)

add_library(leptjson leptjson.c)
add_executable(leptjson_test mine_test.c)
target_link_libraries(leptjson_test leptjson)
