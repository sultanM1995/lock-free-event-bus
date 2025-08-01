#pragma once
#include <atomic>
#include <memory>

using std::atomic;

namespace eventbus {
    template<typename T>
    class LockFreeMpscQueue {

    public:
        explicit LockFreeMpscQueue(const size_t capacity)
               : capacity_(capacity + 1),  // +1 to distinguish full from empty
                 buffer_(std::make_unique<T[]>(capacity_)),
                 head_(0),
                 tail_(0)
        {}

        bool enqueue(const T& item) {
            const size_t current_tail = tail_.fetch_add(1, std::memory_order_acq_rel);
            const size_t idx = current_tail % capacity_;
            const size_t next_idx = (current_tail + 1) % capacity_;
            const size_t current_head = head_.load(std::memory_order_acquire);
            if (next_idx == current_head) {
                tail_.fetch_sub(1, std::memory_order_acq_rel);
                return false;
            }
            buffer_[idx] = item;
            return true;
        }

        bool dequeue(T& item) {
            const size_t current_head = head_.load(std::memory_order_relaxed);
            const size_t current_tail = tail_.load(std::memory_order_acquire);
            if (current_tail % capacity_ == current_head) {
                return false;
            }
            item = buffer_[current_head];
            head_.store((current_head + 1) % capacity_, std::memory_order_release);
            return true;
        }


    private:
        size_t capacity_;
        std::unique_ptr<T[]> buffer_;
        alignas(64) atomic<size_t> head_;
        alignas(64) atomic<size_t> tail_;
    };
}
