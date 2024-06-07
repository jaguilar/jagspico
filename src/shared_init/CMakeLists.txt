add_library(freertos_default freertos_common.cc)
target_link_libraries(freertos_default
        PUBLIC
        FreeRTOS-Kernel
        freertos_config
        FreeRTOS-Kernel-Heap4
        pico_sync
        pico_stdlib
)
target_compile_options(freertos_default INTERFACE -DUSE_FREERTOS=1)

if("${PICO_BOARD}" STREQUAL "pico_w")
  add_library(lwip INTERFACE)
  target_link_libraries(lwip
          INTERFACE
          lwip_config pico_cyw43_arch_lwip_sys_freertos pico_lwip_api
          pico_lwip_core pico_lwip_freertos pico_lwip_mdns)
else()  
  add_library(lwip INTERFACE)
endif()

add_library(common STATIC shared_init.cc)
target_link_libraries(common PUBLIC
    freertos_default
    lwip
    pico_stdlib
    pico_sync
)
target_compile_options(
    common 
    PRIVATE -DWIFI_PASSWORD="$ENV{WIFI_PASSWORD}" -DWIFI_SSID="$ENV{WIFI_SSID}" -DCYW43_HOST_NAME="${WIFI_HOSTNAME}")

add_library(common_nonet STATIC shared_init.cc)
target_link_libraries(common_nonet PUBLIC
    freertos_default
    pico_stdlib
    pico_sync
)
if("${PICO_BOARD}" STREQUAL "pico_w")
  target_link_libraries(common_nonet PUBLIC pico_cyw43_arch_none)    
endif()

function(add_pico_executable name)
  add_executable(${name} ${ARGN})
  pico_add_extra_outputs(${name})
  pico_enable_stdio_usb(${name} 0)
  pico_enable_stdio_uart(${name} 1)
endfunction()
