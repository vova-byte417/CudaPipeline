// 优先级调度器 benchmark
// 演示不同优先级任务的调度行为

#include <thread>
#include <chrono>
#include <random>

#include "cuda_backend.h"
#include "priority_queue.h"
#include "runtime/worker.h"
#include "scheduler/priority_scheduler.h"
#include "metrics/metrics.h"

int main()
{
    std::cout << "=== Priority Scheduler Benchmark ===\n\n";

    CUDABackend backend;
    PriorityRequestQueue prio_queue;
    PriorityScheduler scheduler;
    Metrics metrics;

    // 注意：Worker 现在需要支持 PriorityRequestQueue
    // 这里我们用普通 Worker + PriorityScheduler 演示概念

    // --------------------------------
    // 生成不同优先级的请求
    // --------------------------------
    constexpr int n = 1024;
    constexpr int NUM_REQUESTS = 30;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 10);

    for (int r = 0; r < NUM_REQUESTS; r++) {
        float* a = new float[n];
        float* b = new float[n];
        float* c = new float[n];

        for (int i = 0; i < n; i++) {
            a[i] = i + r * 1000;
            b[i] = i + r * 1000;
        }

        Request req;
        req.request_id = r;
        req.input_size = n;
        req.output_size = n;

        // 60% 高优先级，30% 中，10% 低
        int rand_val = dis(gen);
        if (rand_val < 6) {
            req.priority = Priority::HIGH;
        } else if (rand_val < 9) {
            req.priority = Priority::MEDIUM;
        } else {
            req.priority = Priority::LOW;
        }

        req.operator_name = "vector_add";
        req.h_a = a;
        req.h_b = b;
        req.h_c = c;

        prio_queue.push(req);
    }

    prio_queue.print_status();

    // --------------------------------
    // 模拟调度过程（展示优先级调度逻辑）
    // --------------------------------
    std::cout << "\n=== Simulating Batch Selection ===\n\n";

    int batch_count = 0;
    while (!prio_queue.empty()) {
        Batch batch;
        if (scheduler.select_batch(prio_queue, batch)) {
            std::cout << "Batch " << batch_count++ << ": " 
                      << batch.requests.size() << " requests\n";
            
            // 打印这个 batch 里各优先级的分布
            int high = 0, medium = 0, low = 0;
            for (auto& req : batch.requests) {
                switch (req.priority) {
                    case Priority::HIGH: high++; break;
                    case Priority::MEDIUM: medium++; break;
                    case Priority::LOW: low++; break;
                }
            }
            std::cout << "  Priority distribution: HIGH=" << high 
                      << ", MEDIUM=" << medium << ", LOW=" << low << "\n\n";
        }
    }

    prio_queue.print_status();

    std::cout << "\n=== Priority Scheduler Benchmark Complete ===\n";
    std::cout << "Total batches processed: " << batch_count << "\n\n";

    // 注意：完整的 GPU 执行需要修改 Worker 接口支持 PriorityRequestQueue
    // 架构设计上：Scheduler 应该接受抽象的 QueueInterface，而不是具体的 RequestQueue

    return 0;
}
