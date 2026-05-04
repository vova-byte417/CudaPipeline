#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <cstdlib>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <string>
#include <functional>
#include <random>

#include <cuda_runtime.h>

#include "cuda_backend.h"
#include "queue.h"
#include "runtime/worker.h"
#include "scheduler/sjf_scheduler.h"
#include "scheduler/fcfs_scheduler.h"
#include "scheduler/priority_scheduler.h"
#include "scheduler/rl_scheduler.h"
#include "metrics/metrics.h"
#include "request.h"
#include "estimator.h"
#include "trace_loader.h"
#include "util.h"

// ============================================
// 配置结构体
// ============================================
struct BenchConfig {
    std::string scheduler_type = "SJF";
    int num_requests = 100;
    int batch_size = 8;
    int warmup_requests = 10;
    bool verbose = false;
};

// 基准测试结果
struct BenchmarkResult {
    std::string scheduler_name;
    double throughput;         // req/s
    double p50_ms;
    double p95_ms;
    double p99_ms;
    double avg_latency_ms;
    size_t total_requests;
    double total_time_sec;
};

// ============================================
// 全局变量（简化RL模型加载实现）
// ============================================
std::string g_rl_model_path = "";
bool g_rl_train_mode = true;

// ============================================
// 打印辅助函数
// ============================================
void print_header(const std::string& title, int width = 70) {
    std::cout << "\n" << std::string(width, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(width, '=') << "\n";
}

void print_env_info() {
    std::cout << std::left;
    std::cout << "  " << std::setw(20) << "Hardware:";
    
    cudaDeviceProp prop;
    if (cudaGetDeviceProperties(&prop, 0) == cudaSuccess) {
        std::cout << prop.name << "\n";
        std::cout << "  " << std::setw(20) << "GPU Memory:" 
                  << prop.totalGlobalMem / 1024 / 1024 << " MB\n";
        std::cout << "  " << std::setw(20) << "CUDA Cores:" 
                  << prop.multiProcessorCount << " SMs\n";
    } else {
        std::cout << "Unknown\n";
    }
    
    int cuda_version;
    cudaRuntimeGetVersion(&cuda_version);
    std::cout << "  " << std::setw(20) << "CUDA Version:"
              << cuda_version / 1000 << "." << (cuda_version % 100) / 10 << "\n";
    
    std::cout << std::string(70, '-') << "\n\n";
}

void print_config(const BenchConfig& cfg) {
    std::cout << std::left;
    std::cout << "  " << std::setw(20) << "Scheduler:" << cfg.scheduler_type << "\n";
    std::cout << "  " << std::setw(20) << "Total Requests:" << cfg.num_requests << "\n";
    std::cout << "  " << std::setw(20) << "Max Batch Size:" << cfg.batch_size << "\n";
    std::cout << "  " << std::setw(20) << "Workload Mix:"
              << "30% small, 50% medium, 20% large\n";
    std::cout << std::string(70, '-') << "\n\n";
}

void print_result(const BenchmarkResult& result) {
    std::cout << std::left << std::fixed << std::setprecision(2);
    std::cout << "  " << std::setw(20) << "Throughput:" 
              << result.throughput << " req/s\n";
    std::cout << "  " << std::setw(20) << "Avg Latency:" 
              << result.avg_latency_ms << " ms\n";
    std::cout << "  " << std::setw(20) << "P50 Latency:" 
              << result.p50_ms << " ms\n";
    std::cout << "  " << std::setw(20) << "P95 Latency:" 
              << result.p95_ms << " ms\n";
    std::cout << "  " << std::setw(20) << "P99 Latency:" 
              << result.p99_ms << " ms\n";
    std::cout << "  " << std::setw(20) << "Total Time:" 
              << result.total_time_sec << " s\n";
}

void print_comparison_table(const std::vector<BenchmarkResult>& results) {
    print_header("Scheduler Performance Comparison", 70);
    
    std::cout << std::left << std::fixed << std::setprecision(2);
    
    std::cout << std::setw(12) << "Scheduler"
              << std::setw(15) << "Throughput"
              << std::setw(12) << "P50 (ms)"
              << std::setw(12) << "P95 (ms)"
              << std::setw(12) << "P99 (ms)"
              << std::setw(12) << "vs FCFS" << "\n";
    
    std::cout << std::string(70, '-') << "\n";
    
    double fcfs_tp = 0.0;
    for (const auto& r : results) {
        if (r.scheduler_name == "FCFS") {
            fcfs_tp = r.throughput;
            break;
        }
    }
    
    for (const auto& r : results) {
        double improvement = 0.0;
        if (fcfs_tp > 0) {
            improvement = (r.throughput - fcfs_tp) / fcfs_tp * 100.0;
        }
        
        std::stringstream tp_ss;
        tp_ss << std::fixed << std::setprecision(1) << r.throughput << " req/s";
        
        std::stringstream imp_ss;
        imp_ss << (improvement >= 0 ? "+" : "") 
               << std::fixed << std::setprecision(1) << improvement << "%";
        
        std::cout << std::setw(12) << r.scheduler_name
                  << std::setw(15) << tp_ss.str()
                  << std::setw(12) << r.p50_ms
                  << std::setw(12) << r.p95_ms
                  << std::setw(12) << r.p99_ms
                  << std::setw(12) << imp_ss.str() << "\n";
    }
    
    std::cout << std::string(70, '=') << "\n\n";
    
    if (results.size() >= 2 && fcfs_tp > 0) {
        auto best = *std::max_element(results.begin(), results.end(),
            [](const BenchmarkResult& a, const BenchmarkResult& b) {
                return a.throughput < b.throughput;
            });
        
        double improvement = (best.throughput - fcfs_tp) / fcfs_tp * 100.0;
        std::cout << "  🏆 Best: " << best.scheduler_name 
                  << " (" << std::fixed << std::setprecision(1) << improvement 
                  << "% faster than FCFS)\n\n";
    }
}

// ============================================
// 请求生成
// ============================================
Request create_dummy_request(int id, int size, const std::string& op_name) {
    Request req;
    req.request_id = id;
    req.input_size = size;
    req.output_size = size;
    req.operator_name = op_name;
    req.priority = rand() % 4;
    
    // SGEMM是方阵，大小是 size x size
    const int matrix_elements = size * size;
    req.h_a = new float[matrix_elements];
    req.h_b = new float[matrix_elements];
    req.h_c = new float[matrix_elements];
    
    std::mt19937 rng(id);  // 每个请求用不同种子
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    
    for (int i = 0; i < matrix_elements; ++i) {
        req.h_a[i] = dist(rng);
        req.h_b[i] = dist(rng);
        req.h_c[i] = 0.0f;
    }
    
    Estimator::estimate(req);
    return req;
}

void free_request(Request& req) {
    delete[] req.h_a;
    delete[] req.h_b;
    delete[] req.h_c;
}

// ============================================
// 调度器工厂
// ============================================
std::unique_ptr<Scheduler> create_scheduler(const std::string& type) {
    if (type == "FCFS") return std::make_unique<FCFS_Scheduler>();
    if (type == "SJF") return std::make_unique<SJFScheduler>();
    if (type == "Priority") return std::make_unique<PriorityScheduler>();
    if (type == "RL") {
        auto rl = std::make_unique<RLScheduler>(g_rl_train_mode);
        if (!g_rl_model_path.empty()) {
            rl->load_model(g_rl_model_path);
            rl->set_epsilon(0.01f);
            std::cout << "[RL] Loaded model: " << g_rl_model_path << std::endl;
        }
        return rl;
    }
    LOG_WARN("Unknown scheduler: " << type << ", using SJF");
    return std::make_unique<SJFScheduler>();
}

// ============================================
// 运行单个基准测试
// ============================================
BenchmarkResult run_benchmark_internal(const BenchConfig& cfg) {
    BenchmarkResult result;
    result.scheduler_name = cfg.scheduler_type;
    result.total_requests = cfg.num_requests;
    
    // 初始化
    CUDABackend backend(0, 4);
    RequestQueue queue;
    Metrics metrics;
    auto scheduler = create_scheduler(cfg.scheduler_type);
    
    Worker worker(&backend, &queue, scheduler.get(), &metrics);
    
    if (!backend.initialize()) {
        LOG_ERROR("Failed to initialize CUDA backend");
        return result;
    }
    
    // Warmup
    std::cout << "[1/4] Warmup... " << std::flush;
    for (int i = 0; i < cfg.warmup_requests; ++i) {
        Request req = create_dummy_request(-i, 64, "sgemm_tiled");
        backend.submit(req);
        free_request(req);
    }
    std::cout << "OK\n";
    
    // 生成工作负载
    std::cout << "[2/4] Generating workload... " << std::flush;
    std::vector<Request> requests;
    
    const int sizes[] = {64, 128, 256, 512};  // 矩阵边长
    const int weights[] = {3, 5, 1, 1};       // 权重
    int total_weight = 10;
    
    for (int i = 0; i < cfg.num_requests; ++i) {
        int r = rand() % total_weight;
        int size_idx = 0;
        int cum = 0;
        for (size_idx = 0; size_idx < 4; ++size_idx) {
            cum += weights[size_idx];
            if (r < cum) break;
        }
        int size = sizes[size_idx];
        requests.push_back(create_dummy_request(i, size, "sgemm_tiled"));
        queue.push(requests.back());
    }
    std::cout << "OK (" << cfg.num_requests << " requests)\n";
    
    // 执行
    std::cout << "[3/4] Running benchmark... " << std::flush;
    auto start = std::chrono::high_resolution_clock::now();
    
    worker.start();
    
    while (!queue.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    worker.stop();
    
    auto end = std::chrono::high_resolution_clock::now();
    result.total_time_sec = std::chrono::duration<double>(end - start).count();
    result.throughput = cfg.num_requests / result.total_time_sec;
    std::cout << "OK\n\n";
    
    // 收集指标
    auto exec_summary = metrics.get_execution_latency_summary();
    result.avg_latency_ms = exec_summary.avg_ms;
    result.p50_ms = exec_summary.p50_ms;
    result.p95_ms = exec_summary.p95_ms;
    result.p99_ms = exec_summary.p99_ms;
    
    // 打印结果
    print_result(result);
    
    // 清理
    std::cout << "\n[4/4] Cleaning up... " << std::flush;
    for (auto& req : requests) {
        free_request(req);
    }
    backend.shutdown();
    std::cout << "OK\n";
    
    return result;
}

void run_benchmark(const BenchConfig& cfg) {
    print_header("CudaPipeline Benchmark v1.0", 70);
    print_env_info();
    print_config(cfg);
    
    auto result = run_benchmark_internal(cfg);
    
    print_header("Benchmark Results", 70);
    print_result(result);
}

// ============================================
// 对比所有调度器
// ============================================
void run_comparison() {
    print_header("CudaPipeline Scheduler Comparison", 70);
    print_env_info();
    
    std::vector<std::string> schedulers = {"FCFS", "SJF", "Priority", "RL"};
    std::vector<BenchmarkResult> all_results;
    
    BenchConfig cfg;
    cfg.num_requests = 500;  // 对比用较少请求，节省时间
    
    for (const auto& sched : schedulers) {
        std::cout << "\n>>> Running " << sched << " scheduler...\n";
        cfg.scheduler_type = sched;
        auto result = run_benchmark_internal(cfg);
        all_results.push_back(result);
    }
    
    print_comparison_table(all_results);
}

// ============================================
// Main 函数
// ============================================
int main(int argc, char* argv[]) {
    BenchConfig cfg;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--scheduler" && i + 1 < argc) {
            cfg.scheduler_type = argv[++i];
        } else if (arg == "--requests" && i + 1 < argc) {
            cfg.num_requests = std::stoi(argv[++i]);
        } else if (arg == "--batch" && i + 1 < argc) {
            cfg.batch_size = std::stoi(argv[++i]);
        } else if (arg == "--load-rl-model" && i + 1 < argc) {
            g_rl_model_path = argv[++i];
            g_rl_train_mode = false;
        } else if (arg == "--compare") {
            run_comparison();
            return 0;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "CudaPipeline Benchmark v1.0\n\n";
            std::cout << "Usage: " << argv[0] << " [options]\n\n";
            std::cout << "Options:\n";
            std::cout << "  --scheduler TYPE       FCFS / SJF / Priority / RL (default: SJF)\n";
            std::cout << "  --requests N           Number of requests (default: 100)\n";
            std::cout << "  --batch N              Max batch size (default: 8)\n";
            std::cout << "  --load-rl-model PATH   Load trained RL model (inference mode)\n";
            std::cout << "  --compare              Run all schedulers comparison\n";
            std::cout << "  -h, --help             Show this help\n\n";
            std::cout << "Examples:\n";
            std::cout << "  " << argv[0] << " --scheduler RL --requests 1000\n";
            std::cout << "  " << argv[0] << " --compare\n";
            return 0;
        }
    }

    run_benchmark(cfg);
    return 0;
}
