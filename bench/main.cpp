#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <cstdlib>   // for rand()

#include "cuda_backend.h"
#include "queue.h"
#include "runtime/worker.h"
#include "scheduler/sjf_scheduler.h"     // 可换成 FCFS / Priority / RL 等
#include "metrics/metrics.h"
#include "request.h"
#include "estimator.h"
#include "trace_loader.h"


int main() {
    std::cout << "=== CUDA Pipeline Scheduler Benchmark ===\n";

    CUDABackend backend;
    RequestQueue queue;
    SJFScheduler scheduler;          // ← 这里切换不同调度器测试
    Metrics metrics;

    Worker worker(&backend, &queue, &scheduler, &metrics);

    // 初始化 CUDA
    if (!backend.initialize()) {
        std::cerr << "Failed to initialize CUDA backend!\n";
        return 1;
    }

    // ====================== Trace Replay 加载 workload ======================
    std::cout << "Loading workload trace...\n";
    
    auto trace = TraceLoader::load_from_json("traces/workload.json");
    std::cout << "Loaded " << trace.size() << " requests from trace.\n";

    // Producer：按 trace 顺序推送请求
    for (auto& req : trace) {
        int n = req.input_size;

        // 分配 host 内存并初始化数据（项目原有逻辑）
        req.h_a = new float[n];
        req.h_b = new float[n];
        req.h_c = new float[n];

        for (int i = 0; i < n; ++i) {
            req.h_a[i] = static_cast<float>(i);
            req.h_b[i] = static_cast<float>(i * 2);
        }

        // 动态估算所有调度所需字段
        Estimator::estimate(req);

        queue.push(req);

        // 模拟真实请求到达间隔（可调整负载强度）
        std::this_thread::sleep_for(std::chrono::microseconds(100 + (rand() % 400)));
    }
    // =========================================================================

    std::cout << "All requests enqueued. Starting worker thread...\n";
    worker.start();

    // 等待任务全部完成（根据 workload 大小调整时间）
    std::this_thread::sleep_for(std::chrono::seconds(10));

    worker.stop();
    backend.shutdown();

    std::cout << "\n=== Benchmark Completed ===\n";
    metrics.print();

    // TODO: 后续可添加 metrics.save_to_csv("results/sjf_trace.csv");

    return 0;
}