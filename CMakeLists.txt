cmake_minimum_required(VERSION 3.21)
include(FetchContent)

FetchContent_Declare(
  picobase
  GIT_REPOSITORY https://github.com/jaguilar/picobase.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(picobase)
include(${picobase_SOURCE_DIR}/basic_imports.cmake)

project(jagspico VERSION 0.0.1 LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if (PROJECT_IS_TOP_LEVEL)
  pico_sdk_init()
endif()

add_subdirectory(src)
