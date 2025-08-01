#include "consumer_group.hpp"

#include "back_pressure_strategy.hpp"
#include "consumer.hpp"

namespace eventbus {
    ConsumerGroup::ConsumerGroup(std::string group_id,
        const size_t partition_count):
    group_id_(std::move(group_id)),
    topic_partition_count_(partition_count) {}

    std::string ConsumerGroup::register_consumer(Consumer* consumer) {
        const size_t consumer_index = assigned_consumers_.size();
        assigned_consumers_.push_back(consumer);
        return group_id_ + "/" + std::to_string(consumer_index);
    }

    void ConsumerGroup::create_partition_assignments_among_consumers_() {

        if (finalized_consumer_group_) {
            throw std::runtime_error("Cannot register after setup is done");
        }

        if (assigned_consumers_.empty()) {
            throw std::runtime_error("No consumers registered for - " + group_id_);
        }

        // Round-robin way of assignment when partition_count > consumer_group_size
        // For example, we have 5 partition and 2 as group size
        // This is how the assignment will be
        // 0 -> 0, 2, 4 and 1 -> 1, 3
        for (size_t i = 0; i < topic_partition_count_; ++i) {
            auto partition_queue = std::make_shared<LockFreeMpscQueue<Event>>(8192);
            partition_queues_.push_back(partition_queue);
            queue_assignments_by_consumer_index_[i % assigned_consumers_.size()]
            .push_back(partition_queue);
        }

        for (size_t i = 0; i < assigned_consumers_.size(); ++i) {
            if (queue_assignments_by_consumer_index_.find(i) == queue_assignments_by_consumer_index_.end()) {
                continue;
            }
            assigned_consumers_[i]->receive_queues(queue_assignments_by_consumer_index_[i]);
        }

        finalized_consumer_group_ = true;
    }

    bool ConsumerGroup::deliver_event_to_consumer_group(const Event& event, const size_t partition_index, const BackPressureHandler& back_pressure_handler) const {
        const auto partition_queue = partition_queues_[partition_index];
        const bool can_enqueue = back_pressure_handler.try_enqueue_with_backpressure_strategy(partition_queue, event);
        return can_enqueue;
    }

}
