#pragma once
#include "scheduler/scheduler.h"
#include "runtime/batch.h"
#include <vector>
#include <algorithm>

class SJFScheduler : public Scheduler {
public:
    bool select_batch(RequestQueue& queue, Batch& batch) override;
    virtual ~SJFScheduler() = default;

private:
    static constexpr int MAX_BATCH_SIZE = 6;   // 可调
};