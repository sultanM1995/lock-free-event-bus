#pragma once
#include <atomic>
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
            const size_t pos = tail_.fetch_add(1, std::memory_order_acq_rel); // take a ticket
            size_t slot_index = pos & (capacity_ - 1); // find the physical slot index
            node_& node = buffer_[slot_index];

            size_t node_seq = node.seq_.load(std::memory_order_acquire); // take the slot sequence

            if (node_seq != pos) {
                return false;
            }

            node.item_ = item;
            node.seq_.store(pos+1, std::memory_order_release);
            return true;
        }

        bool dequeue(T& item) {
            const size_t pos = head_.load(std::memory_order_relaxed);
            size_t slot_index = pos & (capacity_ - 1);

            node_& node = buffer_[slot_index];

            size_t node_seq = node.seq_.load(std::memory_order_acquire);

            if (node_seq != pos + 1) {
                return false;
            }

            item = node.item_;
            node.seq_.store(pos + capacity_, std::memory_order_release);

            head_.store(pos + 1, std::memory_order_relaxed);
            return true;
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
