#ifndef LWIPXX_MQTT_H
#define LWIPXX_MQTT_H

#include <expected>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "freertosxx/mutex.h"
#include "lwip/apps/mqtt.h"

namespace lwipxx {

// Wraps the lwIP MQTT client in a nice (?) C++ interface.
class MqttClient {
 public:
  enum Qos { kBestEffort = 0, kAtLeastOnce = 1, kAtMostOnce = 2 };

  MqttClient() = delete;

  // Returns an MqttClient when the client has successfully connected,
  // or else returns an err_t.
  static std::expected<std::unique_ptr<MqttClient>, err_t> Create(
      std::string_view host, uint16_t port,
      const mqtt_connect_client_info_t& client_info);

  // Publishes message on topic. qos 0 is ephemeral, 1 is at least once, 2 is at
  // most once.
  err_t Publish(
      std::string_view topic, std::string_view message, Qos qos, bool retain);

  struct Message {
    std::string_view topic;
    std::string_view data;
    uint8_t flags;
  };
  using DataHandler = std::function<void(const Message& message)>;

  // Subscribes to a topic, including topic wildcards. Notifications will be
  // sent to the provided handler function.
  //
  // * It is illegal to add the same topic selector twice.
  // * For a topic published by the broker, the first handler whose selector
  //   matches will be chosen.
  err_t Subscribe(
      std::string_view topic_selector, Qos qos, DataHandler handler);

  // The selector from which to unsubscribe must exactly match the selector
  // previously subscribed to. The handler for topic may be called while
  // Unsubscribe is running but won't be called after Unsubscribe returns.
  //
  // You MUST NOT unsubscribe from a topic in its own handler, or a deadlock
  // may result.
  //
  // Unsubscribing from a selector that that is not subscribed to is a no-op.
  err_t Unsubscribe(std::string_view topic_selector, Qos qos);

 private:
  MqttClient(mqtt_client_t* client);

  err_t Connect(
      std::string_view host, uint16_t port,
      const mqtt_connect_client_info_t& client_info);

  err_t SubUnsub(std::string_view topic, Qos qos, bool sub);

  void ChangeTopic(std::string_view topic, int num_messages);
  void ReceiveMessage(std::span<const uint8_t> message, uint8_t flags);

  void AddHandler(std::string_view topic_selector, DataHandler handler);
  void RemoveHandler(std::string_view topic_selector);

  bool TopicMatchesSubscription(
      std::string_view subscription, std::string_view topic);

  std::unique_ptr<mqtt_client_t, decltype(&mqtt_client_free)> client_;

  // Only accessed from the callback thread. Thus not guarded by a lock.
  std::string_view current_topic_;

  // We check the topic for which handler should handle the message at the
  // beginning of the topic. This way, we can call the handler function without
  // needing to do a map lookup after each message. If a topic is removed from
  // the map,

  freertosxx::Mutex mutex_;  // Guards handlers_ and active_handler_index_.
  std::vector<std::pair<std::string, DataHandler>> handlers_;
  DataHandler active_handler_;

  std::string pending_message_;
};

}  // namespace lwipxx

#endif
