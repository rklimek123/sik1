cmake_minimum_required (VERSION 3.1)

set(CMAKE_C_FLAGS "-g -Wall")

add_library(co_servers co_servers.c)
add_library(file file.c)
add_library(http http.c)
add_executable(serwer serwer.c)
target_link_libraries(serwer co_servers)
target_link_libraries(serwer file)
target_link_libraries(serwer http)

install(TARGETS DESTINATION .)