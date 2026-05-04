#include "scheduler/fcfs_scheduler.h"
#include "runtime/batch.h"

bool FCFS_Scheduler::select_batch(
    RequestQueue& queue,
    Batch& batch
)
{
    constexpr int MAX_BATCH_SIZE = 4;

    batch.requests.clear();
    batch.total_input_size = 0;

    Request req;

    while (
        batch.requests.size() < MAX_BATCH_SIZE
    )
    {
        if (!queue.pop(req))
        {
            break;
        }

        batch.requests.push_back(req);

        batch.total_input_size +=
            req.input_size;
    }

    return !batch.requests.empty();
}
