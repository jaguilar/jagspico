add_library(lwipxx_mqtt mqtt.cc)
target_link_libraries(lwipxx_mqtt PUBLIC common pico_lwip_mqtt freertosxx pico_lwip_freertos pico_lwip_arch lwip)
target_compile_features(lwipxx_mqtt PUBLIC cxx_std_23)
target_include_directories(lwipxx_mqtt PUBLIC include)

add_pico_executable(lwipxx_mqtt_test mqtt_test.cc)
target_link_libraries(lwipxx_mqtt_test PRIVATE lwipxx_mqtt freertosxx common)
target_compile_definitions(lwipxx_mqtt_test PUBLIC -DMQTT_HOST="$ENV{MQTT_HOST}" -DMQTT_USER="$ENV{MQTT_USER}" -DMQTT_PASSWORD="$ENV{MQTT_PASSWORD}")