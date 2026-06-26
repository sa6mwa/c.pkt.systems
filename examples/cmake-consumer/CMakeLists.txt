cmake_minimum_required(VERSION 3.21)

project(cpkt_bundle_cmake_consumer LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(CURL CONFIG REQUIRED)
find_package(libxml2 CONFIG REQUIRED)
find_package(Lua CONFIG REQUIRED)
find_package(open62541 CONFIG REQUIRED)

add_executable(cpkt_bundle_cmake_consumer ../abi_smoke.c)
target_compile_options(cpkt_bundle_cmake_consumer PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(cpkt_bundle_cmake_consumer PRIVATE CURL::libcurl LibXml2::LibXml2 Lua::Lua open62541::open62541)
