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

  std::string json_;
  bool want_sep = false;
};

class Publisher {
 public:
  // Tells the publisher to publish a message on the given topic.
  virtual void Publish(
      std::string_view topic, std::string_view payload,
      lwipxx::MqttClient::Qos qos, bool retain) = 0;
};

// Devices must be protected against simultaneous access between whatever sets
// their state on the microcontroller side and the HomeAssistantService, because
// the service may request state republication during the announcement
// procedure.
class Device {
 public:
  virtual ~Device() = default;

  // Appends to builder the discovery fields for this device.
  // open and close brace. The discovery message must not define the "~"
  // property. The discovery message must reference all topics relative to
  // the "~" base path.
  virtual void AppendDiscovery(JsonBuilder& builder) const = 0;

  // Returns the path prefix of all topics related to this device,
  // relative to the base homeassistant topic prefix.
  // For example, if the full topic path for a device state channel is
  // "homeassistant/cover/deviceid/state_topic", then this function should
  // return "cover/deviceid".
  virtual std::string_view BaseTopic() const = 0;

  // A list of topics to which to subscribe. The topics subscribed to are
  // relative to ~/. Any messages sent to these topics will be passed to
  // ReceiveMessage().
  virtual std::span<const std::string_view> SubscribeTo() const = 0;

  // Receives a message to one of our subscribed topics. topic_index is
  // the index of the topic within the SubscribeTo topic list which this
  // message corresponds to.
  virtual void ReceiveMessage(int topic_index, std::string_view payload) = 0;

  // Informs the device of the publisher it should use to send messages to Mqtt.
  virtual void SetPublisher(Publisher* publisher) = 0;

  // Publishes the Device's state. Called when the device is announced. The
  // state may also be published manually in response to state changes.
  virtual void PublishState() = 0;
};

class DeviceBase : public Device {
 public:
  struct CommonDeviceInfo {
    explicit CommonDeviceInfo(std::string_view unique_id)
        : unique_id(unique_id) {}

    std::string_view unique_id;

    std::optional<std::string_view> name;
    std::optional<std::string_view> component;
    std::optional<std::string_view> device_class;

    // Do we have availability info for this device? If not, the device will
    // always appear as available to HomeAssistant.
    bool always_available = true;
  };

  DeviceBase(
      CommonDeviceInfo common_info,
      std::span<const std::string_view> subscribed_suffixes)
      : common_info_(common_info), subscribed_suffixes_(subscribed_suffixes) {
    const auto& unique_id = common_info_.unique_id;
    if (common_info_.component) {
      const auto& component = *common_info_.component;
      base_topic_ = jagspico::ssprintf(
          "%.*s/%.*s",
          component.size(),
          component.data(),
          unique_id.size(),
          unique_id.data());
    } else {
      base_topic_ =
          jagspico::ssprintf("%.*s", unique_id.size(), unique_id.data());
    }
  }

  ~DeviceBase() override = default;

  std::string_view BaseTopic() const override { return base_topic_; }
  std::span<const std::string_view> SubscribeTo() const override {
    return subscribed_suffixes_;
  }

  void AppendDiscovery(JsonBuilder& builder) const final {
    AppendCommonDiscoveryEntries(builder);
    AppendSubtypeDiscoveryEntries(builder);
  }

  // set_publisher is a separate function, rather than a constructor
  // parameter, in order to break mutual dependence between the DeviceBase and
  // the HomeassistantService.
  void SetPublisher(Publisher* publisher) final { publisher_ = publisher; }

  // Commonly used constants.
  static constexpr std::string_view kCommandTopic = "~/set";
  static constexpr std::string_view kStateTopic = "~/state";
  static constexpr std::string_view kAvailabilityTopic = "~/avail";
  static constexpr std::string_view kOnlinePayload = "online";
  static constexpr std::string_view kOfflinePayload = "offline";

 protected:
  virtual void AppendSubtypeDiscoveryEntries(JsonBuilder& json) const;

  void SetAvailable(bool available) {
    if (common_info_.always_available) return;
    Publish(
        lwipxx::MqttClient::Qos::kAtLeastOnce,
        false,
        kAvailabilityTopic,
        available ? kOnlinePayload : kOfflinePayload);
  }

