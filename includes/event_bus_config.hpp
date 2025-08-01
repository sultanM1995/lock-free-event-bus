#pragma once
#include <string>
#include <vector>

namespace eventbus {
    struct TopicConfig {
        std::string name;
        size_t partition_count;
    };

    struct ConsumerGroupConfig {
        std::string group_id;
        std::string topic_name;
        size_t consumer_count;
    };
    struct EventBusConfig {
        std::vector<TopicConfig> topics;
        std::vector<ConsumerGroupConfig> consumer_groups;

    };
}