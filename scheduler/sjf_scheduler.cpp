#include "scheduler/sjf_scheduler.h"
#include <vector>
#include <algorithm>

bool SJFScheduler::select_batch(RequestQueue& queue, Batch& batch)
{
    batch.requests.clear();
    batch.total_input_size = 0;

    std::vector<Request> candidates;
    Request req;

    // 拉取足够候选任务
    while (candidates.size() < MAX_BATCH_SIZE * 3 && queue.pop(req)) {
        candidates.push_back(req);
    }

    if (candidates.empty()) {
        return false;
    }

    // Shortest Job First: 按 estimated_exec_time 排序
    std::sort(candidates.begin(), candidates.end(),
        [](const Request& a, const Request& b) {
            return a.estimated_exec_time < b.estimated_exec_time;
        });

    // 组成 Batch（可加入 total_input_size 限制）
    for (size_t i = 0; i < candidates.size() && batch.requests.size() < MAX_BATCH_SIZE; ++i) {
        batch.requests.push_back(candidates[i]);
        batch.total_input_size += candidates[i].input_size;
    }

    // 未选中的放回队列
    for (size_t i = batch.requests.size(); i < candidates.size(); ++i) {
        queue.push(candidates[i]);
    }

    return !batch.requests.empty();
}