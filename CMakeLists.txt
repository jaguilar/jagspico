cmake_minimum_required(VERSION 3.25)
include(FetchContent)

FetchContent_Declare(
  pico_sdk
  GIT_REPOSITORY https://github.com/raspberrypi/pico-sdk
  GIT_TAG        master
)
FetchContent_Declare(
  freertos_kernel
  GIT_REPOSITORY https://github.com/FreeRTOS/FreeRTOS-Kernel.git
  GIT_TAG        V11.0.1
)
FetchContent_Declare(
  freertos_default_config
  GIT_REPOSITORY https://github.com/jaguilar/freertos_default_config.git
  GIT_TAG        main
)
if (NOT PICO_SDK_PATH)
  FetchContent_MakeAvailable(pico_sdk)
  set(PICO_SDK_PATH ${pico_sdk_SOURCE_DIR})
endif()
if(NOT TARGET pico_stdlib)
  include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
endif()
if (NOT TARGET freertos_config)
  FetchContent_MakeAvailable(freertos_default_config)
endif()
if (NOT FREERTOS_KERNEL_PATH)
  set(FREERTOS_PORT GCC_RP2040 CACHE STRING "")
  FetchContent_Populate(freertos_kernel)
  set(FREERTOS_KERNEL_PATH ${freertos_kernel_SOURCE_DIR})
  include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)
endif()

project(cd74hc595 VERSION 0.0.1 LANGUAGES C CXX ASM)

if (NOT target pico_stdlib)
  pico_sdk_init()
endif()

add_subdirectory(src)
