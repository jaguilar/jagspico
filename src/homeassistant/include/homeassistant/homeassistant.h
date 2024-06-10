#ifndef JAGSPICO_HA_H
#define JAGSPICO_HA_H

#include <pico/printf.h>

#include <array>
#include <cstdio>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <string_view>

#include "lwipxx/mqtt.h"
#include "util/ssprintf.h"

namespace homeassistant {

class JsonBuilder {
 public:
  JsonBuilder(std::string& json) : json_(json) {}

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

  std::string& json_;
  bool want_sep = false;
};


class Device {
 public:
  // Returns a discovery message for the device.
  virtual std::string GetDiscoveryMessage() const = 0;

  // The component type (e.g. "cover", "sensor").
  virtual std::string_view component() const = 0;

  // The ID of this device. We do not take a copy of the ID, it must
  // outlive this object. Can be either "<object_id>" or
  // "<node_id>/<object_id>".
  std::string_view id() { return id_; }
  void set_id(std::string_view v) { id_ = v; }

  std::string Path() {
#if USE_PICO_PRINTF == 1
    std::string s;
    printf(
        s.data(),
        "%.*s/%.*s",
        component().size(),
        component().data(),
        id().size(),
        id().data());
    return s;
#elif USE_SSPRINTF == 1
    return jagspico::ssprintf("%s/%s", component().data(), id().data());
#else
    std::ostringstream ss;
    ss << component() << "/" << id();
    return std::move(ss).str();
#endif
  }

  // A list of suffixes to which to subscribe. These suffixes of
  // homeassistant/<component>/<id>/ will be subscribed to and
  // messages sent to these paths will be delivered to this object.
  // Each of these suffixes should appear somewhere in the discovery message.
  virtual std::span<const std::string_view> suffixes() const = 0;

 protected:
  // The ID of this device.
  std::string_view id_;
};

class Cover : public Device {
 public:
  std::string GetDiscoveryMessage() const override { return ""; }
  std::string_view component() const override { return "cover"; }
  std::span<const std::string_view> suffixes() const override {
    static constexpr std::array<std::string_view, 3> kSuffixes = {
        "set", "state", "availability"};
    return kSuffixes;
  }

  static constexpr std::string_view kCommandSuffix = "command";

  enum State {
    kOpen = 0b00001,
    kOpening = 0b00010,
    kClosing = 0b00100,
    kClosed = 0b01000,
    kStopped = 0b10000,
  };
  void SetState(State s);

  State state() const { return state_; }

 private:
  State state_ = kClosed;
};

// The Somfy Awning home assistant service. Since I have just the one awning,
// this service only announces a single device. It could easily be improved to
// do more, but I won't!
//
// This will announce the device to the home assistant before connecting. If
// there is an error during the announcement phase, an error will be returned
// and the service will be destroyed. Otherwise,
class HomeAssistantService {
 public:
  // open == down closed == up
  enum Command { kCmdOpen = 0b001, kCmdClose = 0b010, kCmdStop = 0b100 };
  using SendBlindsCommand = std::function<void(Command)>;

  static std::expected<std::unique_ptr<HomeAssistantService>, err_t> Connect(
      lwipxx::MqttClient& mqtt_client, SendBlindsCommand send_command) {
    return std::unique_ptr<HomeAssistantService>{
        new HomeAssistantService(mqtt_client, send_command)};
  }

  enum State {
    kOpen = 0b00001,
    kOpening = 0b00010,
    kClosing = 0b00100,
    kClosed = 0b01000,
    kStopped = 0b10000,
  };
  // Notifies the service that the cover state has changed.
  void SetState(State s);

 private:
  HomeAssistantService(
      lwipxx::MqttClient& mqtt_client, SendBlindsCommand send_command);

  static std::string_view StateToPayload(State state);

  void Announce();

  void SendDiscovery();
  void SendAvailable();
  void SendState();

  void ReceiveBirth(const lwipxx::MqttClient::Message& message);
  void ReceiveCommand(const lwipxx::MqttClient::Message& message);

  lwipxx::MqttClient& client_;
  SendBlindsCommand send_command_;
  State cover_state_;
};

}  // namespace homeassistant

#endif  // JAGSPICO_HA_H