#include <cstdio>

#include "freertosxx/mutex.h"
#include "homeassistant/homeassistant.h"
#include "lwipxx/mqtt.h"
#include "pico/time.h"

using namespace homeassistant;

extern "C" void main_task(void* arg) {
  CommonDeviceInfo device_info("test_device");
  device_info.name = "test_device_name";
  device_info.component = "cover";
  device_info.device_class = "awning";

  JsonBuilder b;
  AddCommonInfo(device_info, b);
  AddCoverInfo(device_info, b);
  AddAvailabilityDiscovery(b);
  std::string discovery_message = std::move(b).Finish();
  printf("%s\n", discovery_message.c_str());

  lwipxx::MqttClient::ConnectInfo connect_info{
      .broker_address = std::string(MQTT_HOST),
      .client_id = "test_client",
      .user = MQTT_USER,
      .password = MQTT_PASSWORD,
  };
  SetAvailablityLwt(connect_info);

  std::string foobar("foobar");
  printf("%s %d\n", foobar.c_str(), foobar.size());

  auto maybe_mqtt_client = lwipxx::MqttClient::Create(connect_info);
  if (!maybe_mqtt_client) {
    panic("unable to create mqtt client");
  }
  lwipxx::MqttClient& mqtt_client = **maybe_mqtt_client;

  auto message_handler = [&](const lwipxx::MqttClient::Message& msg) {
    if (msg.data == cover_payloads::kOpenCommand) {
      printf("received open\n");
    } else if (msg.data == cover_payloads::kCloseCommand) {
      printf("received close\n");
    } else if (msg.data == cover_payloads::kStopCommand) {
      printf("received stop\n");
    }
  };

  std::string cmd_chan = AbsoluteChannel(device_info, topic_suffix::kCommand);
  if (ERR_OK !=
      mqtt_client.Subscribe(
          cmd_chan, lwipxx::MqttClient::kAtLeastOnce, message_handler)) {
    panic("subscribe error\n");
  }

  constexpr std::array<std::string_view, 4> states = {
      cover_payloads::kOpenState,
      cover_payloads::kOpeningState,
      cover_payloads::kClosingState,
      cover_payloads::kClosedState,
  };

  PublishAvailable(mqtt_client);

  if (ERR_OK != mqtt_client.Publish(
                    AbsoluteChannel(device_info, "config"),
                    discovery_message,
                    lwipxx::MqttClient::kAtLeastOnce,
                    true)) {
    panic("unable to publish discovery message\n");
  }

  // Cycle through the states.
  std::string state_chan = AbsoluteChannel(device_info, topic_suffix::kState);
  while (true) {
    for (auto state : states) {
      if (ERR_OK !=
          mqtt_client.Publish(
              state_chan, state, lwipxx::MqttClient::kBestEffort, true)) {
        printf("publish error\n");
      }
      sleep_ms(5000);
    }
  }

  vTaskDelete(nullptr);
}