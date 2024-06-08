add_library(jagspico_util cleanup.cc ssprintf.cc)
target_link_libraries(jagspico_util)
target_include_directories(jagspico_util PUBLIC include)
