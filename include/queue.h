#pragma once

#include <queue>
#include <mutex>
#include <vector>
#include <functional>

#include "request.h"

class RequestQueue
{
public:
    void push(const Request& req)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(req);
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

    // 新增：原子批量弹出（解决SJF竞态问题）
    size_t pop_batch(std::vector<Request>& out, size_t max_count)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t count = 0;
        while (count < max_count && !queue_.empty()) {
            out.push_back(queue_.front());
            queue_.pop();
            count++;
        }
        return count;
    }

    // 新增：原子批量推送（保持顺序）
    void push_batch(const std::vector<Request>& reqs)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& req : reqs) {
            queue_.push(req);
        }
    }

    // 新增：原子批量插入到队首（高优先级）
    void push_front_batch(const std::vector<Request>& reqs)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::queue<Request> new_queue;
        for (const auto& req : reqs) {
            new_queue.push(req);
        }
        while (!queue_.empty()) {
            new_queue.push(queue_.front());
            queue_.pop();
        }
        queue_ = std::move(new_queue);
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    // 新增：带锁的遍历（用于调度器决策）
    void peek_all(std::function<void(const Request&)> visitor) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::queue<Request> temp = queue_;
        while (!temp.empty()) {
            visitor(temp.front());
            temp.pop();
        }
    }

private:
    mutable std::mutex mutex_;
    std::queue<Request> queue_;
};
