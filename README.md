# In-Memory Lock-Free Event Bus

A kafka inspired high-performance, in-memory lock-free event bus implementation in C++ designed for low-latency, multi-threaded event processing with horizontal scaling through partitioning.

## 🚀 Performance Highlights

- **Ultra-low latency**: Sub-100μs P99 for sustained workloads
- **Burst handling**: 2.7ms P99 for 15K event bursts
- **Horizontal scaling**: 4.5x throughput improvement with optimal partitioning
- **Zero message loss**: 100% delivery guarantee with configurable back-pressure strategy
- **Lock-free design**: MPSC queues with atomic operations for maximum throughput

## 🏗️ Architecture

### Core Components

**Lock-Free MPSC Queues**: Multiple producer, single consumer queues using ring buffers with atomic head/tail pointers for wait-free enqueue and lock-free dequeue operations. Each queue has a capacity of 16,384 events.

**Topic & Partition System**: Events are distributed across configurable partitions using round-robin or key-based hashing. Each partition is backed by its own dedicated MPSC queue, created when a consumer group subscribes to the topic.

**Consumer Groups with Topic Binding**: Each consumer group subscribes to exactly one topic, while multiple consumer groups can subscribe to a single topic. This one-to-many relationship between topic and consumer group enables independent processing pipelines while maintaining clear subscription boundaries. Crucially, each consumer group receives its own complete set of partition queues, eliminating contention between different consumer groups processing the same topic.

**Multi-Publisher Support**: Multiple publisher threads can safely publish to the same topic simultaneously. The MPSC queue design enables this concurrent publishing without requiring locks or coordination between publishers, ensuring maximum throughput and minimal latency overhead.

**Intelligent Partition Assignment**: When a consumer group is assigned to a topic, partitions are distributed among consumers using round-robin assignment. If the consumer group size exceeds the partition count, some consumers will remain idle without assigned partitions.

**Fan-out Architecture**: Each event is delivered to all subscribed consumer groups, enabling true publish-subscribe semantics with multiple independent processing pipelines.

### Message Ordering Semantics

The event bus implements **Kafka-inspired ordering semantics** that balance consistency with horizontal scaling performance:

**Partition-Level Ordering (Guaranteed)**:
- Events within a single partition maintain strict FIFO order
- A consumer processing partition P0 will always receive events in the order they were published to P0
- This guarantee holds regardless of how many publishers are writing to the partition

**Cross-Partition Ordering (Not Guaranteed)**:
- Events distributed across different partitions have no ordering relationship
- Round-robin distribution means consecutive events may go to different partitions
- Each partition processes independently, enabling parallel consumption

**Key-Based Ordering (Conditional)**:
- Events with the same partition key are routed to the same partition
- All events for `user_123` will be processed in order, but no ordering guarantee between `user_123` and `user_456`
- This pattern enables ordered processing of related events while maintaining scalability
- If no partition key is provided when publishin an event from publisher, the default mechanism is round robin assignment across topic partitions.

**Consumer Batching Behavior**:
When a consumer calls `poll_batch(10)` on multiple partitions, events are returned in partition-grouped blocks:
```
Published timeline: [P0_A, P1_A, P2_A, P0_B, P1_B, P2_B]
Consumer receives:  [P0_A, P0_B, P1_A, P1_B, P2_A, P2_B]
```
This block-based consumption optimizes batching efficiency while preserving within-partition ordering.

### Architecture Flow

**📤 Publishing Layer**
- Multiple publisher threads write concurrently to the same topic
- Event Bus Router handles partition selection using atomic operations
- No coordination required between publishers - true lock-free design

**🗂️ Storage Layer**
- Topics contain 1+ partitions for horizontal scaling
- Each partition backed by dedicated 16K MPSC queue
- Round-robin or key-based event distribution across partitions
- **Independent Queue Sets**: Each consumer group gets its own copy of complete set of partition queues, enabling contention-free parallel consumption

**📥 Consumption Layer**
- Consumer groups subscribe to exactly one topic
- Consumers within group assigned partitions using round-robin distribution
- Fan-out architecture: Multiple groups can consume same topic simultaneously
- Each consumer group processes events independently without interference

**🔄 Data Flow**
1. **Publisher** → Event Bus Router (atomic operations)
2. **Router** → Partition Selection (round-robin/key-based hashing)
3. **Partition** → MPSC Queue Enqueue (lock-free, 16K capacity)
4. **Fan-out** → Copy events to all consumer groups subscribed to topic
5. **Consumer Group** → Round-robin consumer assignment within group
6. **Consumer** → Poll & process events from assigned partition queues

