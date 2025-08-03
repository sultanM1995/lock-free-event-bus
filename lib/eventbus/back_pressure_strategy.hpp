#pragma once
#include <chrono>
#include <thread>

namespace eventbus {
    enum class BackPressureStrategy {
        DROP_NEWEST,        // Drop incoming events when queue is full
        BLOCK,              // Block until space is available
        SPIN,               // Busy spin until space is available
        YIELDING_SPIN       // Spin with periodic yields
    };

    struct BackPressureConfig {
        BackPressureStrategy strategy = BackPressureStrategy::DROP_NEWEST;

        // For spinning strategies
        int spin_yield_threshold = 1000;

        // For blocking strategy
        std::chrono::microseconds block_sleep_duration{10};
        std::chrono::milliseconds timeout{1000}; // Max time to wait before giving up for spin and yielding spin
    };

    class BackPressureHandler {
    public:
        explicit BackPressureHandler(const BackPressureConfig& config = {}) : config_(config) {}

        template<typename QueueType, typename EventType>
        bool try_enqueue_with_backpressure_strategy(const QueueType& queue, const EventType& event) const {
            switch (config_.strategy) {
                case BackPressureStrategy::DROP_NEWEST:
                    return handle_drop_newest(queue, event);

                case BackPressureStrategy::BLOCK:
                    return handle_blocking(queue, event);

                case BackPressureStrategy::SPIN:
                    return handle_spinning(queue, event);

                case BackPressureStrategy::YIELDING_SPIN:
                    return handle_yielding_spin(queue, event);

                default:
                    return handle_drop_newest(queue, event);
            }
        }
    private:
        BackPressureConfig config_;

        template<typename QueueType, typename EventType>
        bool handle_drop_newest(const QueueType& queue, const EventType& event) const{
            // Simply try to enqueue, drop if queue is full
            return queue->enqueue(event);
        }

        template<typename QueueType, typename EventType>
        bool handle_blocking(const QueueType& queue, const EventType& event) const {
            while (!queue->enqueue(event)) {
                std::this_thread::sleep_for(config_.block_sleep_duration);
            }
            return true;
        }

        template<typename QueueType, typename EventType>
        bool handle_spinning(const QueueType& queue, const EventType& event) const {
            const auto start_time = std::chrono::steady_clock::now();

            while (!queue->enqueue(event)) {
                // Check timeout to prevent infinite spinning
                if (std::chrono::steady_clock::now() - start_time > config_.timeout) {
                    return false; // Timeout, give up
                }
            }

            return true;
        }

        template<typename QueueType, typename EventType>
        bool handle_yielding_spin(const QueueType& queue, const EventType& event) const {
            const auto start_time = std::chrono::steady_clock::now();
            int spin_count = 0;

            while (!queue->enqueue(event)) {
                // Check timeout
                if (std::chrono::steady_clock::now() - start_time > config_.timeout) {
                    return false; // Timeout, give up
                }
                ++spin_count;
                if (spin_count >= config_.spin_yield_threshold) {
                    std::this_thread::yield(); // Give other threads a chance
                    spin_count = 0; // Reset counter
                }
            }
            return true;
        }
    };
}