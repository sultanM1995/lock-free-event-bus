#pragma once
#include <string>

#include "back_pressure_strategy.hpp"
#include "event.hpp"
#include "lock_free_spsc_queue.hpp"

namespace eventbus {
    class Consumer;
    class ConsumerGroup {
    public:
        ConsumerGroup(std::string group_id, size_t partition_count);
        size_t register_consumer(Consumer* consumer);
        void create_partition_assignments_among_consumers_();

        // called by bus to deliver message to one of the partitions of topic that this consumer is consuming from.
        bool deliver_event_to_consumer_group(const Event& event, size_t partition_index, const BackPressureHandler& back_pressure_handler) const;

    private:
        std::string group_id_; // Consumer group id
        std::atomic<size_t> next_consumer_idx_{0}; // tracks the consumer that's connecting to this group
        size_t topic_partition_count_; // partition count of the topic that this group consumes from
        std::vector<std::shared_ptr<LockFreeSpscQueue<Event>>> partition_queues_; // queue for each partition
        std::unordered_map<size_t, std::vector<std::shared_ptr<LockFreeSpscQueue<Event>>>> queue_assignments_by_consumer_index_; // consumer to list of queue map.
        std::vector<Consumer*> assigned_consumers_;
        bool finalized_consumer_group_{false};
    };
}
