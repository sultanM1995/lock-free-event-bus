#pragma once
#include "consumer_group.hpp"
#include "event.hpp"
#include "lock_free_mpsc_queue.hpp"
#include <vector>

namespace eventbus {
    class Consumer {
    public:
        explicit Consumer(ConsumerGroup& consumer_group);

        void receive_queues(const std::vector<std::shared_ptr<LockFreeMpscQueue<Event>>>& queues);

        [[nodiscard]] std::vector<Event> poll_batch(size_t max_events = 10) const;

        [[nodiscard]] const std::string& consumer_id() const {
            return consumer_id_;
        }


    private:
        std::vector<std::shared_ptr<LockFreeMpscQueue<Event>>> queues_;
        std::string consumer_id_;
    };
}