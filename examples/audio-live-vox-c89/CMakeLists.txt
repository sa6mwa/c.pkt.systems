cmake_minimum_required(VERSION 3.21)

project(cpkt_audio_live_vox_c89_example LANGUAGES C)

find_package(CpktAudio CONFIG REQUIRED)

add_executable(cpkt_audio_live_vox_c89_example main.c)
target_compile_options(cpkt_audio_live_vox_c89_example PRIVATE
  -std=c89
  -Wall
  -Wextra
  -Wpedantic)
target_link_libraries(cpkt_audio_live_vox_c89_example PRIVATE cpkt::audio)
