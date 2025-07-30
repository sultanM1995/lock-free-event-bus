#pragma once
#include <unordered_map>
#include <vector>
#include <string>

#include "back_pressure_strategy.hpp"
#include "consumer_group.hpp"
#include "event.hpp"
#include "lock_free_spsc_queue.hpp"
#include "topic.hpp"

namespace eventbus {
    using queue_ptr = std::shared_ptr<LockFreeSpscQueue<Event>>;

    class EventBus {

    public:
        explicit EventBus(const BackPressureConfig& config = {})
            : backpressure_handler_(config){}

        void set_up_done() {
            if (set_up_done_.load(std::memory_order_relaxed)) {
                return;
            }

            for (auto& [topic, consumer_groups] : consumer_groups_by_topic_name_) {
                for (const auto& consumer_group : consumer_groups) {
                    consumer_group->create_partition_assignments_among_consumers_();
                }
            }
            set_up_done_.store(true, std::memory_order_relaxed);
        }

        void create_topic(const std::string& topic_name, const int partition_count) {
            if (set_up_done_.load(std::memory_order_relaxed)) {
                throw std::runtime_error("Can't create topic, event bus setup is done.");
            }
            if (does_topic_exist(topic_name)) {
                throw std::runtime_error("Topic already exists.");
            }
            topics_.emplace(topic_name, Topic(topic_name, partition_count));
        }

        std::shared_ptr<ConsumerGroup> create_consumer_group(const std::string& group_id, const std::string& topic_name) {
            if (set_up_done_.load(std::memory_order_relaxed)) {
                throw std::runtime_error("Can't create consumer group, event bus setup is done.");
            }
            if (!does_topic_exist(topic_name)) {
                throw std::runtime_error("Topic is not valid for this consumer group.");
            }
            if (topic_name_by_consumer_group_id_.find(group_id) != topic_name_by_consumer_group_id_.end()) {
                throw std::runtime_error("Consumer group already assigned to topic - " + topic_name_by_consumer_group_id_.at(group_id));
            }

            const auto consumer_group = std::make_shared<ConsumerGroup>(group_id,
                topics_.at(topic_name).partition_count()); // using shared_ptr here to avoid dangling pointer problem when vector grows

            consumer_groups_by_topic_name_[topic_name].push_back(consumer_group);

            topic_name_by_consumer_group_id_[group_id] = topic_name;

            return consumer_group;
        }

        bool publish_event(const Event& event, const std::string& partition_key = "") {

            if (!does_topic_exist(event.topic)) {
                throw std::runtime_error("Topic does not exist to publish.");
            }

            auto consumer_groups_by_topic_name_it = consumer_groups_by_topic_name_.find(event.topic);

            if (consumer_groups_by_topic_name_it == consumer_groups_by_topic_name_.end()) {
                return false; // No consumer groups for this topic, drop message
            }

            const std::vector<std::shared_ptr<ConsumerGroup>>& consumer_groups = consumer_groups_by_topic_name_it->second;

            event.id = get_next_message_id_for_topic(event.topic); // ideally we should create a wrapper here on event and store metadata like id on top level of that wrapper

            const size_t partition_index = get_partition_index(event.id,
                    topics_.at(event.topic).partition_count(), partition_key);

            bool all_succeeded = true;
            for (auto& consumer_group : consumer_groups) { // fan out to all groups
                const bool success = consumer_group-> deliver_event_to_consumer_group(event, partition_index, backpressure_handler_);
                all_succeeded = all_succeeded && success;
            }
            return all_succeeded;
        }


    private:
        std::unordered_map<std::string, Topic> topics_;
        std::unordered_map<std::string, std::vector<std::shared_ptr<ConsumerGroup>>> consumer_groups_by_topic_name_;
        std::unordered_map<std::string, size_t> message_id_by_topic_name_;
        std::unordered_map<std::string, std::string> topic_name_by_consumer_group_id_;
        std::atomic<bool> set_up_done_{false};
        BackPressureHandler backpressure_handler_;

        bool does_topic_exist(const std::string &topic_name) {
            if (topics_.find(topic_name) != topics_.end()) {
                return true;
            }
            return false;
        }

        static size_t get_partition_index(const size_t event_id, const size_t partition_count,
            const std::string& partition_key) {
            if (partition_key.empty()) {
                return event_id % partition_count; // round robin
            }
            return std::hash<std::string>{}(partition_key) % partition_count; // key based hashing
        }

        size_t get_next_message_id_for_topic(const std::string& topic_name) {
            return message_id_by_topic_name_[topic_name]++;
        }
    };
}
