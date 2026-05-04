#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <cstdlib>
#include <iomanip>

#include "cuda_backend.h"
#include "queue.h"
#include "runtime/worker.h"
#include "scheduler/sjf_scheduler.h"
#include "scheduler/fcfs_scheduler.h"
#include "scheduler/priority_scheduler.h"
#include "metrics/metrics.h"
#include "request.h"
#include "estimator.h"
#include "trace_loader.h"
#include "util.h"

// 快速配置
struct BenchConfig {
    std::string scheduler_type = "SJF";  // FCFS / SJF / Priority
    int num_requests = 100;
    int batch_size = 8;
    int warmup_requests = 10;
    bool verbose = false;
};

void print_header(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

void print_config(const BenchConfig& cfg) {
    std::cout << "  Scheduler    : " << cfg.scheduler_type << "\n";
    std::cout << "  Requests     : " << cfg.num_requests << "\n";
    std::cout << "  Batch Size   : " << cfg.batch_size << "\n";
    std::cout << std::string(60, '-') << "\n\n";
}

Request create_dummy_request(int id, int size, const std::string& op_name) {
    Request req;
    req.request_id = id;
    req.input_size = size;
    req.output_size = size;
    req.operator_name = op_name;
    req.priority = rand() % 4;
    
    // 分配主机内存
    req.h_a = new float[size];
    req.h_b = new float[size];
    req.h_c = new float[size];
    
    // 初始化数据
    for (int i = 0; i < size; ++i) {
        req.h_a[i] = static_cast<float>(i);
        req.h_b[i] = static_cast<float>(i * 2);
    }
    
    Estimator::estimate(req);
    return req;
}

void free_request(Request& req) {
    delete[] req.h_a;
    delete[] req.h_b;
    delete[] req.h_c;
}

std::unique_ptr<Scheduler> create_scheduler(const std::string& type) {
    if (type == "FCFS") return std::make_unique<FCFS_Scheduler>();
    if (type == "SJF") return std::make_unique<SJFScheduler>();
    if (type == "Priority") return std::make_unique<PriorityScheduler>();
    LOG_WARN("Unknown scheduler: " << type << ", using SJF");
    return std::make_unique<SJFScheduler>();
}

void run_benchmark(const BenchConfig& cfg) {
    print_header("CUDA Pipeline Benchmark");
    print_config(cfg);

    // ========== 初始化 ==========
    CUDABackend backend(0, 4);  // device 0, 4 streams
    RequestQueue queue;
    Metrics metrics;
    auto scheduler = create_scheduler(cfg.scheduler_type);
    
    Worker worker(&backend, &queue, scheduler.get(), &metrics);

    if (!backend.initialize()) {
        LOG_ERROR("Failed to initialize CUDA backend");
        return;
    }

    // ========== Warmup ==========
    std::cout << "[1/4] Warmup... ";
    for (int i = 0; i < cfg.warmup_requests; ++i) {
        Request req = create_dummy_request(-i, 1024, "vector_add");
        backend.submit(req);
        free_request(req);
    }
    std::cout << "OK\n";

    // ========== 生成负载 ==========
    std::cout << "[2/4] Generating workload... ";
    std::vector<Request> requests;
    const int sizes[] = {512, 1024, 2048, 4096, 8192, 16384};
    
    for (int i = 0; i < cfg.num_requests; ++i) {
        int size = sizes[rand() % 6];
        requests.push_back(create_dummy_request(i, size, "vector_add"));
        queue.push(requests.back());
    }
    std::cout << "OK (" << cfg.num_requests << " requests)\n";

    // ========== 执行 ==========
    std::cout << "[3/4] Running benchmark... ";
    auto start = std::chrono::high_resolution_clock::now();
    
    worker.start();
    
    // 等待队列空
    while (!queue.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // 额外等待执行完成
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    worker.stop();
    
    auto end = std::chrono::high_resolution_clock::now();
    double total_sec = std::chrono::duration<double>(end - start).count();
    std::cout << "OK\n\n";

    // ========== 结果输出 ==========
    print_header("Benchmark Results");
    
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Total Time     : " << total_sec << " s\n";
    std::cout << "  Throughput     : " << cfg.num_requests / total_sec << " req/s\n";
    std::cout << "  Avg Batch Size : " << static_cast<double>(cfg.num_requests) / 
                             (total_sec * 1000 / 10) << " req/batch\n\n";
    
    metrics.print();

    // ========== 清理 ==========
    std::cout << "\n[4/4] Cleaning up... ";
    for (auto& req : requests) {
        free_request(req);
    }
    backend.shutdown();
    std::cout << "OK\n";
}

void run_comparison() {
    print_header("Scheduler Comparison");
    
    std::vector<std::string> schedulers = {"FCFS", "SJF", "Priority"};
    
    for (const auto& sched : schedulers) {
        BenchConfig cfg;
        cfg.scheduler_type = sched;
        cfg.num_requests = 200;
        cfg.verbose = false;
        run_benchmark(cfg);
    }
}

int main(int argc, char* argv[]) {
    // 解析命令行参数
    BenchConfig cfg;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--scheduler" && i + 1 < argc) {
            cfg.scheduler_type = argv[++i];
        } else if (arg == "--requests" && i + 1 < argc) {
            cfg.num_requests = std::stoi(argv[++i]);
        } else if (arg == "--batch" && i + 1 < argc) {
            cfg.batch_size = std::stoi(argv[++i]);
        } else if (arg == "--compare") {
            run_comparison();
            return 0;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --scheduler TYPE   FCFS / SJF / Priority (default: SJF)\n"
                      << "  --requests N       Number of requests (default: 100)\n"
                      << "  --batch N          Max batch size (default: 8)\n"
                      << "  --compare          Run all schedulers comparison\n"
                      << "  -h, --help         Show this help\n";
            return 0;
        }
    }

    run_benchmark(cfg);
    return 0;
}
