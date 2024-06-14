

#include "lwipxx/mqtt.h"

#include "pico/platform.h"
#include "pico/time.h"
#include "util/include/util/ssprintf.h"

using freertosxx::EventGroup;
using jagspico::ssprintf;
using lwipxx::MqttClient;

MqttClient::ConnectInfo CommonConnectInfo(int client_id) {
  return {
      .broker_address = MQTT_HOST,
      .client_id = ssprintf("lwipxx_test%d", client_id),
      .user = MQTT_USER,
      .password = MQTT_PASSWORD,
      .lwt_topic = ssprintf("/lwipxx_test/lwt%d", client_id),
      .lwt_message = "unavailable",
      .lwt_qos = MqttClient::Qos::kAtLeastOnce,
      .lwt_retain = true,
  };
}

extern "C" void main_task(void* args) {
  auto c1 = *lwipxx::MqttClient::Create(CommonConnectInfo(1));
  auto c2 = *lwipxx::MqttClient::Create(CommonConnectInfo(2));

  EventGroup evt;
  if (ERR_OK != c2->Subscribe(
                    "/lwipxx_test/chan1",
                    MqttClient::Qos::kAtLeastOnce,
                    [&](const MqttClient::Message& message) {
                      printf(
                          "Client 2 received message on %.*s: %.*s\n",
                          (int)message.topic.size(),
                          message.topic.data(),
                          (int)message.data.size(),
                          message.data.data());
                      evt.Set(1);
                    })) {
    panic("whaaa\n");
  }
  if (ERR_OK != c1->Publish(
                    "/lwipxx_test/chan1",
                    "Hello, world!",
                    MqttClient::Qos::kAtLeastOnce,
                    false)) {
    panic("yikes!\n");
  };
  evt.Wait(1, {.clear = true});
  printf("PASS\n");
  while (true) {
    sleep_ms(10000);
  }
}