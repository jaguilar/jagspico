#ifndef JAGSPICO_HA_H
#define JAGSPICO_HA_H

#include <pico/printf.h>

#include <array>
#include <cstdio>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "freertosxx/mutex.h"
#include "lwip/apps/mqtt.h"
#include "lwipxx/mqtt.h"
#include "pico/platform.h"
#include "pico/unique_id.h"
#include "util/cleanup.h"
#include "util/ssprintf.h"

namespace homeassistant {

class JsonBuilder {
 public:
  JsonBuilder() { json_.append("{"); }

  std::string Finish() && {
    json_.append("}");
    return std::move(json_);
  }

  void Kv(std::string_view key, std::string_view value) {
    Key(key);
    json_.append("\"");
    json_.append(value);
    json_.append("\"");
    want_sep = true;
  }

  void Kv(std::string_view key, double number) {
    Key(key);
    json_.append(std::to_string(number));
    want_sep = true;
  }

  void Kv(std::string_view key, const char* text) {
    Kv(key, std::string_view(text));
  }

  void Kv(std::string_view key, bool b) {
    Key(key);
    json_.append(b ? "true" : "false");
    want_sep = true;
  }

  template <typename T>
  void KvIf(std::string_view key, const std::optional<T>& value) {
    if (value) Kv(key, *value);
  }

  auto EnterDict(std::string_view key) {
    Key(key);
    json_.append("{");
    want_sep = false;
    return jagspico::Cleanup([&] { ExitDict(); });
  }

  void ExitDict() {
    json_.append("}");
    want_sep = true;
  }

 private:
  void Key(std::string_view key) {
    if (want_sep) json_.append(", ");
    json_.append("\"");
    json_.append(key);
    json_.append("\": ");
  }

  std::string json_;
  bool want_sep = false;
};

std::string_view AvailabilityTopic();
void SetAvailablityLwt(lwipxx::MqttClient::ConnectInfo& info);

// Adds availability information
void AddAvailabilityDiscovery(JsonBuilder& json);

// Publishes an availability message on this device's availability topic.
void PublishAvailable(lwipxx::MqttClient& client);

struct CommonDeviceInfo {
  explicit CommonDeviceInfo(std::string_view unique_id)
      : unique_id(unique_id) {}

  std::string_view unique_id;

  // The name of the device.
  std::optional<std::string_view> name;

  // E.g. "cover" "sensor" etc.
  std::optional<std::string_view> component;

  // E.g. "awning" "door" "battery" "humidity" etc.
  std::optional<std::string_view> device_class;
};

// Publishes a retained discovery message about this device.
void PublishDiscovery(
    lwipxx::MqttClient& client, const CommonDeviceInfo& device_info,
    std::string_view discovery_message);

void AddCommonInfo(const CommonDeviceInfo& info, JsonBuilder& builder);

std::string DeviceRootTopic(const CommonDeviceInfo& info);

// DeviceRootTopic(info) "/" suffix
std::string AbsoluteChannel(
    const CommonDeviceInfo& info, std::string_view suffix);
std::string RelativeChannel(std::string_view suffix);

void AddCoverInfo(const CommonDeviceInfo& info, JsonBuilder& builder);
void AddSensorInfo(
    const CommonDeviceInfo& info,
    std::optional<std::string_view> unit_of_measurement, JsonBuilder& builder);

namespace topic_suffix {
constexpr std::string_view kDiscovery = "config";
constexpr std::string_view kCommand = "cmd";
constexpr std::string_view kState = "sta";
}  // namespace topic_suffix

namespace cover_payloads {
constexpr std::string_view kOpenCommand = "o";
constexpr std::string_view kCloseCommand = "c";
constexpr std::string_view kStopCommand = "s";
constexpr std::string_view kOpenState = "o";
constexpr std::string_view kOpeningState = "p";
constexpr std::string_view kClosingState = "c";
constexpr std::string_view kClosedState = "l";
constexpr std::string_view kStoppedState = "s";
}  // namespace cover_payloads

}  // namespace homeassistant

#endif  // JAGSPICO_HA_H