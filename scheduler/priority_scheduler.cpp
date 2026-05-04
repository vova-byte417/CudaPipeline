#include "scheduler/priority_scheduler.h"
#include <algorithm>  // for sorting

bool PriorityScheduler::select_batch(RequestQueue& queue, Batch& batch) {
    constexpr int MAX_BATCH_SIZE = 8;  // 可调参数
    batch.requests.clear();
    batch.total_input_size = 0;

    // 临时收集候选任务（因为队列是 std::queue，无法直接按 priority 排序）
    std::vector<Request> candidates;
    Request req;

    // 先拉一批候选（可优化为优先级队列）
    while (candidates.size() < MAX_BATCH_SIZE * 2 && queue.pop(req)) {  // 多拉一些
        candidates.push_back(req);
    }

    // 按 priority 排序（priority 越小优先级越高）
    std::sort(candidates.begin(), candidates.end(), 
              [](const Request& a, const Request& b) {
                  return a.priority < b.priority;  // 或结合 input_size
              });

    // 选前 MAX_BATCH_SIZE 个
    for (size_t i = 0; i < candidates.size() && batch.requests.size() < MAX_BATCH_SIZE; ++i) {
        batch.requests.push_back(candidates[i]);
        batch.total_input_size += candidates[i].input_size;
    }

    // 把没选上的放回队列（简化实现，实际可优化）
    for (size_t i = batch.requests.size(); i < candidates.size(); ++i) {
        queue.push(candidates[i]);  // 注意：push 可能需要调整顺序
    }

    return !batch.requests.empty();
}