  void Publish(
      lwipxx::MqttClient::Qos qos, bool retain, std::string_view topic,
      std::string_view payload) {
    publisher_->Publish(topic, payload, qos, retain);
  }

 private:
  void AppendCommonDiscoveryEntries(JsonBuilder& json) const {
    json.KvIf("name", common_info_.name);
    json.Kv("unique_id", common_info_.unique_id);
    json.KvIf("device_class", common_info_.device_class);
    if (!common_info_.always_available) {
      auto dict_closer = json.EnterDict("availability");
      json.Kv("topic", kAvailabilityTopic);
      json.Kv("payload_available", kOnlinePayload);
      json.Kv("payload_not_available", kOfflinePayload);
    }
  }

  // The ID of this device.
  CommonDeviceInfo common_info_;
  std::string base_topic_;
  std::span<const std::string_view> subscribed_suffixes_;
  Publisher* publisher_ = nullptr;
};

class Cover : public DeviceBase {
 public:
  static constexpr std::array<std::string_view, 1> kSubscribedTopics = {
      kCommandTopic};

  enum class Command : int {
    kOpen = 0b001,
    kClose = 0b010,
    kStop = 0b100,
  };
  using CommandHandler = std::function<void(Command)>;

  Cover(CommonDeviceInfo info, CommandHandler handler)
      : DeviceBase(info, kSubscribedTopics), command_handler_(handler) {}

  void AppendSubtypeDiscoveryEntries(JsonBuilder& json) const override {
    json.Kv("command_topic", kCommandTopic);
    json.Kv("state_topic", kStateTopic);
    json.Kv("payload_open", kOpenCommand);
    json.Kv("payload_close", kCloseCommand);
    json.Kv("payload_stop", kStopCommand);
    json.Kv("state_open", kOpenState);
    json.Kv("state_opening", kOpeningState);
    json.Kv("state_closed", kClosedState);
    json.Kv("state_closing", kClosingState);
    json.Kv("state_stopped", kStoppedState);
    json.Kv("optimistic", false);
    json.Kv("retain", true);
  }

  enum class State {
    kOpen = 0b00001,
    kOpening = 0b00010,
    kClosing = 0b00100,
    kClosed = 0b01000,
    kStopped = 0b10000,
  };
  void SetState(State s) {
    state_ = s;
    PublishState();
  }

 private:
  static constexpr std::string_view kOpenCommand = "open";
  static constexpr std::string_view kCloseCommand = "close";
  static constexpr std::string_view kStopCommand = "stop";
  static constexpr std::string_view kOpenState = "open";
  static constexpr std::string_view kOpeningState = "opening";
  static constexpr std::string_view kClosingState = "closing";
  static constexpr std::string_view kClosedState = "closed";
  static constexpr std::string_view kStoppedState = "closed";

  static std::string_view StateToPayload(State s) {
    switch (s) {
      case State::kOpen:
        return kOpenState;
      case State::kOpening:
        return kOpeningState;
      case State::kClosing:
        return kClosingState;
      case State::kClosed:
        return kClosedState;
      case State::kStopped:
        return kStoppedState;
    }
    __builtin_unreachable();
  }

  void PublishState() {
    Publish(
        lwipxx::MqttClient::kAtLeastOnce,
        false,
        kStateTopic,
        StateToPayload(state_));
  }

  State state() const { return state_; }

 private:
  void ReceiveMessage(int topic_index, std::string_view payload) override {
    if (payload.starts_with(kOpenCommand)) {
      command_handler_(Command::kOpen);
    } else if (payload.starts_with(kCloseCommand)) {
      command_handler_(Command::kClose);
    } else if (payload.starts_with(kStopCommand)) {
      command_handler_(Command::kStop);
    }
    printf("unexpected command payload: %s\n", payload.data());
  }

  std::string_view id_;
  std::string_view device_class_;
  State state_ = State::kClosed;
  CommandHandler command_handler_;
};

