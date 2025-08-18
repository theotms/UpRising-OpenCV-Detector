#ifndef MQTT_PUBLISHER_H
#define MQTT_PUBLISHER_H

#include <mqtt/async_client.h>
#include <string>
#include "json.hpp"
using json = nlohmann::json;


class MQTTPublisher {
public:
    MQTTPublisher(const std::string& address, const std::string& topic);
    bool connect();
    void publish(const std::string& message);
    void disconnect();

private:
    std::string serverAddress;
    std::string topicName;
    mqtt::async_client client;
};

#endif
