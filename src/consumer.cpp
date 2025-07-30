#include "consumer.hpp"

#include <iostream>

namespace eventbus {
     Consumer::Consumer(ConsumerGroup& consumer_group) {
        consumer_id = consumer_group.register_consumer(this);
     }

     void Consumer::receive_queues(const std::vector<std::shared_ptr<LockFreeSpscQueue<Event>>>& queues) {
         queues_ = queues;
     }

    // implemented batching by  division approach. Dividing max_events by the queue size. If any remainder, add
    // one to each of the queue until remainder is exhausted
    [[nodiscard]] std::vector<Event> Consumer::poll_batch(const size_t max_events) const {
         if (queues_.empty() || max_events == 0) {
             return {};
         }

         std::vector<Event> events;
         events.reserve(max_events);

         const size_t num_queues = queues_.size();
         const size_t events_per_queue = max_events / num_queues;
         size_t remainder = max_events % num_queues;

         for (size_t q_idx = 0; q_idx < num_queues; ++q_idx) {
             // Calculate how many events to take from this queue
             size_t events_to_take = events_per_queue;
             if (remainder > 0) {
                 events_to_take += 1;
                 --remainder;
             }

             // Take events from this queue
             size_t taken = 0;
             while (taken < events_to_take) {
                 if (Event event; queues_[q_idx]->dequeue(event)) {
                     events.push_back(std::move(event));
                     taken++;
                 } else {
                     break;  // No more events in this queue
                 }
             }
         }
         return events;
     }
}
