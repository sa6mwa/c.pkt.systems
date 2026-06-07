cmake_minimum_required(VERSION 3.21)

project(cpkt_bundle_cmake_consumer LANGUAGES C)

set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(CURL CONFIG REQUIRED)

add_executable(cpkt_bundle_cmake_consumer ../abi_smoke.c)
target_compile_options(cpkt_bundle_cmake_consumer PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(cpkt_bundle_cmake_consumer PRIVATE CURL::libcurl)
