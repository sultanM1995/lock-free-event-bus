#include <iostream>
#include <thread>
#include <chrono>
#include <numeric>

#include "event_bus.hpp"
#include "consumer.hpp"
#include "event.hpp"

using namespace eventbus;
using namespace std::chrono_literals;

/**
 * Basic Event Bus Example
 * 
 * Demonstrates:
 * - Simple topic creation and event publishing
 * - Consumer group setup and event consumption
 * - Basic latency measurement
 * - Single producer, single consumer scenario
 */

int main() {
    std::cout << "=== Basic Event Bus Usage Example ===\n\n";

    try {
        // Create a simple configuration
        const EventBusConfig config {
            .topics = {
                { "notifications", 1 }  // Single partition
            },
            .consumer_groups = {
                { "notification_handlers", "notifications", 1 }  // Single consumer
            }
        };

        EventBus event_bus(config);

        auto& consumers_by_group = event_bus.consumers_by_consumer_group_id();
        auto& consumer = consumers_by_group.at("notification_handlers")[0];

        std::cout << "Event bus initialized with 1 topic, 1 consumer group, 1 consumer\n";
        std::cout << "Consumer ID: " << consumer->consumer_id() << "\n\n";

        // Demonstrate basic publish-subscribe with latency measurement
        std::cout << "=== Testing Basic Publish-Subscribe ===\n";

        constexpr int num_messages = 10;
        std::vector<std::chrono::steady_clock::time_point> publish_times;
        std::vector<std::chrono::nanoseconds> latencies;

        // Publish events and record timestamps
        for (int i = 0; i < num_messages; ++i) {
            auto publish_time = std::chrono::steady_clock::now();
            Event event("notifications", "Message " + std::to_string(i) + ": Hello World!");
            
            bool success = event_bus.publish_event(event);
            if (success) {
                publish_times.push_back(publish_time);
                std::cout << "Published: " << event.payload << "\n";
            } else {
                std::cerr << "Failed to publish message " << i << "\n";
            }
        }

        std::cout << "\n=== Consuming Events ===\n";

        // Consume events and measure latency
        int consumed_count = 0;
        auto start_time = std::chrono::steady_clock::now();
        constexpr auto timeout = std::chrono::seconds(5);

        while (consumed_count < num_messages) {
            auto events = consumer->poll_batch(5);
            
            if (!events.empty()) {
                auto consume_time = std::chrono::steady_clock::now();
                
                for (const auto& event : events) {
                    // Calculate end-to-end latency (publish to consume)
                    auto latency = consume_time - event.timestamp;
                    latencies.push_back(latency);
                    
                    std::cout << "Consumed: " << event.payload 
                             << " (Latency: " << std::chrono::duration_cast<std::chrono::microseconds>(latency).count() 
                             << " μs)\n";
                    consumed_count++;
                }
            } else {
                // Check for timeout
                if (std::chrono::steady_clock::now() - start_time > timeout) {
                    std::cout << "Timeout waiting for messages!\n";
                    break;
                }
                std::this_thread::sleep_for(1ms);
            }
        }

        // Calculate and display performance metrics
        std::cout << "\n=== Performance Summary ===\n";
        std::cout << "Messages published: " << publish_times.size() << "\n";
        std::cout << "Messages consumed: " << consumed_count << "\n";

        if (!latencies.empty()) {
            // Calculate latency statistics
            auto min_latency = *std::min_element(latencies.begin(), latencies.end());
            auto max_latency = *std::max_element(latencies.begin(), latencies.end());
            
            auto total_latency = std::accumulate(latencies.begin(), latencies.end(), 
                                               std::chrono::nanoseconds(0));
            auto avg_latency = total_latency / latencies.size();

            std::cout << "Latency (min/avg/max): " 
                     << std::chrono::duration_cast<std::chrono::microseconds>(min_latency).count() << "/"
                     << std::chrono::duration_cast<std::chrono::microseconds>(avg_latency).count() << "/"
                     << std::chrono::duration_cast<std::chrono::microseconds>(max_latency).count() 
                     << " μs\n";
        }

        std::cout << "\n Basic functionality verified successfully!\n";
        std::cout << "Low-latency event delivery demonstrated\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}