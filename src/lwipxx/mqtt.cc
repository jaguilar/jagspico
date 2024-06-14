#include "lwipxx/mqtt.h"

#include <algorithm>
#include <cstdio>
#include <expected>
#include <functional>
#include <memory>
#include <ranges>
#include <span>
#include <string_view>
#include <variant>

#include "arch/cc.h"
#include "freertosxx/include/freertosxx/queue.h"
#include "lwip/api.h"
#include "lwip/apps/mqtt.h"
#include "lwip/err.h"
#include "lwip/ip_addr.h"
#include "pico/time.h"
#include "portmacro.h"
#include "util/include/util/cleanup.h"

namespace lwipxx {
using freertosxx::MutexLock;

#define MQTTDBG(...) printf(__VA_ARGS__)

std::expected<std::unique_ptr<MqttClient>, err_t> MqttClient::Create(
    ConnectInfo info) {
  if (std::string* host = std::get_if<std::string>(&info.broker_address)) {
    ip_addr_t address;
    err_t err = netconn_gethostbyname(host->c_str(), &address);
    if (err != ERR_OK) {
      printf("Error resolving host: %s, %s\n", host->c_str(), lwip_strerr(err));
      return std::unexpected(err);
    }
    info.broker_address = address;
  }

  std::unique_ptr<MqttClient> client(new MqttClient(std::move(info)));
  client->Connect();
  return client;
}

MqttClient::~MqttClient() {
  LOCK_TCPIP_CORE();
  mqtt_disconnect(client_.get());
  mqtt_client_free(client_.get());
  UNLOCK_TCPIP_CORE();
}

void MqttClient::ConnectionCb(const mqtt_connection_status_t& status) {
  if (shutdown_) return;
  if (status == MQTT_CONNECT_ACCEPTED) {
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
    // All of the subscriptions will be in a broken state if we were
    // disconnected. Resubscribe any we want to be subscribed to and unsubscribe
    // the rest.
    for (auto& sub : subscriptions_) {
      if (sub->has_pending_callback) continue;
      StartTransition(*sub, kRetryAllErrors);
    }
  } else {
    // We are disconnected. Mark all subscriptions as being in the opposite
    // state as we'd like them to be. Then, retry our connection with a backoff.
    for (auto& sub : subscriptions_) {
      sub->is_subscribed = !sub->want_subscribed;
    }
    WithBackoff(connect_failures_, [this] { Connect(); });
  }
}

void MqttClient::Connect() {
  mqtt_connect_client_info_t connect_info{
      .client_id = connect_info_.client_id.c_str(),
      .client_user = connect_info_.user.c_str(),
      .client_pass = connect_info_.password.c_str(),
      .keep_alive = 60,
      .will_topic = connect_info_.lwt_topic.c_str(),
      .will_msg = connect_info_.lwt_message.c_str(),
      .will_qos = 0,
      .will_retain = 1,
  };
  err_t err = mqtt_client_connect(
      client_.get(),
      &std::get<ip_addr_t>(connect_info_.broker_address),
      connect_info_.broker_port,
      +[](mqtt_client_t* unused, void* arg, mqtt_connection_status_t status) {
        static_cast<MqttClient*>(arg)->ConnectionCb(status);
      },
      this,
      &connect_info);
  if (err != ERR_OK) {
    WithBackoff(connect_failures_, [this] { Connect(); });
  }
}

err_t MqttClient::Publish(
    std::string_view topic, std::string_view message, Qos qos, bool retain) {
  struct PublishCbData {
    err_t result;
    freertosxx::EventGroup event;
  } cb_data;
  LOCK_TCPIP_CORE();
  err_t err = mqtt_publish(
      client_.get(),
      topic.data(),
      message.data(),
      message.size(),
      static_cast<uint8_t>(qos),
      retain ? 1 : 0,
      +[](void* arg, err_t err) {
        PublishCbData* data = static_cast<PublishCbData*>(arg);
        data->result = err;
        data->event.Set(1);
      },
      &cb_data);
  UNLOCK_TCPIP_CORE();
  if (err != ERR_OK) {
    printf(
        "Error publishing message to %s: %s\n", topic.data(), lwip_strerr(err));
    return err;
  }
  cb_data.event.Wait(1);
  return cb_data.result;
}

err_t MqttClient::Subscribe(
    std::string_view topic_selector, Qos qos, DataHandler handler) {
  LOCK_TCPIP_CORE();
  auto cleanup = jagspico::Cleanup([&] { UNLOCK_TCPIP_CORE(); });
  // See if we already have a subscription for this topic.
  auto it = std::find_if(
      subscriptions_.begin(), subscriptions_.end(), [&](const auto& sub) {
        return sub->topic == topic_selector;
      });
  if (it != subscriptions_.end()) {
    (*it)->qos = qos;
    (*it)->handler = std::move(handler);
    (*it)->want_subscribed = true;
    (*it)->failed_requests = 0;
    return StartTransition(**it, kRetryAllErrors);
  }

  auto sub = std::make_unique<Subscription>(Subscription{
      .topic = std::string(topic_selector),
      .qos = qos,
      .handler = std::move(handler),
  });
  err_t err = StartTransition(*sub, kAllowPermanentError);
  if (err == ERR_OK) {
    subscriptions_.push_back(std::move(sub));
  }
  return err;
}

err_t MqttClient::Unsubscribe(std::string_view topic_selector) {
  LOCK_TCPIP_CORE();
  auto it = std::find_if(
      subscriptions_.begin(), subscriptions_.end(), [&](const auto& sub) {
        return sub->topic == topic_selector;
      });
  err_t err = ERR_OK;
  if (it != subscriptions_.end()) {
    (*it)->want_subscribed = false;
    err = StartTransition(**it, kAllowPermanentError);
  }
  UNLOCK_TCPIP_CORE();
  return err;
}

err_t MqttClient::StartTransition(
    Subscription& sub, TransitionFailureHandling failure_handling) {
  if (sub.has_pending_callback) return ERR_OK;
  if (sub.want_subscribed == sub.is_subscribed) {
    if (!sub.want_subscribed) {
      // We're unsubscribed and we want to be, so remove the subscription
      // object.
      subscriptions_.erase(
          std::remove_if(
              subscriptions_.begin(),
              subscriptions_.end(),
              [&](const std::unique_ptr<Subscription>& subscription) {
                return subscription.get() == &sub;
              }),
          subscriptions_.end());
    }
    return ERR_OK;
  }

  struct TransitionArg {
    MqttClient& client;
    Subscription& sub;
    bool is_subscribe;
  };
  auto cb_arg = new TransitionArg{*this, sub, sub.want_subscribed};

  mqtt_request_cb_t cb = +[](void* varg, err_t err) {
    std::unique_ptr<TransitionArg> arg(static_cast<TransitionArg*>(varg));
    arg->client.FinishTransition(arg->sub, arg->is_subscribe, err);
  };

  err_t err = mqtt_sub_unsub(
      client_.get(),
      sub.topic.data(),
      sub.qos,
      cb,
      cb_arg,
      cb_arg->is_subscribe);
  if (err != ERR_OK) {
    // Immediate errors are probably out of memory errors. We delete the
    // callback and return them.
    delete cb_arg;
    sub.has_pending_callback = false;
    ++sub.failed_requests;
  }
  return err;
}

void MqttClient::FinishTransition(
    Subscription& sub, bool is_subscribe, err_t err) {
  sub.has_pending_callback = false;
  if (err == ERR_OK) {
    sub.is_subscribed = is_subscribe;
    err = StartTransition(sub, kRetryAllErrors);
  }

  // All done.
  if (err == ERR_OK) return;

  // If an error occurred either in this callback or in starting the next
  // transition, instead retry the next transition with a backoff.
  sub.has_pending_callback = true;
  WithBackoff(sub.failed_requests, [this, &sub = sub] {
    sub.has_pending_callback = false;
    StartTransition(sub, kRetryAllErrors);
  });
}

template <int min_ms, int max_ms>
TickType_t Backoff(int& attempts) {
  constexpr TickType_t kMinBackoffLatency = pdMS_TO_TICKS(min_ms);
  constexpr TickType_t kMaxBackoffLatency = pdMS_TO_TICKS(max_ms);
  constexpr int kMaxReconnectAttemptsToCount =
      std::log(kMaxBackoffLatency / kMinBackoffLatency) / std::log(2);
  const TickType_t ret = kMinBackoffLatency << attempts;
  if (attempts < kMaxReconnectAttemptsToCount) ++attempts;
  return ret;
}

void MqttClient::WithBackoff(int& failed_attempts, std::function<void()> f) {
  struct Arg {
    MqttClient& client;
    std::function<void()> f;
  };
  auto pend_cb = +[](void* varg, uint32_t unused_arg) {
    std::unique_ptr<Arg> arg(static_cast<Arg*>(varg));
    LOCK_TCPIP_CORE();
    auto cleanup = jagspico::Cleanup([&] { UNLOCK_TCPIP_CORE(); });
    arg->f();
  };
  xTimerPendFunctionCall(
      pend_cb,
      new Arg{*this, std::move(f)},
      0,
      Backoff<250, 5000>(failed_attempts));
}

bool MqttClient::TopicMatchesSubscription(
    const Subscription& subscription, std::string_view topic) {
  std::string_view sub_topic = subscription.topic;
  while (!topic.empty()) {
    if (sub_topic.empty()) return false;
    const std::string_view topic_part = topic.substr(0, topic.find('/'));
    if (sub_topic.front() == '+') {
      topic.remove_prefix(topic_part.size() + 1);
      sub_topic.remove_prefix(2);
    } else if (sub_topic.front() == '#') {
      return true;
    } else {
      const std::string_view subscription_part =
          sub_topic.substr(0, sub_topic.find('/'));
      if (topic_part != subscription_part) return false;
      if (topic_part.size() == topic.size() &&
          subscription_part.size() == sub_topic.size()) {
        return true;
      }
      topic.remove_prefix(topic_part.size() + 1);
      sub_topic.remove_prefix(subscription_part.size() + 1);
    }
  }
  if (!sub_topic.empty()) {
    return false;
  }
  return true;
}

void MqttClient::ChangeTopic(std::string_view topic, int total_length) {
  MQTTDBG("ChangeTopic(%*s, %d)\n", topic.size(), topic.data(), total_length);
  for (int i = 0; i < subscriptions_.size(); ++i) {
    if (TopicMatchesSubscription(*subscriptions_[i], topic)) {
      active_topic_ = topic;
      active_subscription_ = subscriptions_[i].get();
      return;
    }
  }
  active_subscription_ = nullptr;
}

void MqttClient::ReceiveMessage(
    std::span<const uint8_t> message, uint8_t flags) {
  // No matching handler.
  if (active_subscription_ == nullptr) return;

  const bool completed_message = flags & MQTT_DATA_FLAG_LAST;
  if (!completed_message || !pending_message_.empty()) {
    pending_message_.append(message.begin(), message.end());
  }
  if (!completed_message) {
    return;
  }

  const std::string_view data =
      !pending_message_.empty()
          ? pending_message_
          : std::string_view{
                reinterpret_cast<const char*>(message.data()), message.size()};
  Message m{active_topic_, data, flags};
  MQTTDBG(
      "ReceiveMessage(%*s%s, %c)\n",
      std::min<int>(10, m.topic.size()),
      m.topic.data(),
      m.topic.size() > 10 ? "..." : "",
      m.flags);
  active_subscription_->handler(std::move(m));
  pending_message_.clear();
}

}  // namespace lwipxx