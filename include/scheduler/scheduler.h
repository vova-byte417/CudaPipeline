#pragma once

#include "request.h"
#include "queue.h"
#include "runtime/batch.h"

class Scheduler
{
public:

    virtual ~Scheduler() {}

    virtual bool select_batch(
        RequestQueue& queue,
        Batch& batch
    ) = 0;
};