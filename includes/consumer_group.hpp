#pragma once
#include <string>
#include <utility>
#include "event.hpp"
#include "lock_free_spsc_queue.hpp"

namespace eventbus {
    class ConsumerGroup {

    public:
        ConsumerGroup(std::string group_id, const size_t consumer_group_size, const size_t partition_count):
        group_id_(std::move(group_id)), consumer_group_size_(consumer_group_size), partition_count_(partition_count) {
            effective_consumer_count_ = std::min(consumer_group_size_, partition_count_);
            create_partition_assignments();
        }


        // called by an individual consumer to get it's queues to consume from.
        std::vector<std::shared_ptr<LockFreeSpscQueue<Event>>>& get_queues() {
            const size_t idx = next_consumer_idx_.fetch_add(1, std::memory_order_relaxed);
            if (idx >= consumer_group_size_) {
                throw std::runtime_error("consumer_group '" + group_id_ +
                    "' is full, Group size is - " + std::to_string(consumer_group_size_));
            }
            return queue_assignments_by_consumer_index_[idx];
        }

        // called by bus to deliver message to all the consumers of this group.
        void deliver_event_to_consumer_group(const Event& event, const size_t partition_index) const {
            const auto partition_queue = partition_queues_[partition_index];
            partition_queue->enqueue(event);
        }


    private:
        std::string group_id_; // Consumer group id
        size_t consumer_group_size_; // Consumer group size defined
        size_t effective_consumer_count_; // effective consumer count
        std::atomic<size_t> next_consumer_idx_{0}; // tracks the consumer that's connecting to this group
        size_t partition_count_; // partition count of the topic that this group consumes from
        std::vector<std::shared_ptr<LockFreeSpscQueue<Event>>> partition_queues_; // queue for each partition
        std::unordered_map<size_t, std::vector<std::shared_ptr<LockFreeSpscQueue<Event>>>> queue_assignments_by_consumer_index_;
        // consumer to list of queue map.

        void create_partition_assignments() {
            // Round-robin way of assignment when partition_count > consumer_group_size
            // For example, we have 5 partition and 2 as group size
            // This is how the assignment will be
            // 0 -> 0, 2, 4 and 1 -> 1, 3
            for (size_t i = 0; i < partition_count_; ++i) {
                auto partition_queue = std::make_shared<LockFreeSpscQueue<Event>>(8192);
                partition_queues_.push_back(partition_queue);
                queue_assignments_by_consumer_index_[i % consumer_group_size_]
                .push_back(partition_queue);
            }
        }
    };
}
