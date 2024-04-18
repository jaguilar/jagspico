add_library(common STATIC shared_init.c)
target_link_libraries(common PUBLIC
    freertos_config
    FreeRTOS-Kernel-Heap4
    lwip
    pico_stdlib
    pico_sync
)
target_compile_options(
    common 
    PRIVATE -DWIFI_PASSWORD="${WIFI_PASSWORD}" -DWIFI_SSID="${WIFI_SSID}" -DCYW43_HOST_NAME="picotest")

add_library(common_nonet STATIC shared_init.c)
target_link_libraries(common_nonet PUBLIC 
    freertos_config
    FreeRTOS-Kernel-Heap4
    pico_stdlib
    pico_sync
    pico_cyw43_arch_none
)

function(add_pico_executable name)
  add_executable(${name} ${ARGN})
  pico_add_extra_outputs(${name})
  pico_enable_stdio_usb(${name} 0)
  pico_enable_stdio_uart(${name} 1)
endfunction()
