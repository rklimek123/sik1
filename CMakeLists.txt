cmake_minimum_required (VERSION 3.1)

set(CMAKE_C_FLAGS "-g -Wall")

add_library(http http.c)
add_library(file file.c)
add_executable(serwer serwer.c)
target_link_libraries(serwer http)
target_link_libraries(serwer file)

install(TARGETS DESTINATION .)