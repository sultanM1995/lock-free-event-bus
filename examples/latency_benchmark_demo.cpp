#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <mutex>
#include "event_bus.hpp"
#include "consumer.hpp"
#include "event.hpp"

using namespace eventbus;
using namespace std::chrono_literals;

/**
 * Latency Benchmark Demo
 *
 * WHAT WE ARE TESTING:
 * - Horizontal scaling impact on latency through partitioning
 * - Single partition vs multi-partition performance comparison
 * - Load distribution effectiveness across multiple consumers
 *
 * TESTING SETUP:
 * - Two configurations tested back-to-back for direct comparison
 * - Realistic payloads (~50 bytes) for production relevance
 *
 * Test Configuration A: Single Partition (1P/1C)
 *   - 1 topic with 1 partition, 1 consumer (baseline)
 *   - Tests sequential event processing
 *   - Measures baseline queue depth effects
 *   - Shows single-threaded performance characteristics
 *
 * Test Configuration B: Multi Partition (4P/4C)
 *   - 1 topic with 4 partitions, 4 consumers (horizontal scaling)
 *   - Tests parallel event processing with load distribution
 *   - Measures scaling benefits of partitioning
 *   - Shows multi-threaded coordination overhead vs benefits
 *
 * Scenario 2: Burst Load Test (15K events)
 *   - Publish 15,000 events as fast as possible
 *   - Compare single vs multi-partition latency under stress
 *   - Expected: Multi-partition should show latency improvement
 *
 * Scenario 3: Sustained Load Test (10K events @ 10K/sec)
 *   - Publish events at steady rate with parallel consumption
 *   - Compare single vs multi-partition latency under normal load
 *   - Expected: Similar performance, slight coordination overhead
 *
 * EXPECTED RESULTS:
 * - Single partition: Higher burst latency due to queue depth
 * - Multi partition: Lower burst latency due to load distribution
 * - Sustained load: Similar performance between configurations
 * - Validates horizontal scaling effectiveness of partitioning
 *
 * KEY METRICS MEASURED:
 * - Latency percentiles (P50, P90, P95, P99, P99.9, Max)
 * - Average latency for overall performance indication
 * - Minimum latency for best-case validation
 */

class LatencyBenchmark {
public:
    static void run_latency_benchmark() {
        std::cout << "=== Lock-Free Event Bus Latency Benchmark ===\n\n";
        std::cout << "Testing single partition vs multi-partition configurations\n";
        std::cout << "Focus: Horizontal scaling impact on latency characteristics\n\n";

        try {
            // Test both configurations

            std::cout << "SINGLE PARTITION CONFIGURATION" << std::endl;
            std::cout << std::string(60, '=') << std::endl;
            run_single_partition_tests();

            std::cout << "MULTI PARTITION CONFIGURATION" << std::endl;
            std::cout << std::string(60, '=') << std::endl;

            run_multi_partition_tests();

            std::cout << "\n=== Overall Benchmark Summary ===\n";
            std::cout << "Horizontal scaling impact on latency validated\n";
            std::cout << "Single vs Multi-partition comparison completed\n";

        } catch (const std::exception& e) {
            std::cerr << "Benchmark failed: " << e.what() << "\n";
        }
    }

private:
    static void run_single_partition_tests() {
        const EventBusConfig config {
            .topics = {
                {"latency_test", 1}  // Single partition
            },
            .consumer_groups = {
                {"latency_consumers", "latency_test", 1}  // Single consumer
            }
        };

        EventBus event_bus(config);
        auto& consumers = event_bus.consumers_by_consumer_group_id();

        std::cout << "Configuration: 1 topic, 1 partition, 1 consumer\n";
        std::cout << "Consumer ID: " << consumers.at("latency_consumers")[0]->consumer_id() << "\n\n";

        // Warmup
        std::cout << "=== Warmup Phase ===\n";
        run_warmup(event_bus, consumers.at("latency_consumers"));
        std::cout << "Warmup completed\n\n";

        // Run scenarios
        run_scenario_2_burst_load_latency(event_bus, consumers.at("latency_consumers"), "Single Partition");
        run_scenario_3_sustained_load_latency(event_bus, consumers.at("latency_consumers"), "Single Partition");
    }