### Architecture Walkthrough: Trading System Example

Let's trace through a concrete example to see how the event bus handles real-world scenarios.

**Scenario**: Trading system with 3 applications publishing to "trades" topic, consumed by multiple independent services.

**System Configuration**:
```cpp
EventBusConfig config {
    .topics = {
        {"trades", 4}  // 4 partitions for parallel processing
    },
    .consumer_groups = {
        {"risk_processors", "trades", 4},    // 4 consumers for real-time risk analysis
        {"audit_loggers", "trades", 2},      // 2 consumers for compliance logging  
        {"reporting_service", "trades", 1}   // 1 consumer for daily reports
    }
};
```

**Step 1: 3 Publishers Start Publishing to the trades Topic**
```
Trading Engine → publish(buy_order_AAPL)  → Event Bus Router
Risk System   → publish(sell_order_MSFT) → Event Bus Router
UI Dashboard  → publish(cancel_order_123) → Event Bus Router
```

**Step 2: Event Bus Routes Events to Partitions Based On Internally Generated Event Id and Round Robin Fashion**
```
buy_order_AAPL   → event_id=1001 → 1001 % 4 = 1 → Partition 1
sell_order_MSFT  → event_id=1002 → 1002 % 4 = 2 → Partition 2  
cancel_order_123 → event_id=1003 → 1003 % 4 = 3 → Partition 3
```

**Step 3: Fan-out Creates Independent Queue Sets**
```
Each consumer group gets its own complete set of 4 partition queues:

risk_processors queues:    [P0_queue, P1_queue, P2_queue, P3_queue]
audit_loggers queues:      [P0_queue, P1_queue, P2_queue, P3_queue]  
reporting_service queues:  [P0_queue, P1_queue, P2_queue, P3_queue]

All 3 events are copied to each queue set - no contention between groups!
```

**Step 4: Independent Consumer Group Processing**
```
risk_processors (4 consumers - perfect distribution):
├─ Consumer 1 → processes Partition 0 events
├─ Consumer 2 → processes Partition 1 events (buy_order_AAPL)
├─ Consumer 3 → processes Partition 2 events (sell_order_MSFT)  
└─ Consumer 4 → processes Partition 3 events (cancel_order_123)

audit_loggers (2 consumers - balanced distribution):
├─ Consumer 1 → processes Partitions 0,2 events (sell_order_MSFT)
└─ Consumer 2 → processes Partitions 1,3 events (buy_order_AAPL, cancel_order_123)

reporting_service (1 consumer - handles all partitions):
└─ Consumer 1 → processes all partition events sequentially
```

**Step 5: Parallel Processing Results**
- **risk_processors**: Analyzes each trade in real-time for risk compliance
- **audit_loggers**: Records all trades for regulatory compliance
- **reporting_service**: Aggregates trades for end-of-day reports

**Key Architectural Benefits Demonstrated**:
- **Lock-free Publishing**: All 3 publishers write concurrently without blocking
- **Horizontal Scaling**: 4 partitions enable parallel processing within each consumer group
- **Independent Processing**: Each consumer group processes the same events without interference
- **Contention-Free Design**: Separate queue sets eliminate cross-group contention
- **Load Distribution**: Partition assignment optimizes processing across available consumers

**Key Architectural Relationships**:
- **Publishers → Topics**: Many-to-Many (multiple publishers can write to multiple topics)
- **Topics → Consumer Groups**: One-to-Many (each topic can have multiple consumer groups)
- **Consumer Groups → Topics**: Many-to-One (each consumer group subscribes to exactly one topic)
- **Consumer Groups → Consumers**: One-to-Many (each group contains multiple consumers)
- **Consumers → Partitions**: Many-to-Many (consumers can handle multiple partitions via round-robin assignment)

## 📋 Features

