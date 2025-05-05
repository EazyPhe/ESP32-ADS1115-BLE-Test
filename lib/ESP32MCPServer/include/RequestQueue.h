#pragma once

#include <queue>
#include <mutex>

template<typename T>
class RequestQueue {
public:
    RequestQueue(size_t maxSize = 32) : maxQueueSize(maxSize) {}
    
    bool push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex);
        if (queue.size() >= maxQueueSize) {
            return false;
        }
        queue.push(item);
        return true;
    }
    
    bool pop(T& item) {
        std::lock_guard<std::mutex> lock(mutex);
        if (queue.empty()) {
            return false;
        }
        item = queue.front();
        queue.pop();
        return true;
    }
    
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex);
        return queue.empty();
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex);
        return queue.size();
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex);
        std::queue<T> empty;
        std::swap(queue, empty);
    }

private:
    std::queue<T> queue;
    mutable std::mutex mutex;
    const size_t maxQueueSize;
};