// A HomeAssistant service. Register Device instances with this service to have
// them talk to HomeAssistant.
//
// Life of a device:
// 1. Create the device and connect it to any objects that receive commands from
//    it.
//    * Warning: discover the device's state *before* registering it with the
//      HomeAssistantService. Otherwise we may send the wrong information to
//      HomeAssistant during the initial announcement.
// 2. Register the device with the HomeAssistantService.
// 3. HomeAssistantService gets Device discovery message and announces the
// device to MQTT.
// 4. HomeAssistantService subscribes to device's SubscribeTo topics.
//
//
class HomeAssistantService : public Publisher {
 public:
  static const char* GetAvailabilityTopic() {
    const std::string* kAvailabilityTopic = [] {
      pico_unique_board_id_t id;
      pico_get_unique_board_id(&id);
      const uint64_t unique_id = *reinterpret_cast<uint64_t*>(id.id);
      return new std::string(jagspico::ssprintf("devices/%llx/avail", id.id));
    }();
    return kAvailabilityTopic->c_str();
  }

  static std::string SetAvailabilityLWT(mqtt_connect_client_info_t& c) {
    c.will_msg = "unavailable";
    c.will_topic = GetAvailabilityTopic();
    c.will_retain = true;
    c.will_qos = lwipxx::MqttClient::kBestEffort;
  }

  // mqtt_client: a connected MQTT client.
  // topic_prefix: the prefix for all devices to be published by this service.
  //               Must be a valid MQTT topic part.
  static std::expected<std::unique_ptr<HomeAssistantService>, err_t> Connect(
      mqtt_connect_client_info_t connect_info,
      std::string topic_prefix = "homeassistant") {
    return std::unique_ptr<HomeAssistantService>{
        new HomeAssistantService(mqtt_client, std::move(topic_prefix))};
  }

  static void SetupAvailabilityLWT(mqtt_connect_client_info_t& info) {}

  // Adds a list of devices
  void AddDevice(freertosxx::Borrowable<Device> device) {
    devices_.push_back(device);

    auto bdevice = device.Borrow();
    bdevice->SetPublisher(this);
    std::string_view device_topic_suffix = bdevice->BaseTopic();
    std::string device_topic = jagspico::ssprintf(
        "%s/%*s",
        homeassistant_topic_prefix_.c_str(),
        device_topic_suffix.size(),
        device_topic_suffix.data());
    Subscribe(device_topic, bdevice);
    Announce(device_topic, bdevice);
  }

 private:
  HomeAssistantService(
      lwipxx::MqttClient& mqtt_client, std::string topic_prefix)
      : client_(mqtt_client), topic_prefix_(std::move(topic_prefix)) {}

  void Announce(
      std::string_view device_topic,
      freertosxx::BorrowedPointer<Device>& device) {
    JsonBuilder json;
    json.Kv("~", device_topic);
    device->AppendDiscovery(json);
    (void)client_.Publish(
        jagspico::ssprintf("%s/config", device_topic),
        std::move(json).Finish(),
        lwipxx::MqttClient::Qos::kAtLeastOnce,
        true);
  }

  // Subscribes the device to any updates it requests.
  void Subscribe(
      std::string_view device_topic,
      freertosxx::BorrowedPointer<Device>& device) {
    auto subs = device->SubscribeTo();
    for (int i = 0; i < subs.size(); ++i) {
      std::string topic = jagspico::ssprintf("%s/%s", root_topic, subs[i]);
      client_.Subscribe(
          topic,
          lwipxx::MqttClient::kAtLeastOnce,
          [this, i, d = freertosxx::Borrowable<Device>(device)](
              const lwipxx::MqttClient::Message& message) mutable {
            // If we get a message on a subscribed topic, borrow the device and
            // give it the message.
            auto borrowed = d.Borrow();
            borrowed->ReceiveMessage(i, message.data);
          });
    }
  }

  void ReceiveBirth(const lwipxx::MqttClient::Message& message);
  void ReceiveCommand(const lwipxx::MqttClient::Message& message);

  lwipxx::MqttClient& client_;
  std::string topic_prefix_;
  std::vector<freertosxx::Borrowable<Device>> devices_;
  std::string homeassistant_topic_prefix_;
};

}  // namespace homeassistant

#endif  // JAGSPICO_HA_H