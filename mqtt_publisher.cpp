#include "mqtt_publisher.h"
#include <iostream>
#include "json.hpp"
using json = nlohmann::json;


MQTTPublisher::MQTTPublisher(const std::string& address, const std::string& topic)
    : serverAddress(address), topicName(topic), client(address, "vision_publisher") {}

bool MQTTPublisher::connect() {
    try {
        client.connect()->wait();
        std::cout << "[MQTT] Connected to broker." << std::endl;
        return true;
    } catch (const mqtt::exception& e) {
        std::cerr << "[MQTT] Connection failed: " << e.what() << std::endl;
        return false;
    }
}

void MQTTPublisher::publish(const std::string& message) {
    try {
        client.publish(topicName, message.c_str(), message.length(), 1, false);
    } catch (const mqtt::exception& e) {
        std::cerr << "[MQTT] Publish failed: " << e.what() << std::endl;
    }
}

void MQTTPublisher::disconnect() {
    try {
        client.disconnect()->wait();
        std::cout << "[MQTT] Disconnected." << std::endl;
    } catch (const mqtt::exception& e) {
        std::cerr << "[MQTT] Disconnect failed: " << e.what() << std::endl;
    }
}
