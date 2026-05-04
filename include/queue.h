#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

#include "request.h"

class RequestQueue
{
public:

    void push(const Request& req)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(req);
        cv_.notify_one();  // 唤醒等待的 Worker
    }

    bool pop(Request& req)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (queue_.empty())
            return false;

        req = queue_.front();
        queue_.pop();

        return true;
    }

    // 阻塞等待直到有请求或超时
    bool wait_for_request(std::chrono::milliseconds timeout = std::chrono::milliseconds(100))
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, timeout, [this] { return !queue_.empty(); });
    }

    bool empty()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t size()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:

    std::queue<Request> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
};