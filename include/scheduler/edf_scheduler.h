#pragma once

#include "scheduler/scheduler.h"
#include "request.h"
#include "queue.h"
#include "deadline_queue.h"
#include "runtime/batch.h"
#include <vector>
#include <algorithm>
#include <iostream>
#include <chrono>

// EDF (Earliest Deadline First) 调度器
// 适用于有实时要求的任务调度
class EDF_Scheduler : public Scheduler
{
public:
    EDF_Scheduler(int max_batch_size = 4)
        : max_batch_size_(max_batch_size)
    {}

    // 从普通队列选择（兼容接口）
    bool select_batch(
        RequestQueue& queue,
        Batch& batch
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

    // 真正的 EDF 调度：从截止时间队列选择（队列本身就是按 deadline 排序的）
    bool select_batch(
        DeadlineQueue& deadline_queue,
        Batch& batch
    )
    {
        batch.requests.clear();
        batch.total_input_size = 0;

        Request req;
        while (batch.requests.size() < max_batch_size_) {
            if (!deadline_queue.pop(req)) {
                break;
            }
            batch.requests.push_back(req);
            batch.total_input_size += req.input_size;
        }

        if (!batch.requests.empty()) {
            std::cout << "[EDF_Scheduler] Selected batch of " 
                      << batch.requests.size() << " requests (earliest deadline first)\n";
        }

        return !batch.requests.empty();
    }

    virtual ~EDF_Scheduler() = default;

private:
    int max_batch_size_ = 4;
};
