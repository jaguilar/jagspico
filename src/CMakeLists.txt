include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(shared_init)

add_subdirectory(arduino)
add_subdirectory(cd74hc595)
add_subdirectory(disp4digit)
add_subdirectory(freertosxx)
add_subdirectory(homeassistant)
add_subdirectory(lwipxx)
add_subdirectory(util)