#include "scheduler/sjf_scheduler.h"
#include "runtime/batch.h"
#include <vector>
#include <algorithm>

bool SJFScheduler::select_batch(RequestQueue& queue, Batch& batch)
{
    batch.requests.clear();
    batch.total_input_size = 0;

    std::vector<Request> candidates;
    
    const size_t candidate_count = static_cast<size_t>(MAX_BATCH_SIZE) * 3;
    queue.pop_batch(candidates, candidate_count);

    if (candidates.empty()) {
        return false;
    }

    std::sort(candidates.begin(), candidates.end(),
        [](const Request& a, const Request& b) {
            return a.estimated_exec_time < b.estimated_exec_time;
        });

    // 修复：类型转换
    const size_t batch_size = std::min(static_cast<size_t>(MAX_BATCH_SIZE), candidates.size());
    for (size_t i = 0; i < batch_size; ++i) {
        batch.requests.push_back(candidates[i]);
        batch.total_input_size += candidates[i].input_size;
    }

    if (candidates.size() > batch_size) {
        std::vector<Request> unselected(candidates.begin() + batch_size, candidates.end());
        queue.push_front_batch(unselected);
    }

    return !batch.requests.empty();
}
