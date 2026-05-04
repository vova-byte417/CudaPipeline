#pragma once
#include "scheduler/scheduler.h"
#include "runtime/batch.h"

class PriorityScheduler : public Scheduler {
public:
    bool select_batch(RequestQueue& queue, Batch& batch) override;
    virtual ~PriorityScheduler() = default;
};