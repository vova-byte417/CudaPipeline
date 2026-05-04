#pragma once

#include "request.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <iostream>

// 截止时间比较器：deadline 小的排在前面（最早截止时间优先
struct DeadlineCompare {
    bool operator()(const Request& a, const Request& b) const {
        return a.deadline > b.deadline;  // 注意：priority_queue 是大顶堆，所以这里反过来
    }
};

// 截止时间队列：内部用 priority_queue，按 deadline 排序
class DeadlineQueue
{
public:
    DeadlineQueue() = default;

    void push(const Request& req)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(req);
        total_requests_++;
        cv_.notify_one();
    }

    bool pop(Request& req)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (queue_.empty()) {
            return false;
        }

        req = queue_.top();
        queue_.pop();
        total_requests_--;

        return true;
    }

    // 阻塞等待
    bool wait_for_request(std::chrono::milliseconds timeout = std::chrono::milliseconds(100))
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, timeout, [this] { return !queue_.empty(); });
    }

    bool empty() const
    {
        return total_requests_ == 0;
    }

    size_t size() const
    {
        return total_requests_;
    }

    // 辅助函数：获取当前最早的截止时间
    uint64_t earliest_deadline()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return 0;
        }
        return queue_.top().deadline;
    }

    void print_status()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!queue_.empty()) {
            std::cout << "[DeadlineQueue] " << total_requests_ 
                      << " requests, earliest deadline: " 
                      << queue_.top().deadline << "\n";
        } else {
            std::cout << "[DeadlineQueue] Empty\n";
        }
    }

private:
    std::priority_queue<Request, std::vector<Request>, DeadlineCompare> queue_;
    size_t total_requests_ = 0;

    std::mutex mutex_;
    std::condition_variable cv_;
};
