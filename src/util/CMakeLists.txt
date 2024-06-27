add_library(jagspico_util cleanup.cc ssprintf.cc)
target_link_libraries(jagspico_util PUBLIC pico_stdlib)
target_include_directories(jagspico_util PUBLIC include)

add_pico_executable(ssprintf_test ssprintf_test.cc)
target_link_libraries(ssprintf_test PRIVATE jagspico_util pico_stdlib)
