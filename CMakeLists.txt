cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

project(ruzzle_client)

add_executable(
    ruzzle_client
    src/main.c
)

target_link_libraries(ruzzle_client ws2_32)
