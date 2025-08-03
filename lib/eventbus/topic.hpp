#pragma once
#include <string>
#include <utility>

namespace eventbus {
    class Topic {
    public:
        explicit Topic(std::string name, const size_t partition_count):
        name_(std::move(name)),
        partition_count_(partition_count){}


        [[nodiscard]] const std::string& name() {
            return name_;
        }

        [[nodiscard]] size_t partition_count() const {
            return partition_count_;
        }

    private:
        std::string name_;
        size_t partition_count_;
    };
}

