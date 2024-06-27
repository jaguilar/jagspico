add_library(homeassistant homeassistant.cc)
target_compile_features(homeassistant PRIVATE cxx_std_23)
target_link_libraries(homeassistant PUBLIC lwipxx_mqtt jagspico_util pico_unique_id)
target_include_directories(homeassistant PUBLIC include)

add_pico_executable(homeassistant_test test.cc)
target_link_libraries(homeassistant_test PRIVATE homeassistant)
target_compile_options(
  homeassistant_test PRIVATE 
  -DMQTT_HOST="$ENV{MQTT_HOST}"
  -DMQTT_USER="$ENV{MQTT_USER}" 
  -DMQTT_PASSWORD="$ENV{MQTT_PASSWORD}")