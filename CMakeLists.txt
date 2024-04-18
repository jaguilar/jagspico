cmake_minimum_required(VERSION 3.25)
include(FetchContent)

FetchContent_Declare(
  picobase
  GIT_REPOSITORY https://github.com/jaguilar/picobase.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(picobase)
include(${picobase_SOURCE_DIR}/basic_imports.cmake)

project(jagspico VERSION 0.0.1 LANGUAGES C CXX ASM)

if (PROJECT_IS_TOP_LEVEL)
  pico_sdk_init()
endif()

add_subdirectory(src)