- **Lock-free concurrency**: Thread-safe publishing with atomic operations enabling multiple publishers to write simultaneously without blocking
- **Configurable partitioning**: Horizontal scaling through partition count tuning, with automatic MPSC queue creation per partition
- **Strict consumer group semantics**: Each consumer group subscribes to exactly one topic, ensuring clear data flow boundaries and simplified management
- **Intelligent partition assignment**: Round-robin distribution of partitions among consumers, with automatic handling of consumer-to-partition ratio imbalances
- **Comprehensive back-pressure strategies**: DROP_NEWEST (default), BLOCK, SPIN, YIELDING_SPIN to handle queue overflow scenarios gracefully
- **Batch consumption**: Efficient event processing with configurable batch sizes across multiple assigned partition queues
- **Memory efficient design**: Value semantics with optimized Event structure and 16K capacity queues per partition
- **Type-safe architecture**: Template-based design with compile-time safety and clear API boundaries

## 🛠️ Building

### Prerequisites

- C++17 compatible compiler
- CMake 3.14+

### Build Instructions

```bash
git clone https://github.com/sultanM1995/lock-free-event-bus.git
cd lock-free-event-bus
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### Running Examples

```bash
# Basic usage demonstration
./basic_usage_demo

# Partition scaling performance test
./partition_scaling_demo

# Latency benchmark comparison
./latency_benchmark_demo
```

## 📚 Quick Start

### Basic Usage

```cpp
#include "event_bus.hpp"
#include "consumer.hpp"
#include "event.hpp"

using namespace eventbus;

// Configure event bus
EventBusConfig config {
    .topics = {
        {"notifications", 4}  // 4 partitions
    },
    .consumer_groups = {
        {"processors", "notifications", 2}  // 2 consumers
    }
};

// Create event bus
EventBus event_bus(config);

// Get consumers
auto& consumers = event_bus.consumers_by_consumer_group_id().at("processors");

// Publish events
Event event("notifications", "{\"message\":\"Hello World!\"}");
event_bus.publish_event(event);

// Consume events
auto events = consumers[0]->poll_batch(10);
for (const auto& event : events) {
    std::cout << "Received: " << event.payload << std::endl;
}
```

### Advanced Configuration

```cpp
// Topic with multiple partitions for high-throughput scenarios
EventBusConfig config {
    .topics = {
        {"trade_events", 8},      // 8 partitions for high parallelism
        {"notifications", 2}      // 2 partitions for moderate load
    },
    .consumer_groups = {
        {"risk_processors", "trade_events", 4},    // 4 consumers, 2 partitions each
        {"audit_logger", "trade_events", 2},       // 2 consumers, 4 partitions each  
        {"email_service", "notifications", 1}      // 1 consumer, 2 partitions
    }
};

// Custom back-pressure configuration to handle queue overflow
BackPressureConfig bp_config {
    .strategy = BackPressureStrategy::YIELDING_SPIN,  // Balance latency and CPU usage
    .spin_yield_threshold = 1000,                     // Yield after 1000 spin cycles
    .timeout = std::chrono::milliseconds(500)         // Give up after 500ms
};

EventBus event_bus(config, bp_config);

// Key-based partitioning ensures related events go to same partition
event_bus.publish_event(event, "user_123");  // Routes consistently to same partition

