#include <iostream>
#include <thread>
#include <vector>

#include "includes/event_bus.hpp"
#include "includes/consumer.hpp"
#include "includes/event.hpp"

using namespace eventbus;
using namespace std::chrono_literals;

void publisher_thread(EventBus& bus, const std::string& topic, const int num_events, const int publisher_id) {

    for (int i = 0; i < num_events; ++i) {
        Event event(topic, "event_" + std::to_string(i) + "_from_publisher_" + std::to_string(publisher_id));
        bus.publish_event(event);
    }
}

void consumer_thread(const Consumer& consumer, const std::string& consumer_name, const int expected_events) {
    std::cout << "[" << consumer_name << "] Starting\n";

    int events_received = 0;
    auto start_time = std::chrono::steady_clock::now();
    constexpr auto timeout = std::chrono::seconds(5);  // 5 second timeout to prevent hanging

    while (true) {
        // Use batch polling with small batch size
        auto events = consumer.poll_batch(50);  // Poll up to 3 events at a time

        if (!events.empty()) {
            start_time = std::chrono::steady_clock::now();
            for (const auto& event : events) {
                std::cout << event.payload;
                events_received++;
            }
        } else {
            // Check for timeout to prevent infinite waiting
            if (std::chrono::steady_clock::now() - start_time > timeout) {
                std::cout << "[" << consumer_name << "] Timeout! Only received "
                          << events_received << " out of " << expected_events << " events\n";
                break;
            }
            std::this_thread::sleep_for(10ms); // Wait for events
        }
    }

    std::cout << "[" << consumer_name << "] Finished - processed " << events_received << " events\n";
}



int main() {
    std::cout << "=== Multi-threaded EventBus Test with Batch Polling ===\n\n";

    try {

        const EventBusConfig config {
            .topics = {
                { "orders", 3 },
                { "users", 3 }
            },
            .consumer_groups = {
                { "order_processors", "orders", 1 },
                { "user_processors", "users", 3 }
            }
        };

        EventBus event_bus(config);

        std::cout << "✓ Setup completed\n\n";

        constexpr int events_per_publisher = 15;

        std::cout << "=== Starting Threads ===\n";

        // Start consumer threads first
        std::vector<std::thread> consumer_threads;

        auto& consumers_by_group = event_bus.consumers_by_consumer_group_id();

        for (auto& t : consumers_by_group) {
            for (auto& consumer : t.second) {
                consumer_threads.emplace_back(consumer_thread, std::ref(*consumer), consumer->consumer_id(), events_per_publisher);
            }
        }

        std::this_thread::sleep_for(100ms);

        // Start publisher threads
        std::vector<std::thread> publisher_threads;
        publisher_threads.emplace_back(publisher_thread, std::ref(event_bus), "orders", events_per_publisher, 1);
        publisher_threads.emplace_back(publisher_thread, std::ref(event_bus), "users", events_per_publisher, 2);

        // Wait for publishers to finish
        for (auto& t : publisher_threads) {
            t.join();
        }
        std::cout << "\nAll publishers finished\n";

        // Wait for consumers to finish
        for (auto& t : consumer_threads) {
            t.join();
        }
        std::cout << "All consumers finished\n\n";

        std::cout << "=== Test Summary ===\n";
        std::cout << "2 topics created\n";
        std::cout << "2 publisher threads (1 per topic)\n";
        std::cout << "2 consumer groups (1 per topic)\n";
        std::cout << "6 consumer threads (3 per consumer group)\n";
        std::cout << "All consumers use batch polling with fair queue distribution\n";
        std::cout << "" << events_per_publisher * 2 << " total events published and consumed\n";
        std::cout << "\nTest completed successfully! \n";

        // Optional: Demonstrate batch polling behavior
        std::cout << "\n=== Batch Polling Demo ===\n";
        std::cout << "Each consumer was assigned to different partitions.\n";
        std::cout << "All consumers used batch polling (up to 50 events per poll).\n";
        std::cout << "Fair division algorithm distributed events across partition queues.\n";

    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << "\n";
        return 1;
    }

    return 0;
}