#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include "event.hpp"
#include "lock_free_spsc_queue.hpp"

class EventBus {
    using QueuePtr = std::shared_ptr<LockFreeSpscQueue<Event>>;
public:
    void subscribe(const std::string& topic, const QueuePtr& queue) {
        subscribers_[topic].emplace_back(queue);
    }

    void publish(const Event& event) {
        const auto it = subscribers_.find(event.topic);
        if (it != subscribers_.end()) {
            for (const auto& queue : it->second) {
                queue->enqueue(event);
            }
        }
    }


private:
    std::unordered_map<std::string, std::vector<QueuePtr>> subscribers_;
};
