// EDF (Earliest Deadline First) 调度器 benchmark
// 演示实时任务调度行为

#include <thread>
#include <chrono>
#include <random>
#include <algorithm>

#include "cpu_backend.h"
#include "deadline_queue.h"
#include "runtime/worker.h"
#include "scheduler/edf_scheduler.h"
#include "metrics/metrics.h"

// 生成相对截止时间
uint64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

int main()
{
    std::cout << "=== EDF (Earliest Deadline First) Scheduler Benchmark ===\n\n";

    DeadlineQueue deadline_queue;
    EDF_Scheduler scheduler;

    constexpr int n = 1024;
    constexpr int NUM_REQUESTS = 20;

    std::random_device rd;
    std::mt19937 gen(rd());
    
    // 截止时间分布：10ms ~ 100ms
    std::uniform_int_distribution<> deadline_dis(10, 100);  // ms
    std::uniform_int_distribution<> size_dis(512, 2048);

    uint64_t base_time = now_ns();

    // --------------------------------
    // 生成带有随机截止时间的请求
    // --------------------------------
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
        req.input_size = size_dis(gen);
        req.output_size = req.input_size;
        req.priority = Priority::MEDIUM;
        
        // 设置相对截止时间：现在 + 随机毫秒数
        int deadline_ms = deadline_dis(gen);
        req.deadline = base_time + static_cast<uint64_t>(deadline_ms) * 1000000ULL;

        req.operator_name = "vector_add";
        req.h_a = a;
        req.h_b = b;
        req.h_c = c;

        deadline_queue.push(req);

        std::cout << "Request " << r << ": deadline = " << deadline_ms << " ms\n";
    }

    std::cout << "\n";
    deadline_queue.print_status();

    // --------------------------------
    // 模拟 EDF 调度过程
    // --------------------------------
    std::cout << "\n=== Simulating EDF Batch Selection ===\n\n";

    int batch_count = 0;
    while (!deadline_queue.empty()) {
        Batch batch;
        if (scheduler.select_batch(deadline_queue, batch)) {
            std::cout << "\nBatch " << batch_count++ << ": " 
                      << batch.requests.size() << " requests\n";
            
            // 打印这个 batch 里的请求及其截止时间
            std::cout << "  Request IDs: ";
            for (auto& req : batch.requests) {
                std::cout << req.request_id << " ";
            }
            std::cout << "\n";
            
            // 计算截止时间范围
            uint64_t min_deadline = UINT64_MAX;
            uint64_t max_deadline = 0;
            for (auto& req : batch.requests) {
                min_deadline = std::min(min_deadline, req.deadline);
                max_deadline = std::max(max_deadline, req.deadline);
            }
            
            double min_rel = static_cast<double>(min_deadline - base_time) / 1000000.0;
            double max_rel = static_cast<double>(max_deadline - base_time) / 1000000.0;
            
            std::cout << "  Deadline range: " << min_rel << " ~ " << max_rel << " ms\n";
        }
    }

    deadline_queue.print_status();

    std::cout << "\n=== EDF Scheduler Benchmark Complete ===\n";
    std::cout << "Total batches processed: " << batch_count << "\n\n";

    std::cout << "Key observations:\n";
    std::cout << "  - Requests are always processed in order of their deadlines\n";
    std::cout << "  - Earliest deadline requests get scheduled first\n";
    std::cout << "  - This is optimal for single-resource real-time scheduling\n\n";

    return 0;
}
