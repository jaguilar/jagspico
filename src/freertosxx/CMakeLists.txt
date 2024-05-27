add_library(freertosxx queue.cc mutex.cc event.cc)
target_link_libraries(freertosxx PRIVATE FreeRTOS-Kernel freertos_config)
target_include_directories(freertosxx PUBLIC include)
