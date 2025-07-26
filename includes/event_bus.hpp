#pragma once
#include <unordered_map>
#include <vector>
#include <string>

#include "consumer_group.hpp"
#include "event.hpp"
#include "lock_free_spsc_queue.hpp"
#include "topic.hpp"

namespace eventbus {
    using queue_ptr = std::shared_ptr<LockFreeSpscQueue<Event>>;

    class EventBus {

    public:
        void set_up_done() {
            set_up_done_.store(true, std::memory_order_relaxed);
        }

        void create_topic(const std::string& topic_name, const int partition_count) {
            if (set_up_done_.load(std::memory_order_relaxed)) {
                throw std::runtime_error("Can't create topic, event bus setup is done.");
            }
            if (does_topic_exist(topic_name)) {
                throw std::runtime_error("Topic already exists.");
            }
            topics_[topic_name] = std::make_unique<Topic>(topic_name, partition_count);
        }

        std::shared_ptr<ConsumerGroup> create_consumer_group(const std::string& group_id, const std::string& topic_name,
            const size_t group_size) {
            if (set_up_done_.load(std::memory_order_relaxed)) {
                throw std::runtime_error("Can't create consumer group, event bus setup is done.");
            }
            if (!does_topic_exist(topic_name)) {
                throw std::runtime_error("Topic is not valid for this consumer group.");
            }

            const auto consumer_group = std::make_shared<ConsumerGroup>(group_id, group_size,
                topics_[topic_name] -> partition_count());

            consumer_groups_by_topic_name_[topic_name].push_back(consumer_group);

            return consumer_group;
        }

        void publish_event(const Event& event, const std::string& partition_key = "") {

            if (!does_topic_exist(event.topic)) {
                throw std::runtime_error("Topic does not exist to publish.");
            }

            auto consumer_groups_by_topic_name_it = consumer_groups_by_topic_name_.find(event.topic);

            if (consumer_groups_by_topic_name_it == consumer_groups_by_topic_name_.end()) {
                return; // No consumer groups for this topic, drop message
            }

            const std::vector<std::shared_ptr<ConsumerGroup>>& consumer_groups =
                consumer_groups_by_topic_name_.at(event.topic);

            event.id = get_next_message_id_for_topic(event.topic); // ideally we should create a wrapper here on event and store metadata like id on top level of that wrapper

            for (auto& consumer_group : consumer_groups) { // fan out to all groups
                consumer_group-> deliver_event_to_consumer_group(event, get_partition_index(event.id,
                    topics_.at(event.topic) -> partition_count(), partition_key));
            }
        }


    private:
        std::unordered_map<std::string, std::unique_ptr<Topic>> topics_;
        std::unordered_map<std::string, std::vector<std::shared_ptr<ConsumerGroup>>> consumer_groups_by_topic_name_;
        std::unordered_map<std::string, size_t> message_id_by_topic_name_;
        std::atomic<bool> set_up_done_{false};

        bool does_topic_exist(const std::string &topic_name) {
            if (topics_.find(topic_name) != topics_.end()) {
                return true;
            }
            return false;
        }

        static size_t get_partition_index(const size_t event_id, const size_t partition_count,
            const std::string& partition_key) {
            if (partition_key.empty()) {
                return event_id % partition_count;
            }
            return std::hash<std::string>{}(partition_key) % partition_count;
        }

        size_t get_next_message_id_for_topic(const std::string& topic_name) {
            return message_id_by_topic_name_[topic_name]++;
        }
    };
}
