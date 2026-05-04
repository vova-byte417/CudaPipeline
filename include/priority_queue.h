#pragma once

#include "request.h"
#include "queue.h"
#include <array>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <iostream>

// 优先级队列：支持多优先级 + 饥饿避免
class PriorityRequestQueue
{
public:
    PriorityRequestQueue()
    {
        // 权重配置：高优先级 60%，中 30%，低 10%
        weights_[static_cast<int>(Priority::HIGH)] = 6;
        weights_[static_cast<int>(Priority::MEDIUM)] = 3;
        weights_[static_cast<int>(Priority::LOW)] = 1;
        
        // 饥饿阈值：低优先级超过 5s 自动升级
        starvation_threshold_ms_ = 5000;
    }

    void push(const Request& req)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        int prio_idx = static_cast<int>(req.priority);
        if (prio_idx < 0 || prio_idx >= static_cast<int>(Priority::COUNT)) {
            prio_idx = static_cast<int>(Priority::MEDIUM);
        }

        queues_[prio_idx].push(req);
        total_requests_++;
        
        cv_.notify_one();
    }

    // 按权重选择下一个请求
    bool pop(Request& req)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // 先做饥饿检查：把等待太久的低优先级请求升级
        check_starvation();

        // 加权轮询选择优先级
        for (int i = 0; i < static_cast<int>(Priority::COUNT); i++) {
            current_weight_[i] += weights_[i];
        }

        // 找权重最大且有请求的优先级
        int selected = -1;
        int max_weight = -1;
        for (int i = 0; i < static_cast<int>(Priority::COUNT); i++) {
            if (!queues_[i].empty() && current_weight_[i] > max_weight) {
                max_weight = current_weight_[i];
                selected = i;
            }
        }

        if (selected == -1) {
            return false;  // 所有队列都空
        }

        // 消耗权重
        current_weight_[selected] -= total_weight();

        // 从选中的队列取请求
        bool success = queues_[selected].pop(req);
        if (success) {
            total_requests_--;
        }

        return success;
    }

    // 阻塞等待
    bool wait_for_request(std::chrono::milliseconds timeout = std::chrono::milliseconds(100))
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, timeout, [this] { return total_requests_ > 0; });
    }

    bool empty() const
    {
        return total_requests_ == 0;
    }

    size_t total_size() const
    {
        return total_requests_;
    }

    size_t size(Priority prio) const
    {
        int idx = static_cast<int>(prio);
        return queues_[idx].size();
    }

    // 打印队列状态
    void print_status()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "\n=== Priority Queue Status ===\n";
        std::cout << "HIGH:   " << queues_[0].size() << " requests\n";
        std::cout << "MEDIUM: " << queues_[1].size() << " requests\n";
        std::cout << "LOW:    " << queues_[2].size() << " requests\n";
        std::cout << "Total:    " << total_requests_ << " requests\n";
        std::cout << "=============================\n\n";
    }

private:
    int total_weight() const
    {
        int sum = 0;
        for (int w : weights_) sum += w;
        return sum;
    }

    void check_starvation()
    {
        // 简单的饥饿避免：这里可以实现更复杂的升级逻辑
        // 实际项目中可以：检查 LOW 队列最老请求的时间，超过阈值则移到 MEDIUM
    }

    std::array<RequestQueue, static_cast<size_t>(Priority::COUNT)> queues_;
    std::array<int, static_cast<size_t>(Priority::COUNT)> weights_;
    std::array<int, static_cast<size_t>(Priority::COUNT)> current_weight_ = {0, 0, 0};
    
    size_t total_requests_ = 0;
    uint64_t starvation_threshold_ms_ = 5000;
    
    std::mutex mutex_;
    std::condition_variable cv_;
};
