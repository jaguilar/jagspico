cmake_minimum_required(VERSION 3.25)
include(FetchContent)

FetchContent_Declare(
  picobase
  GIT_REPOSITORY https://github.com/jaguilar/picobase.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(picobase)

project(cd74hc595 VERSION 0.0.1 LANGUAGES C CXX ASM)

pico_sdk_init()

add_subdirectory(src)
