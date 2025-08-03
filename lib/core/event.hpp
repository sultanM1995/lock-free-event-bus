#pragma once
#include <string>
#include <utility>

namespace eventbus {
    struct Event {
        std::string topic;
        std::string payload;
        mutable std::size_t id{};
        std::chrono::steady_clock::time_point timestamp;

        Event () = default;

        Event(std::string topic, std::string payload): topic(std::move(topic)), payload(std::move(payload)),
                                                       timestamp(std::chrono::steady_clock::now()) {}
    };
}