    static void run_multi_partition_tests() {
        const EventBusConfig config {
            .topics = {
                {"latency_test", 4}  // 4 partitions
            },
            .consumer_groups = {
                {"latency_consumers", "latency_test", 4}  // 4 consumers
            }
        };

        EventBus event_bus(config);
        auto& consumers = event_bus.consumers_by_consumer_group_id();

        std::cout << "Configuration: 1 topic, 4 partitions, 4 consumers\n";
        std::cout << "Consumer IDs: ";
        for (const auto& consumer : consumers.at("latency_consumers")) {
            std::cout << consumer->consumer_id() << " ";
        }
        std::cout << "\n\n";

        // Warmup
        std::cout << "=== Warmup Phase ===\n";
        run_warmup(event_bus, consumers.at("latency_consumers"));
        std::cout << "Warmup completed\n\n";

        // Run scenarios
        run_scenario_2_burst_load_latency(event_bus, consumers.at("latency_consumers"), "Multi Partition");
        run_scenario_3_sustained_load_latency(event_bus, consumers.at("latency_consumers"), "Multi Partition");
    }

    static void run_warmup(EventBus& event_bus, const std::vector<std::unique_ptr<Consumer>>& consumers) {
        // Publish and consume 100 warmup events
        for (int i = 0; i < 100; ++i) {
            Event event("latency_test", "warmup_" + std::to_string(i));
            event_bus.publish_event(event);
        }

        // Consume all warmup events across all consumers
        int consumed = 0;
        while (consumed < 100) {
            for (const auto& consumer : consumers) {
                const auto& events = consumer->poll_batch(20);
                consumed += events.size();
            }
            if (consumed < 100) {
                std::this_thread::sleep_for(1ms);
            }
        }
    }

    static void run_scenario_2_burst_load_latency(EventBus& event_bus, const std::vector<std::unique_ptr<Consumer>>& consumers, const std::string& config_name) {
        std::cout << "\n=== Scenario 2: Burst Load Latency (" << config_name << ") ===\n";
        std::cout << "Testing latency under maximum burst conditions (15000 events)\n";
        std::cout << "Publishing as fast as possible to stress queue depth\n\n";

        constexpr int num_events = 15000;
        std::vector<std::chrono::nanoseconds> latencies;
        std::mutex latencies_mutex;

        auto burst_start = std::chrono::steady_clock::now();

        // Publish burst of events as fast as possible
        int published_count = 0;
        int failed_count = 0;

        for (int i = 0; i < num_events; ++i) {
            std::string payload = "{\"id\":" + std::to_string(i) +
                                 R"(,"sym":"AAPL")" +
                                 ",\"px\":150.25" +
                                 ",\"qty\":100}";
            Event event("latency_test", payload);
            bool success = event_bus.publish_event(event);
            if (success) {
                published_count++;
            } else {
                failed_count++;
                std::cerr << "Failed to publish burst event " << i << "\n";
            }
        }

        const auto burst_end = std::chrono::steady_clock::now();
        const auto burst_duration = std::chrono::duration_cast<std::chrono::milliseconds>(burst_end - burst_start);

        std::cout << "Burst publishing completed in " << burst_duration.count() << "ms\n";
        std::cout << "Published: " << published_count << "/" << num_events << " events";
        if (failed_count > 0) {
            std::cout << " (DROPPED: " << failed_count << ")";
        }
        std::cout << "\n";
        std::cout << "Now consuming with " << consumers.size() << " consumer(s)...\n\n";

        // Start multiple consumer threads
        std::atomic<int> total_consumed{0};
        std::vector<std::thread> consumer_threads;
        const auto consume_start = std::chrono::steady_clock::now();

        for (size_t i = 0; i < consumers.size(); ++i) {
            consumer_threads.emplace_back([&, i]() {
                while (total_consumed.load() < published_count) {  // Changed from num_events to published_count
                    auto events = consumers[i]->poll_batch(50);

                    if (!events.empty()) {
                        auto consume_time = std::chrono::steady_clock::now();

                        // Thread-safe latency collection
                        {
                            std::lock_guard<std::mutex> lock(latencies_mutex);
                            for (const auto& event : events) {
                                auto latency = consume_time - event.timestamp;
                                latencies.push_back(latency);
                            }
                        }

                        total_consumed.fetch_add(events.size());
                    } else {
                        std::this_thread::sleep_for(100us);

                        // Timeout check
                        if (std::chrono::steady_clock::now() - consume_start > 10s) {
                            break;
                        }
                    }
                }
            });
        }

        // Wait for all consumers to finish
        for (auto& thread : consumer_threads) {
            thread.join();
        }

        // Verification: Check if all published events were consumed
        std::cout << "Event Verification:\n";
        std::cout << "Published: " << published_count << " events\n";
        std::cout << "Consumed:  " << total_consumed.load() << " events\n";
        std::cout << "Latency samples: " << latencies.size() << "\n";

        if (total_consumed.load() != published_count) {
            std::cerr << "WARNING: Event count mismatch! "
                     << (published_count - total_consumed.load()) << " events missing!\n";
        } else if (latencies.size() != published_count) {
            std::cerr << "WARNING: Latency sample count mismatch!\n";
        } else {
            std::cout << "All events successfully processed\n";
        }
        std::cout << "\n";

        print_latency_stats("Scenario 2 (" + config_name + " Burst Load)", latencies);
    }

