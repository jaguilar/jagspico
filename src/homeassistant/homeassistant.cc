#include "homeassistant/homeassistant.h"

#include <string_view>

#include "lwipxx/mqtt.h"
#include "pico/platform.h"
#include "pico/time.h"
#include "util/ssprintf.h"

namespace homeassistant {

static constexpr std::string_view kOnlinePayload = "online";
static constexpr std::string_view kOfflinePayload = "offline";

std::string_view AvailabilityTopic() {
  static const std::string* const kAvailabilityTopic = [] {
    pico_unique_board_id_t id;
    pico_get_unique_board_id(&id);
    const uint64_t unique_id = *reinterpret_cast<uint64_t*>(id.id);
    return new std::string(
        jagspico::ssprintf("devices/%0llx/available", unique_id));
  }();
  return *kAvailabilityTopic;
}

void SetAvailablityLwt(lwipxx::MqttClient::ConnectInfo& info) {
  info.lwt_topic = AvailabilityTopic();
  info.lwt_message = kOfflinePayload;
  info.lwt_qos = lwipxx::MqttClient::Qos::kBestEffort;
  info.lwt_retain = true;
}

void AddAvailabilityDiscovery(JsonBuilder& json) {
  auto dict_closer = json.EnterDict("availability");
  json.Kv("topic", AvailabilityTopic());
  json.Kv("payload_available", kOnlinePayload);
  json.Kv("payload_not_available", kOfflinePayload);
}

std::string DeviceRootTopic(const CommonDeviceInfo& info) {
  return jagspico::ssprintf(
      "homeassistant/%s/%s", info.component->data(), info.unique_id.data());
}

std::string AbsoluteChannel(
    const CommonDeviceInfo& info, std::string_view suffix) {
  std::string topic = DeviceRootTopic(info);
  topic.append("/");
  topic.append(suffix);
  return topic;
}

std::string RelativeChannel(std::string_view suffix) {
  return "~/" + std::string(suffix);
}

void PublishAvailable(lwipxx::MqttClient& client) {
  while (ERR_OK != client.Publish(
                       AvailabilityTopic(),
                       kOnlinePayload,
                       lwipxx::MqttClient::Qos::kBestEffort,
                       true)) {
    printf("unable to publish initial availability message, retrying\n");
    sleep_ms(5000);
  }
}

void PublishDiscovery(
    lwipxx::MqttClient& client, const CommonDeviceInfo& device_info,
    std::string_view discovery_message) {
  while (ERR_OK != client.Publish(
                       AbsoluteChannel(device_info, topic_suffix::kDiscovery),
                       discovery_message,
                       lwipxx::MqttClient::kAtLeastOnce,
                       true)) {
    printf("unable to publish discovery message, retrying\n");
    sleep_ms(5000);
  }
}

void AddCommonInfo(const CommonDeviceInfo& info, JsonBuilder& builder) {
  builder.Kv("~", DeviceRootTopic(info));
  builder.KvIf("name", info.name);
  builder.Kv("unique_id", info.unique_id);
  builder.KvIf("device_class", info.device_class);
}

void AddCoverInfo(const CommonDeviceInfo& info, JsonBuilder& builder) {
  builder.Kv("command_topic", RelativeChannel(topic_suffix::kCommand));
  builder.Kv("state_topic", RelativeChannel(topic_suffix::kState));
  builder.Kv("payload_open", cover_payloads::kOpenCommand);
  builder.Kv("payload_close", cover_payloads::kCloseCommand);
  builder.Kv("payload_stop", cover_payloads::kStopCommand);
  builder.Kv("state_open", cover_payloads::kOpenState);
  builder.Kv("state_opening", cover_payloads::kOpeningState);
  builder.Kv("state_closed", cover_payloads::kClosedState);
  builder.Kv("state_closing", cover_payloads::kClosingState);
  builder.Kv("state_stopped", cover_payloads::kStoppedState);
  builder.Kv("optimistic", false);
  builder.Kv("retain", true);
}

void AddSensorInfo(
    const CommonDeviceInfo& info,
    std::optional<std::string_view> unit_of_measurement, JsonBuilder& builder) {
  builder.Kv("state_topic", RelativeChannel(topic_suffix::kState));
  if (unit_of_measurement) {
    builder.Kv("unit_of_measurement", *unit_of_measurement);
  }
  builder.Kv("force_update", true);
  builder.Kv("state_class", "measurement");
}

}  // namespace homeassistant