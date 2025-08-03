#pragma once
#include <atomic>
#include <iostream>
#include <memory>

using std::atomic;

namespace eventbus {
    template<typename T>
    class LockFreeMpscQueue {

    public:
        explicit LockFreeMpscQueue(const size_t capacity)
               : capacity_(capacity),
                 buffer_(std::make_unique<node_[]>(capacity_)),
                 head_(0),
                 tail_(0) {
            for (size_t i = 0; i < capacity_; ++i) {
                buffer_[i].seq_.store(i, std::memory_order_relaxed);
            }
        }

        bool enqueue(const T& item) {
            size_t pos = tail_.load(std::memory_order_relaxed);
            while (true) {
                size_t slot_index = pos & (capacity_ - 1);
                node_& node = buffer_[slot_index];

                // Check if this slot is ready for our position
                const size_t seq = node.seq_.load(std::memory_order_acquire);
                intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

                if (diff == 0) {
                    // Slot is ready for our position - try to claim it
                    // Here I could use cas strong as well but it's okay because if our cas fails here my pos will reset and I will retry through while loop
                    if (tail_.compare_exchange_weak(pos, pos + 1,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_relaxed)) {
                        // We successfully claimed the slot - write our data
                        node.item_ = item;

                        // Mark the slot as ready for consumer
                        node.seq_.store(pos + 1, std::memory_order_release);
                        return true;
                    }
                    // CAS failed, pos was updated to current tail value, retry
                } else if (diff < 0) {
                    // Slot not ready - queue is full
                    return false;
                } else {
                    // Another producer got ahead - refresh pos and retry
                    pos = tail_.load(std::memory_order_relaxed);
                }
            }
        }

        bool dequeue(T& item) {
            const size_t pos = head_.load(std::memory_order_relaxed);
            size_t slot_index = pos & (capacity_ - 1);

            node_& node = buffer_[slot_index];

            size_t node_seq = node.seq_.load(std::memory_order_acquire);

            if (node_seq != pos + 1) {
                return false;  // No data ready for this position
            }

            item = node.item_;
            node.seq_.store(pos + capacity_, std::memory_order_release);

            head_.store(pos + 1, std::memory_order_relaxed);
            return true;
        }

        void debug_print() {
            std::cout << "head: " << head_.load() << ", tail: " << tail_.load() << std::endl;
            for (size_t i = 0; i < capacity_; ++i) {
                std::cout << "slot[" << i << "].seq_: " << buffer_[i].seq_.load() << std::endl;
            }
        }

    private:
        struct node_ {
            T item_;
            std::atomic<size_t> seq_;
        };
        size_t capacity_;
        std::unique_ptr<node_[]> buffer_;
        alignas(64) atomic<size_t> head_;
        alignas(64) atomic<size_t> tail_;
    };
}