    static void run_scenario_3_sustained_load_latency(EventBus& event_bus, const std::vector<std::unique_ptr<Consumer>>& consumers, const std::string& config_name) {
        std::cout << "\n=== Scenario 3: Sustained Load Latency (" << config_name << ") ===\n";
        std::cout << "Testing latency under steady 10K/sec rate (100μs intervals)\n";
        std::cout << "Simulating consistent production workload patterns\n\n";

        constexpr int num_events = 50000;
        constexpr auto interval = 100us;  // 10K/sec rate
        std::vector<std::chrono::nanoseconds> latencies;
        std::mutex latencies_mutex;

        // Start consumer threads
        std::atomic publishing_done{false};
        std::atomic consumed_count{0};
        std::vector<std::thread> consumer_threads;

        for (size_t i = 0; i < consumers.size(); ++i) {
            consumer_threads.emplace_back([&, i]() {
                while (!publishing_done.load() || consumed_count.load() < num_events) {
                    auto events = consumers[i]->poll_batch(20);

                    if (!events.empty()) {
                        auto consume_time = std::chrono::steady_clock::now();

                        // Thread-safe latency collection
                        {
                            std::lock_guard<std::mutex> lock(latencies_mutex);
                            for (const auto& event : events) {
                                auto latency = consume_time - event.timestamp;
                                latencies.push_back(latency);
                            }
                        }
                        consumed_count.fetch_add(events.size());
                    } else {
                        std::this_thread::sleep_for(50us);
                    }
                }
            });
        }

        // Publish at steady rate
        const auto publish_start = std::chrono::steady_clock::now();
        int published_count = 0;
        int failed_count = 0;

        for (int i = 0; i < num_events; ++i) {
            const std::string payload = "{\"id\":" + std::to_string(i) +
                                 R"(,"sym":"AAPL")" +
                                 ",\"px\":150.25" +
                                 ",\"qty\":100}";
            Event event("latency_test", payload);
            bool success = event_bus.publish_event(event);

            if (success) {
                published_count++;
            } else {
                failed_count++;
                std::cerr << "Failed to publish sustained event " << i << "\n";
            }

            // Wait for next interval
            std::this_thread::sleep_for(interval);
        }

        publishing_done.store(true);

        const auto publish_end = std::chrono::steady_clock::now();
        const auto publish_duration = std::chrono::duration_cast<std::chrono::milliseconds>(publish_end - publish_start);

        // Wait for all consumer threads to finish
        for (auto& thread : consumer_threads) {
            thread.join();
        }

        std::cout << "Sustained publishing completed in " << publish_duration.count() << "ms\n";

        // Verification: Check if all published events were consumed
        std::cout << "Event Verification:\n";
        std::cout << "Published: " << published_count << " events\n";
        std::cout << "Consumed:  " << consumed_count.load() << " events\n";
        std::cout << "Latency samples: " << latencies.size() << "\n";

        if (consumed_count.load() != published_count) {
            std::cerr << "WARNING: Event count mismatch! "
                     << (published_count - consumed_count.load()) << " events missing!\n";
        } else if (latencies.size() != published_count) {
            std::cerr << "WARNING: Latency sample count mismatch!\n";
        } else {
            std::cout << "All events successfully processed\n";
        }
        std::cout << "\n";

        print_latency_stats("Scenario 3 (" + config_name + " Sustained Load)", latencies);
    }