// Note: If a consumer group has more consumers than topic partitions,
// excess consumers will remain idle. Plan accordingly:
// - 8 partitions + 12 consumers = 4 idle consumers
// - 8 partitions + 6 consumers = optimal distribution
```

## 📊 Performance Benchmarks

### System Configuration
- **Hardware**: Apple M1, 8C/8T, 16GB RAM
- **OS**: macOS 13.2.1
- **Compiler**: Apple clang 14.0.3 (-O3 Release)

### Latency Performance (Trading Payloads ~40 bytes)

| Scenario | Configuration | P50 | P99 | P99.9 |
|----------|---------------|-----|-----|-------|
| **Burst Load** (15K events) | Single Partition | 3.1ms | 4.1ms | 4.1ms |
| | Multi Partition (4P/4C) | 1.6ms | 2.7ms | 2.7ms |
| **Sustained Load** (50K @ 10K/sec) | Single Partition | 15μs | 69μs | 92μs |
| | Multi Partition (4P/4C) | 22μs | 68μs | 83μs |

### Throughput Scaling (CPU-bound workload)

| Partitions | Consumers | Throughput | Scaling Factor |
|------------|-----------|------------|----------------|
| 1 | 1 | 1,159 msg/sec | 1.0x (baseline) |
| 4 | 4 | 4,211 msg/sec | 3.6x |
| 8 | 8 | 5,230 msg/sec | 4.5x |
| 15 | 15 | 5,269 msg/sec | 4.5x |

**Key Findings:**
- 2x latency improvement for burst workloads through horizontal scaling
- Near-linear throughput scaling up to CPU core count (8 cores)
- Sub-100μs sustained latency
- Zero message loss across all test configurations

## 🎯 Use Cases

### Ideal For:
- **High-frequency event processing** with low-latency requirements
- **Multi-threaded applications** needing efficient event distribution
- **Real-time systems** with burst traffic patterns

## 🔧 Configuration Options

### Topic Configuration
```cpp
TopicConfig {
    .name = "trade_events",
    .partition_count = 8  // Plan based on expected consumer groups
}
```

Understanding partition planning is crucial for optimal performance. The partition count directly affects your ability to scale consumers horizontally. Each consumer group that subscribes to this topic will have its consumers distributed across these 8 partitions. If you later create a consumer group with 12 consumers, only 8 will actively process events while 4 remain idle.

### Consumer Group Configuration
```cpp
ConsumerGroupConfig {
    .group_id = "risk_processors",
    .topic_name = "trade_events",    // Each group subscribes to exactly one topic
    .consumer_count = 4              // Optimal: match or divide evenly into partition count
}
```

The relationship between consumer count and partition count determines your processing efficiency. When you have 8 partitions and 4 consumers, each consumer handles exactly 2 partitions. This creates an optimal load distribution. However, if you configure 6 consumers for 8 partitions, 2 consumers will handle 2 partitions each while 4 consumers handle 1 partition each, creating uneven workload distribution.

### Back-pressure Strategies

Your choice of back-pressure strategy significantly impacts both performance and message delivery guarantees:

**DROP_NEWEST** (Default): When a queue reaches its 16K capacity, new events are discarded. This strategy provides the lowest latency since publishers never block, but you sacrifice delivery guarantees. This approach works well for high-frequency telemetry data where losing some events is acceptable.

**BLOCK**: Publishers wait until queue space becomes available. This guarantees message delivery but can create backpressure that affects overall system throughput. Use this strategy when every message is critical and you can tolerate variable publishing latency.

**SPIN**: Publishers continuously check for available queue space without yielding CPU time. This provides the lowest latency when queues have space but consumes maximum CPU resources. Ideal for latency-critical applications with dedicated CPU cores.

**YIELDING_SPIN**: Publishers spin for a configurable number of cycles before yielding to other threads. This balances latency and CPU efficiency, making it suitable for most production environments where you need good performance without monopolizing system resources.

## 📈 Performance Tuning

### Optimal Partitioning Strategy

The relationship between partitions, consumers, and CPU cores requires careful planning to achieve optimal performance. Your partition count serves as the fundamental scaling unit for your entire system, since it determines the maximum parallelism achievable by any consumer group.

**Partition Planning Guidelines**: Start with a partition count that matches your available CPU cores, typically 1-2 partitions per core. This provides a solid foundation for most workloads. However, consider your future scaling needs as well, since increasing partitions later requires system reconfiguration. If you anticipate multiple consumer groups with different scaling requirements, plan for the highest expected consumer count.

**Consumer Group Sizing**: The optimal consumer count equals the partition count for maximum parallelism. When you have fewer consumers than partitions, each consumer handles multiple partitions, which can work well but may create hotspots if some partitions receive more traffic. When you have more consumers than partitions, the excess consumers remain idle, wasting resources but providing no performance benefit.

**Avoiding Over-partitioning**: While it might seem logical to create many partitions for future flexibility, excessive partitioning introduces coordination overhead without providing benefits. The diminishing returns typically begin when partition counts exceed 2x your CPU core count, as thread contention starts to offset the parallelism gains.

### Queue Capacity Management

Each partition maintains its own 16K event queue, which means your total memory footprint scales with partition count. The default 16K capacity provides excellent burst tolerance for most workloads while maintaining reasonable memory usage.

**Capacity Planning**: Monitor your queue depths during peak loads to determine if the default 16K capacity suits your workload. If you frequently see dropped events with DROP_NEWEST strategy, consider either increasing queue capacity or adding more consumer throughput. Remember that larger queues provide better burst tolerance but increase memory usage and potentially worsen latency during queue draining.

**Memory Footprint Calculation**: Your total queue memory equals (partition count × queue capacity × event size). For a system with 8 partitions and average 40-byte events, you would allocate approximately 5MB for queue storage, plus additional overhead for queue metadata and consumer coordination.

### Consumer Batch Optimization

The batch size you choose for consumer polling significantly impacts both latency and throughput characteristics. Smaller batches provide lower latency since events get processed more quickly after arrival, but they also increase the overhead from frequent polling operations.

**Latency-Optimized Batching**: Use small batch sizes of 1-10 events when your application prioritizes response time over raw throughput. This approach works well for real-time systems where individual event processing time is critical.

**Throughput-Optimized Batching**: Choose larger batch sizes of 50-100 events when you need to maximize overall system throughput. The increased batching amortizes the polling overhead across more events, but individual events may wait longer before processing begins.

**Workload-Adaptive Batching**: Consider implementing dynamic batch sizing based on queue depth. When queues are nearly empty, use small batches for low latency. When queues build up during traffic bursts, automatically increase batch sizes to drain queues more efficiently.

## 🧪 Testing

The repository includes comprehensive benchmarks and examples:

### Benchmark Suite
- **`latency_benchmark_demo`**: End-to-end latency measurement
- **`partition_scaling_demo`**: Horizontal scaling validation
- **`basic_usage_demo`**: Functional correctness verification

### Running Benchmarks
```bash
cd build
./latency_benchmark_demo > ../benchmarks/results/latency_$(date +%Y%m%d).txt
./partition_scaling_demo > ../benchmarks/results/scaling_$(date +%Y%m%d).txt
```

## 📋 TODO & Future Enhancements

### 🧪 Testing & Validation
- [ ] **Unit Test Suite**: Comprehensive unit tests for all core components
    - [ ] MPSC queue operations (enqueue/dequeue correctness)
    - [ ] Event bus configuration validation
    - [ ] Consumer group assignment logic
    - [ ] Partition selection algorithms (round-robin & key-based)
    - [ ] Back-pressure strategy implementations
- [ ] **Integration Tests**: End-to-end system behavior validation
    - [ ] Multi-publisher concurrent publishing scenarios
    - [ ] Consumer group fan-out correctness
    - [ ] Message ordering verification across different configurations
    - [ ] Back-pressure activation and recovery testing
- [ ] **Stress Testing**: System behavior under extreme conditions
    - [ ] High-frequency publishing (1M+ events/sec)
    - [ ] Memory pressure testing with large queue depths
    - [ ] Consumer failure and recovery scenarios
    - [ ] Long-running stability tests (24+ hour runs)
- [ ] **Performance Regression Tests**: Automated performance monitoring
    - [ ] CI/CD integration for benchmark execution
    - [ ] Performance baseline comparison across releases
    - [ ] Memory usage and leak detection

### 🚀 Performance Optimizations
- [ ] **Memory Pool Implementation**: Reduce allocation overhead for Event objects
- [ ] **Lock-free Statistics**: Real-time performance metrics without coordination overhead

### 🔧 Feature Extensions
- [ ] **Event Persistence**: Optional disk-based event storage for replay scenarios
- [ ] **Message Filtering**: Consumer-side event filtering to reduce processing overhead

### 🛡️ Production Readiness
- [ ] **Error Handling**: Comprehensive error recovery and reporting mechanisms
- [ ] **Configuration Validation**: Runtime validation of EventBusConfig parameters
- [ ] **Graceful Shutdown**: Clean consumer group termination and resource cleanup
- [ ] **Health Checks**: System health monitoring and diagnostic endpoints
- [ ] **Documentation**: API documentation generation and usage examples

### 🔍 Observability & Debugging
- [ ] **Event Tracing**: Distributed tracing support for event flow debugging
- [ ] **Queue Depth Monitoring**: Real-time queue utilization metrics
- [ ] **Consumer Lag Tracking**: Monitor processing delays across consumer groups

## 🤝 Contributing

Contributions are welcome! Please ensure:

1. **Code follows C++17 standards**
2. **Include unit tests** for new features
3. **Run benchmark suite** to verify performance
4. **Update documentation** for API changes

### Development Setup
```bash
# Debug build for development
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)

# Run with debugging symbols
gdb ./basic_usage_demo   # or: lldb ./basic_usage_demo
```

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- **LMAX Disruptor**: Inspiration for lock-free ring buffer design
- **Lock-free programming community**: Techniques and best practices

## 📞 Support

- **Issues**: [GitHub Issues](https://github.com/your-username/lock-free-event-bus/issues)
- **Discussions**: [GitHub Discussions](https://github.com/your-username/lock-free-event-bus/discussions)
- **Documentation**: See `examples/` directory for detailed usage patterns

