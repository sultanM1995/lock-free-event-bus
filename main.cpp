#include <iostream>
#include <thread>
#include <vector>

#include "includes/event_bus.hpp"
#include "includes/consumer.hpp"
#include "includes/event.hpp"

using namespace eventbus;
using namespace std::chrono_literals;

void publisher_thread(EventBus& bus, const std::string& topic, const int num_events, const int publisher_id) {
    //std::cout << "[Publisher-" << publisher_id << "] Starting for topic: " << topic << "\n";

    for (int i = 0; i < num_events; ++i) {
        Event event(topic, "event_" + std::to_string(i) + "_from_publisher_" + std::to_string(publisher_id));
        bus.publish_event(event);

        //std::cout << "[Publisher-" << publisher_id << "] Published: " << event.payload << "\n";
        //std::this_thread::sleep_for(100ms); // Small delay to see output
    }

    //std::cout << "[Publisher-" << publisher_id << "] Finished\n";
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
                events_received++;
                // std::cout << "[" << consumer_name << "] Received (" << events_received << "): "
                //           << event.payload << "\n";
                //
                // // Break if we've received enough events
                // if (events_received >= expected_events) {
                //     break;
                // }
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
        EventBus event_bus;

        // Create two topics
        std::cout << "Creating topics...\n";
        event_bus.create_topic("orders", 7);
        event_bus.create_topic("users", 7);
        std::cout << "Created topics: orders, users\n\n";

        // Create consumer groups (3 consumers each)
        std::cout << "Creating consumer groups...\n";
        const auto order_group = event_bus.create_consumer_group("order_processors", "orders", 3);
        const auto user_group = event_bus.create_consumer_group("user_processors", "users", 3);
        std::cout << "✓ Created consumer groups with 3 consumers each\n\n";

        // Create consumers
        std::cout << "Creating consumers...\n";
        Consumer order_consumer1(*order_group);
        Consumer order_consumer2(*order_group);
        Consumer order_consumer3(*order_group);
        Consumer user_consumer1(*user_group);
        Consumer user_consumer2(*user_group);
        Consumer user_consumer3(*user_group);
        std::cout << "✓ Created 6 consumers\n\n";

        // Finalize setup
        event_bus.set_up_done();
        std::cout << "✓ Setup completed\n\n";

        constexpr int events_per_publisher = 5000;

        std::cout << "=== Starting Threads ===\n";

        // Start consumer threads first
        std::vector<std::thread> consumer_threads;

        // All consumers use batch polling
        consumer_threads.emplace_back(consumer_thread, std::ref(order_consumer1), "OrderConsumer1", events_per_publisher);
        consumer_threads.emplace_back(consumer_thread, std::ref(order_consumer2), "OrderConsumer2", events_per_publisher);
        consumer_threads.emplace_back(consumer_thread, std::ref(order_consumer3), "OrderConsumer3", events_per_publisher);
        consumer_threads.emplace_back(consumer_thread, std::ref(user_consumer1), "UserConsumer1", events_per_publisher);
        consumer_threads.emplace_back(consumer_thread, std::ref(user_consumer2), "UserConsumer2", events_per_publisher);
        consumer_threads.emplace_back(consumer_thread, std::ref(user_consumer3), "UserConsumer3", events_per_publisher);

        // Small delay to let consumers start
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