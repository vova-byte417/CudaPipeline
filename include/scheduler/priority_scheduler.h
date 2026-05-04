#pragma once

#include "scheduler/scheduler.h"
#include "priority_queue.h"
#include "runtime/batch.h"
#include <iostream>

// 优先级调度器：支持按权重分配算力
class PriorityScheduler : public Scheduler
{
public:
    PriorityScheduler(int max_batch_size = 4)
        : max_batch_size_(max_batch_size)
    {}

    bool select_batch(
        RequestQueue& queue,  // Note: 这里实际上是普通队列，
        Batch& batch          // 如果要用优先级队列，需要另外的接口
    ) override
    {
        batch.requests.clear();
        batch.total_input_size = 0;

        Request req;
        while (batch.requests.size() < max_batch_size_) {
            if (!queue.pop(req)) {
                break;
            }
            batch.requests.push_back(req);
            batch.total_input_size += req.input_size;
        }

        return !batch.requests.empty();
    }

    // 专门为 PriorityRequestQueue 设计的接口
    bool select_batch(
        PriorityRequestQueue& prio_queue,
        Batch& batch
    )
    {
        batch.requests.clear();
        batch.total_input_size = 0;

        Request req;
        while (batch.requests.size() < max_batch_size_) {
            if (!prio_queue.pop(req)) {
                break;
            }
            batch.requests.push_back(req);
            batch.total_input_size += req.input_size;
        }

        if (!batch.requests.empty()) {
            std::cout << "[PriorityScheduler] Selected batch of " 
                      << batch.requests.size() << " requests\n";
        }

        return !batch.requests.empty();
    }

    virtual ~PriorityScheduler() = default;

private:
    int max_batch_size_ = 4;
};
