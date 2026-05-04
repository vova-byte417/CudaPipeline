#pragma once

#include "scheduler/scheduler.h"
#include "runtime/batch.h"

class FCFS_Scheduler : public Scheduler
{
public:
    bool select_batch(
        RequestQueue& queue,
        Batch& batch
    ) override;
    virtual ~FCFS_Scheduler() = default;
   
};