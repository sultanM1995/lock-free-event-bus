#pragma once
#include <atomic>
#include <memory>

using std::atomic;

namespace eventbus {
    template<typename T>
    class LockFreeSpscQueue {

    public:
        explicit LockFreeSpscQueue(const size_t capacity)
               : capacity_(capacity + 1),  // +1 to distinguish full from empty
                 buffer_(std::make_unique<T[]>(capacity_)),
                 head_(0),
                 tail_(0)
        {}

        bool enqueue(const T& item) {
            const size_t current_tail = tail_.load(std::memory_order_relaxed);
            const size_t next_tail = (current_tail + 1) % capacity_;
            if (next_tail == head_.load(std::memory_order_acquire)) {
                return false;
            }
            buffer_[current_tail] = item;
            tail_.store(next_tail, std::memory_order_release);
            return true;
        }

        bool dequeue(T& item) {
            const size_t current_head = head_.load(std::memory_order_relaxed);
            if (current_head == tail_.load(std::memory_order_acquire)) {
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
