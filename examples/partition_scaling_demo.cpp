#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <iomanip>
#include "event_bus.hpp"
#include "consumer.hpp"
#include "event.hpp"

using namespace eventbus;
using namespace std::chrono_literals;

/**
 * Partition Scaling Demo
 * 
 * WHAT WE ARE TESTING:
 * - How partition count affects system throughput under identical workloads
 * - Whether the lock-free event bus scales linearly with increased parallelism
 * - Load distribution effectiveness across multiple consumers
 *
 * TESTING SETUP:
 * Test 1: 1 partition, 1 consumer (baseline - no parallelism)
 *   - Single producer publishes 10,000 events to 1 partition
 *   - Single consumer processes all events sequentially
 *   - Measures baseline throughput without parallel processing
 *
 * Test 2: 4 partitions, 4 consumers (optimal parallelism)
 *   - Single producer publishes 10,000 events distributed across 4 partitions
 *   - 4 consumers process events in parallel (1 consumer per partition)
 *   - Tests if 4x partitions yield proportional throughput increase
 *
 * Test 3: 8 partitions, 8 consumers
 *   - Single producer publishes 10,000 events distributed across 8 partitions
 *   - 8 consumers process events (1 partitions per consumer)

 *
 * Test 4: 15 partitions, 15 consumers (over-partitioned scenario)
 *   - Single producer publishes 10,000 events distributed across 15 partitions
 *   - 15 consumers process events
 *   - Tests diminishing returns of excessive partitioning
 *
 * EXPECTED RESULTS:
 * - Test 2 should show 3-4x throughput improvement over Test 1
 * - Test 3 should show additional gains but with diminishing returns
 * - Load should distribute evenly across consumers in each test
 * - Zero message loss across all configurations
 *
 * KEY METRICS MEASURED:
 * - Total throughput (messages/second)
 * - Events processed per consumer (load distribution)
 * - Scaling improvement ratio compared to baseline
 *
 * TECHNICAL VALIDATION:
 * This test validates that the lock-free MPSC queue architecture enables
 * true horizontal scaling through partitioning, a core requirement for
 * production messaging systems like Apache Kafka.
 */