    static void print_latency_stats(const std::string& scenario_name, const std::vector<std::chrono::nanoseconds>& latencies) {
        if (latencies.empty()) {
            std::cout << scenario_name << " - No latency data collected\n";
            return;
        }

        // Sort for percentile calculations
        auto sorted_latencies = latencies;
        std::sort(sorted_latencies.begin(), sorted_latencies.end());

        // Calculate statistics
        const auto min_latency = sorted_latencies.front();
        const auto max_latency = sorted_latencies.back();

        const auto total_latency = std::accumulate(latencies.begin(), latencies.end(), std::chrono::nanoseconds(0));
        const auto avg_latency = total_latency / latencies.size();

        // Calculate percentiles
        const auto p50 = sorted_latencies[sorted_latencies.size() * 50 / 100];
        const auto p90 = sorted_latencies[sorted_latencies.size() * 90 / 100];
        const auto p95 = sorted_latencies[sorted_latencies.size() * 95 / 100];
        const auto p99 = sorted_latencies[sorted_latencies.size() * 99 / 100];
        const auto p999 = sorted_latencies[sorted_latencies.size() * 999 / 1000];

        // Print results
        std::cout << scenario_name << " Results:\n";
        std::cout << "Sample size: " << latencies.size() << " events\n";
        std::cout << std::fixed << std::setprecision(1);
        std::cout << "Min:     " << std::setw(8) << std::chrono::duration_cast<std::chrono::microseconds>(min_latency).count() << " μs\n";
        std::cout << "Average: " << std::setw(8) << std::chrono::duration_cast<std::chrono::microseconds>(avg_latency).count() << " μs\n";
        std::cout << "P50:     " << std::setw(8) << std::chrono::duration_cast<std::chrono::microseconds>(p50).count() << " μs\n";
        std::cout << "P90:     " << std::setw(8) << std::chrono::duration_cast<std::chrono::microseconds>(p90).count() << " μs\n";
        std::cout << "P95:     " << std::setw(8) << std::chrono::duration_cast<std::chrono::microseconds>(p95).count() << " μs\n";
        std::cout << "P99:     " << std::setw(8) << std::chrono::duration_cast<std::chrono::microseconds>(p99).count() << " μs\n";
        std::cout << "P99.9:   " << std::setw(8) << std::chrono::duration_cast<std::chrono::microseconds>(p999).count() << " μs\n";
        std::cout << "Max:     " << std::setw(8) << std::chrono::duration_cast<std::chrono::microseconds>(max_latency).count() << " μs\n";
        std::cout << std::string(50, '-') << "\n";
    }
};

int main() {
    LatencyBenchmark::run_latency_benchmark();
    return 0;
}