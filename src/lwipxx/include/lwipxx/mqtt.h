#ifndef LWIPXX_MQTT_H
#define LWIPXX_MQTT_H

#include <cmath>
#include <expected>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "freertosxx/event.h"
#include "freertosxx/mutex.h"
#include "lwip/apps/mqtt.h"
#include "lwip/ip_addr.h"
#include "projdefs.h"

namespace lwipxx {

// Wraps the lwIP MQTT client in a nice (?) C++ interface.
//
// This class handles reconnection. As yet, we have no need to permanently
// disconnect from a server once connected, so there is no provision for
// destroying this class or ceasing the connection.
class MqttClient {
 public:
  enum Qos { kBestEffort = 0, kAtLeastOnce = 1, kAtMostOnce = 2 };

  struct ConnectInfo {
    std::variant<std::string, ip_addr_t> broker_address;
    uint16_t broker_port = 1883;
    std::string client_id;
    std::string user;
    std::string password;
    std::string lwt_topic;
    std::string lwt_message;
    Qos lwt_qos = kBestEffort;
    bool lwt_retain = true;
  };

  static std::expected<std::unique_ptr<MqttClient>, err_t> Create(
      ConnectInfo info);

  ~MqttClient();

  // Publishes message on topic. Returns only when the message has been
  // acknowledged by the broker, or an error has occurred.
  [[nodiscard]] err_t Publish(
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
  // * If this subscribe fails immediately before we try to contact the server,
  //   we'll return an error and not attempt to reconnect.
  // * Otherwise, we'll attempt to reconnect and resubscribe if the mqtt
  //   server goes down.
  // * If a disconnect occurs, we'll automatically reconnect.
  //
  // TODO: Consider adding a completion callback? But does anyone actually want
  // that?
  [[nodiscard]] err_t Subscribe(
      std::string_view topic_selector, Qos qos, DataHandler handler);

  // The selector from which to unsubscribe must exactly match the selector
  // previously subscribed to. The handler for topic may be called while
  // Unsubscribe is running but won't be called after Unsubscribe returns.
  //
  // You MUST NOT unsubscribe from a topic in its own handler, or a deadlock
  // may result.
  //
  // Unsubscribing from a selector that that is not subscribed to is a no-op.
  [[nodiscard]] err_t Unsubscribe(std::string_view topic_selector);

 private:
  static constexpr EventBits_t kConnected = 0b1;

  // Life of a subscription:
  //
  // 1. The subscription is added with Subscribe. It has one immutable
  //    element at this point: the topic. The QoS and handler may be overwritten
  //    by subsequent calls to Subscribe. We also track the state of this topic
  //    with the remote server in has_pending_callback, subscribed, and
  //    unsubscribing.
  //
  // 2. A subscribtion's removal begins with a call to Unsubscribing. This
  //    sets Subscription.unsubscribing to true.
  //
  // 2. After Subscribe, Unsubscribe, or any time a callback regarding a
  //    subscription ends, we decide whether we need to take some action on a
  //    Subscription. The possible actions are to send a subscribe or an
  //    unsubscribe request.
  //    * If has_pending_callback is true, we take no action.
  //    * We send subscribe if subscribe is false and unsubscribing is false.
  //    * We send unsubscribe if unsubscribing is true and subscribed is true.
  //    Each time we take an action, we set has_pending_callback to true.
  //
  // 3. When a callback ends:
  //    * has_pending_callback is set to false.
  //    * If the callback is a subscribe request and it was successful,
  //      subscribed is marked as true.
  //    * If the callback is an unsubscribe request and it was successful,
  //      subscribed is marked as false. If subscribed is false and
  //      unsubscribing is true, the Subscription is removed from the vector.
  //
  // If a disconnect happens, each Subscription is marked as having its non-
  // desired state (i.e. subscribed is set to the value of unsubscribing).
  // Every Subscription that does not have a pending callback will be either
  // subscribed or unsubscribed as needed.
  struct Subscription {
    std::string topic;
    Qos qos;
    DataHandler handler;
    int failed_requests = 0;
    bool has_pending_callback = false;
    bool want_subscribed = true;
    bool is_subscribed = false;
  };

  MqttClient(ConnectInfo info) : connect_info_(std::move(info)) {}

  void Connect();

  // Called when the connection status changes.
  void ConnectionCb(const mqtt_connection_status_t& status);

  // If the subscription wants to be subscribed and isn't, calls sub.
  // The opposite r.e. unsubscribing.
  enum TransitionFailureHandling { kAllowPermanentError, kRetryAllErrors };
  err_t StartTransition(
      Subscription& sub, TransitionFailureHandling failure_handling);

  void FinishTransition(Subscription& sub, bool is_subscribe, err_t err);

  // Executes a function *in the tcpip thread* with a delay..
  void WithBackoff(int& attempt_count, std::function<void()> f);

  void ChangeTopic(std::string_view topic, int num_messages);
  void ReceiveMessage(std::span<const uint8_t> message, uint8_t flags);

  bool TopicMatchesSubscription(
      const Subscription& subscription, std::string_view topic);

  int connect_failures_ = 0;
  ConnectInfo connect_info_;

  std::unique_ptr<mqtt_client_t, decltype(&mqtt_client_free)> client_{
      mqtt_client_new(), &mqtt_client_free};

  // State manipulation is exclusively handled by the TCP thread, hence no
  // locks.
  std::vector<std::unique_ptr<Subscription>> subscriptions_;
  std::string active_topic_;
  Subscription* active_subscription_ = nullptr;
  std::string pending_message_;

  bool shutdown_ = false;
};

}  // namespace lwipxx

#endif