class PartitionScalingDemo {
    std::atomic<long> total_published_{0};
    std::atomic<long> total_consumed_{0};

public:
    void producer_thread(EventBus& bus, const std::string& topic, int num_events) {
        auto start_time = std::chrono::steady_clock::now();

        long published = 0;
        for (int i = 0; i < num_events; ++i) {
            std::string payload = "{";
            payload += "\"trade_id\":" + std::to_string(i) + ",";
            payload += "\"timestamp\":" + std::to_string(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()) + ",";
            payload += R"("symbol":"STOCK)" + std::to_string(i % 100) + "\",";
            payload += "\"price\":" + std::to_string(100.0 + (i % 500) * 0.01) + ",";
            payload += "\"quantity\":" + std::to_string(1 + (i % 1000)) + ",";
            payload += R"("side":")" + std::string((i % 2) ? "BUY" : "SELL") + "\",";
            payload += R"("user":"TRADER)" + std::to_string(i % 20) + "\",";
            payload += R"("venue":"EXCHANGE)" + std::to_string(i % 5) + "\"";
            payload += "}";
            Event event(topic, payload);
            if (bus.publish_event(event)) {
                published++;
            }
        }

        const auto end_time = std::chrono::steady_clock::now();
        const auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        total_published_.store(published);

        // Only show producer completion time
        std::cout << "Producer completed: " << published << " events in "
                 << duration_ms.count() << "ms\n";
    }

    void consumer_thread(const Consumer& consumer, const int consumer_id, const int target_events) {
        long events_processed = 0;
        while (events_processed < target_events) {
            auto events = consumer.poll_batch(100);
            if (!events.empty()) {
                for (const auto& event : events) {
                    volatile int x = 0;
                    // artifical cpu work
                    for (int i = 0; i < 1000000; ++i) {
                        x += i % 7;
                    }
                }

                events_processed += events.size();
            } else {
                //std::cout << "Empty events processed\n";
                // Brief sleep when no events available
                //std::this_thread::sleep_for(100us);
            }
        }

        total_consumed_.fetch_add(events_processed);
        std::cout << "Consumer " << consumer_id << ": " << events_processed << " events\n";
    }

    double run_partition_test(const int partitions, const int consumers, int events_to_publish) {
        std::cout << "\n--- Testing " << partitions << " partitions, "
                 << consumers << " consumers ---\n";

        // Reset counters
        total_published_ = 0;
        total_consumed_ = 0;

        // Create configuration
        const EventBusConfig config {
            .topics = {
                {"scaling_test", static_cast<size_t>(partitions)}
            },
            .consumer_groups = {
                {"processors", "scaling_test", static_cast<size_t>(consumers)}
            }
        };

        constexpr BackPressureConfig back_pressure_config {
            BackPressureStrategy::BLOCK
        };

        EventBus event_bus(config, back_pressure_config);
        auto& consumer_group = event_bus.consumers_by_consumer_group_id().at("processors");

        const auto processing_start = std::chrono::steady_clock::now();

        // Start consumers with fixed event targets
        std::vector<std::thread> consumer_threads;

        // this assumes we are doing round robin distribution across partitions and  our total events is divisible by total consumers.
        // This is to ensure proper measurement as we are not stopping consumer thread until we consume this target event.
        // So in case of message drop or anything, the consumer will run indefinity, To make sure no message drop, we can change our
        // Backpressure strategy to BLOCKING, default is DROP_NEWEST
        int events_per_consumer = events_to_publish / consumers;

        for (size_t i = 0; i < consumer_group.size(); ++i) {
            consumer_threads.emplace_back(&PartitionScalingDemo::consumer_thread, this,
                                        std::ref(*consumer_group[i]), static_cast<int>(i),
                                        events_per_consumer);
        }

        // Brief delay for consumers to start
        std::this_thread::sleep_for(50ms);

        // Start producer
        std::thread producer(&PartitionScalingDemo::producer_thread, this,
                           std::ref(event_bus), "scaling_test", events_to_publish);

        // Wait for producer to complete
        producer.join();

        // Wait for all consumers to finish their fixed workload
        for (auto& t : consumer_threads) {
            t.join();
        }

        const auto processing_end = std::chrono::steady_clock::now();
        const auto processing_duration = std::chrono::duration_cast<std::chrono::milliseconds>(processing_end - processing_start);

        // Calculate throughput based on actual processing time
        double throughput = 0.0;
        if (processing_duration.count() > 0) {
            throughput = (total_consumed_.load() * 1000.0) / processing_duration.count();
        }

        // Clean summary
        std::cout << "Results: " << total_consumed_.load() << "/" << total_published_.load()
                 << " events processed in " << processing_duration.count() << "ms\n";
        std::cout << "Throughput: " << std::fixed << std::setprecision(0) << throughput << " msg/sec\n";

        return throughput;
    }

    void run_scaling_demonstration() {
        std::cout << "=== Partition Scaling Demonstration ===\n\n";
        std::cout << "Testing the same workload across different partition configurations\n";
        std::cout << "to demonstrate horizontal scaling through parallelism.\n";

        // Test configurations: {partitions, consumers, description}
        std::vector<std::tuple<int, int, std::string>> test_configs = {
            {1, 1, "Baseline: Single Partition"},
            {4, 4, "Scaled: 4 Partitions"},
            {8, 8, "Scaled: 8 Partitions, 8 Consumers"},
            {15, 15, "Scaled: 15 Partitions, 15 Consumers"}
        };

        std::vector<double> throughput_results;

        for (const auto& [partitions, consumers, description] : test_configs) {
            constexpr int TEST_EVENTS = 10000;
            std::cout << "\n" << std::string(50, '=') << "\n";
            std::cout << description << "\n";
            std::cout << std::string(50, '=') << "\n";

            double throughput = run_partition_test(partitions, consumers, TEST_EVENTS);
            throughput_results.push_back(throughput);

            std::this_thread::sleep_for(500ms); // Brief pause between tests
        }

        // Performance Analysis
        std::cout << "\n" << std::string(50, '=') << "\n";
        std::cout << "SCALING ANALYSIS\n";
        std::cout << std::string(50, '=') << "\n";

        double baseline_throughput = throughput_results[0];

        std::cout << "\nThroughput Comparison:\n";
        std::cout << "1 Partition:  " << std::fixed << std::setprecision(0)
                 << throughput_results[0] << " msg/sec (baseline)\n";
        std::cout << "4 Partitions: " << std::fixed << std::setprecision(0)
                 << throughput_results[1] << " msg/sec (";

        if (baseline_throughput > 0) {
            double improvement_4p = throughput_results[1] / baseline_throughput;
            std::cout << std::fixed << std::setprecision(1) << improvement_4p << "x improvement)\n";
        } else {
            std::cout << "N/A)\n";
        }

        std::cout << "8 Partitions: " << std::fixed << std::setprecision(0)
                 << throughput_results[2] << " msg/sec (";

        if (baseline_throughput > 0) {
            double improvement_8p = throughput_results[2] / baseline_throughput;
            std::cout << std::fixed << std::setprecision(1) << improvement_8p << "x improvement)\n";
        } else {
            std::cout << "N/A)\n";
        }

        std::cout << "15 Partitions: " << std::fixed << std::setprecision(0)
         << throughput_results[3] << " msg/sec (";

        if (baseline_throughput > 0) {
            double improvement_15p = throughput_results[3] / baseline_throughput;
            std::cout << std::fixed << std::setprecision(1) << improvement_15p << "x improvement)\n";
        } else {
            std::cout << "N/A)\n";
        }
    }
};

int main() {
    try {
        PartitionScalingDemo demo;
        demo.run_scaling_demonstration();
        
    } catch (const std::exception& e) {
        std::cerr << "Demo failed: " << e.what() << "\n";
        return 1;
    }

    return 0;
}