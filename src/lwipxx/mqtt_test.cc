

#include "lwipxx/mqtt.h"

#include <cstdio>

#include "lwip/err.h"
#include "pico/platform.h"
#include "pico/time.h"
#include "projdefs.h"
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
    panic("chan1 subscribe failed\n");
  }
  if (ERR_OK !=
      c2->Subscribe(
          "/lwipxx_test/chan",
          MqttClient::Qos::kAtLeastOnce,
          [&](const MqttClient::Message& message) { evt.Set(0b10); })) {
    panic("chan subscribe failed\n");
  }
  if (ERR_OK !=
      c2->Subscribe(
          "/lwipxx_test/chan11",
          MqttClient::Qos::kAtLeastOnce,
          [&](const MqttClient::Message& message) { evt.Set(0b10); })) {
    panic("chan subscribe failed\n");
  }
  if (ERR_OK != c1->Publish(
                    "/lwipxx_test/chan1",
                    "Hello, world!",
                    MqttClient::Qos::kAtLeastOnce,
                    false)) {
    panic("chan1 publish failed!\n");
  };
  evt.Wait(1, {.clear = true});
  if (ERR_OK != c2->Unsubscribe("/lwipxx_test/chan1")) {
    panic("unsub failed\n");
  }
  // A message sent now won't trigger the callback.
  if (ERR_OK != c1->Publish(
                    "/lwipxx_test/chan1",
                    "Hello, world!",
                    MqttClient::Qos::kAtLeastOnce,
                    false)) {
    panic("yikes!\n");
  };
  evt.Wait(1, {.clear = true, .timeout = 2500});
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
    panic("chan1 subscribe 2 failed\n");
  }
  printf("waiting for publish to come through\n");
  if (evt.Wait(1, {.clear = true, .timeout = pdMS_TO_TICKS(2500)}) != 1) {
    printf("at least once publish didn't make it back to us!\n");
  }

  std::string big_message(10000, 'a');
  if (ERR_OK != c1->Publish(
                    "/lwipxx_test/chan1",
                    big_message,
                    MqttClient::Qos::kAtLeastOnce,
                    false)) {
    // This is expected to fail unless you set a large message buffer size.
    printf("big message publish failed!\n");
  } else {
    evt.Wait(1, {.clear = true});
  }

  c2.reset();
  if (ERR_OK !=
      c1->Publish(
          "/lwipxx_test/chan1", "asdf", MqttClient::Qos::kAtLeastOnce, false)) {
    panic("publish after c2 destroyed failed!\n");
  };

  if (evt.Get() & 0b10) {
    printf(
        "unexpected messages received on subscribed channels with no pubs\n");
  }

  printf("PASS\n");
  while (true) {
    sleep_ms(10000);
  }
}