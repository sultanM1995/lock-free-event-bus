#pragma once
#include <unordered_map>

#include "event.hpp"
#include "event_bus.hpp"
#include "lock_free_spsc_queue.hpp"


class Subscriber {
public:
    explicit Subscriber(EventBus& bus) : bus_(bus) {};

    void subscribe(const std::string& topic) {
        auto queue = std::make_shared<LockFreeSpscQueue<Event>>(8192);
        bus_.subscribe(topic, queue);
        queues_[topic] = queue;
    }

    std::optional<Event> poll(const std::string& topic) {
        auto it = queues_.find(topic);
        if (it != queues_.end()) {
            Event e{"", ""};
            if (it -> second -> dequeue(e)) {
                return e;
            }
        }
        return std::nullopt;
    }


private:
    EventBus& bus_;
    std::unordered_map<std::string, std::shared_ptr<LockFreeSpscQueue<Event>>> queues_;
};
