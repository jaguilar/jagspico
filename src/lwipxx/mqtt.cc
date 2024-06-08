#include "lwipxx/mqtt.h"

#include <cstdio>
#include <expected>
#include <memory>
#include <span>

#include "freertosxx/include/freertosxx/queue.h"
#include "lwip/api.h"
#include "lwip/apps/mqtt.h"
#include "lwip/err.h"

namespace lwipxx {

#define MQTTDBG(...) printf(__VA_ARGS__)

using ErrCb = void (*)(void*, err_t);
auto SendErrToQueue() {
  return +[](void* arg, err_t err) {
    reinterpret_cast<freertosxx::Queue<err_t>*>(arg)->Send(err);
  };
}

std::expected<std::unique_ptr<MqttClient>, err_t> MqttClient::Create(
    std::string_view host, uint16_t port,
    const mqtt_connect_client_info_t& client_info) {
  std::unique_ptr<MqttClient> client(new MqttClient(mqtt_client_new()));
  err_t err = client->Connect(host, port, client_info);
  if (err != ERR_OK) {
    return std::unexpected(err);
  }
  return client;
}

err_t MqttClient::Publish(
    std::string_view topic, std::string_view message, Qos qos, bool retain) {
  freertosxx::StaticQueue<err_t, 1> err_queue;
  err_t err = mqtt_publish(
      client_.get(),
      topic.data(),
      message.data(),
      message.size(),
      static_cast<uint8_t>(qos),
      retain ? 1 : 0,
      SendErrToQueue(),
      &err_queue);
  if (err != ERR_OK) {
    printf(
        "Error publishing message to %s: %s\n", topic.data(), lwip_strerr(err));
  }
  err_t cb_err = err_queue.Receive();
  if (err == ERR_OK) err = cb_err;
  if (err != ERR_OK) {
    printf(
        "Error publishing message starting with %s (%d bytes): %s\n",
        std::string(topic.substr(0, 10)).c_str(),
        topic.size(),
        lwip_strerr(err));
  } else {
    printf("Published to %s\n", topic.data());
  }
  return err;
}

err_t MqttClient::Subscribe(
    std::string_view topic_selector, Qos qos, DataHandler handler) {
  {
    freertosxx::MutexLock l(mutex_);
    for (const auto& [key, _] : handlers_) {
      if (key == topic_selector) {
        printf("Error: topic selector already registered: %s\n", key.data());
        return ERR_ARG;
      }
    }
    handlers_.push_back(
        std::make_pair(std::string(topic_selector), std::move(handler)));
  }
  err_t err = SubUnsub(topic_selector, qos, true);
  if (err != ERR_OK) {
    RemoveHandler(topic_selector);
    return err;
  }
  return err;
}

err_t MqttClient::Unsubscribe(std::string_view topic_selector, Qos qos) {
  {
    freertosxx::MutexLock l(mutex_);
    RemoveHandler(topic_selector);
  }
  return SubUnsub(topic_selector, qos, false);
}

MqttClient::MqttClient(mqtt_client_t* client)
    : client_(client, mqtt_client_free) {}
err_t MqttClient::Connect(
    std::string_view host, uint16_t port,
    const mqtt_connect_client_info_t& client_info) {
  ip_addr_t address;
  err_t err = netconn_gethostbyname(host.data(), &address);
  if (err != ERR_OK) {
    printf("Error resolving host: %s, %s\n", host.data(), lwip_strerr(err));
    return err;
  }

  freertosxx::StaticQueue<mqtt_connection_status_t, 1> connection_status_queue;
  err = mqtt_client_connect(
      client_.get(),
      &address,
      port,
      +[](mqtt_client_s*, void* arg, mqtt_connection_status_t status) {
        reinterpret_cast<decltype(connection_status_queue)*>(arg)->Send(status);
      },
      &connection_status_queue,
      &client_info);
  if (err != ERR_OK) {
    printf("Error connecting to MQTT: %s\n", lwip_strerr(err));
    return err;
  }
  mqtt_connection_status_t connection_status =
      connection_status_queue.Receive();
  if (connection_status != MQTT_CONNECT_ACCEPTED) {
    printf("Connection status != accepted: %d\n", connection_status);
    return ERR_CONN;
  }
  printf("mqtt connected\n");
  mqtt_set_inpub_callback(
      client_.get(),
      +[](void* arg, const char* buf, uint32_t remaining) {
        reinterpret_cast<decltype(this)>(arg)->ChangeTopic(
            std::string_view(buf), remaining);
      },
      +[](void* arg, const uint8_t* buf, uint16_t len, uint8_t flags) {
        reinterpret_cast<decltype(this)>(arg)->ReceiveMessage(
            std::span<const uint8_t>(buf, len), flags);
      },
      this);
  return ERR_OK;
}

err_t MqttClient::SubUnsub(std::string_view topic, Qos qos, bool sub) {
  freertosxx::StaticQueue<err_t, 1> err_queue;
  err_t err = mqtt_sub_unsub(
      client_.get(),
      topic.data(),
      static_cast<uint8_t>(qos),
      SendErrToQueue(),
      &err_queue,
      sub);
  err_t cb_err = err_queue.Receive();
  if (err == ERR_OK) err = cb_err;
  if (err != ERR_OK) {
    printf("Error subscribing to %s: %s\n", topic.data(), lwip_strerr(err));
  }
  printf("%ssubscribed: %s\n", sub ? "" : "un", topic.data());
  return err;
}

void MqttClient::RemoveHandler(std::string_view topic_selector) {
  for (int i = 0; i < handlers_.size(); i++) {
    if (handlers_[i].first == topic_selector) {
      handlers_.erase(handlers_.begin() + i);
      return;
    }
  }
}

bool MqttClient::TopicMatchesSubscription(
    std::string_view subscription, std::string_view topic) {
  while (!topic.empty()) {
    if (subscription.empty()) return false;
    const std::string_view topic_part = topic.substr(0, topic.find('/'));
    if (subscription.front() == '+') {
      topic.remove_prefix(topic_part.size() + 1);
      subscription.remove_prefix(2);
    } else if (subscription.front() == '#') {
      return true;
    } else {
      const std::string_view subscription_part =
          subscription.substr(0, subscription.find('/'));
      if (topic_part != subscription_part) return false;
      if (topic_part.size() == topic.size() &&
          subscription_part.size() == subscription.size()) {
        return true;
      }
      topic.remove_prefix(topic_part.size() + 1);
      subscription.remove_prefix(subscription_part.size() + 1);
    }
  }
  if (!subscription.empty()) {
    return false;
  }
  return true;
}

void MqttClient::ChangeTopic(std::string_view topic, int num_messages) {
  MQTTDBG("ChangeTopic(%*s, %d)\n", topic.size(), topic.data(), num_messages);
  freertosxx::MutexLock l(mutex_);
  current_topic_ = topic;
  remaining_messages_ = num_messages;
  if (current_topic_.empty()) {
    assert(remaining_messages_ == 0);
    return;
  }
  for (int i = 0; i < handlers_.size(); ++i) {
    if (TopicMatchesSubscription(handlers_[i].first, current_topic_)) {
      active_handler_ = handlers_[i].second;
      return;
    }
  }
}

void MqttClient::ReceiveMessage(
    std::span<const uint8_t> message, uint8_t flags) {
  if (active_handler_) {
    Message m{
        current_topic_,
        std::string_view(
            reinterpret_cast<const char*>(message.data()), message.size()),
        flags};
    MQTTDBG(
        "ReceiveMessage(%*s%s, %c)\n",
        std::min<int>(10, m.topic.size()),
        m.topic.data(),
        m.topic.size() > 10 ? "..." : "",
        m.flags);
    active_handler_(std::move(m));
  } else {
    MQTTDBG("ReceiveMessage(): No active handler for message\n");
  }
  if (--remaining_messages_ == 0) {
    MQTTDBG("Topic complete\n");
    active_handler_ = {};
  }
}

}  // namespace lwipxx