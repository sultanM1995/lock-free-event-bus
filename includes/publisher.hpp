// producer.h
#pragma once
#include "event_bus.hpp"
#include <string>
#include <thread>

class Publisher {
public:
    Publisher(EventBus& bus, std::string topic)
        : bus_(bus), topic_(std::move(topic)) {}

    void run(int count, int delayMs) const {
        for (int i = 1; i <= count; ++i) {
            Event event(topic_, topic_ + " message " + std::to_string(i));
            bus_.publish(event);
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        }
    }

private:
    EventBus& bus_;
    std::string topic_;
};
