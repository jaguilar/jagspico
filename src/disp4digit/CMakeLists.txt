add_library(disp4digit disp4digit.cc)
target_link_libraries(disp4digit PUBLIC hardware_gpio hardware_pio driver_cd74hc595 freertos_default)
target_include_directories(disp4digit PUBLIC ..)
