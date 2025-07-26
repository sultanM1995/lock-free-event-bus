#pragma once
#include "consumer_group.hpp"
#include "event.hpp"
#include "lock_free_spsc_queue.hpp"

namespace eventbus {
    class Consumer {
    public:
        explicit Consumer(ConsumerGroup& consumer_group) {
            queue_ = consumer_group.get_queues();
        }

        [[nodiscard]] std::optional<Event> poll() const {
            for (const auto& q : queue_) {
                if (Event event; q -> dequeue(event)) {
                    return event;
                }
            }
            return std::nullopt;
        }

    private:
        std::vector<std::shared_ptr<LockFreeSpscQueue<Event>>> queue_;
    };
}
