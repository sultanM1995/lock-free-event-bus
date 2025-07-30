#pragma once
#include "consumer_group.hpp"
#include "event.hpp"
#include "lock_free_spsc_queue.hpp"
#include <vector>

namespace eventbus {
    class Consumer {
    public:
        explicit Consumer(ConsumerGroup& consumer_group);

        void receive_queues(const std::vector<std::shared_ptr<LockFreeSpscQueue<Event>>>& queues);

        [[nodiscard]] std::vector<Event> poll_batch(size_t max_events = 10) const;

    private:
        std::vector<std::shared_ptr<LockFreeSpscQueue<Event>>> queues_;
        size_t consumer_id = 0;
    };
}