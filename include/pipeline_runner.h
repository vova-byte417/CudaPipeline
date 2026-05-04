#pragma once

#include <string>
#include <memory>
#include <chrono>
#include <functional>
#include <vector>
#include <map>

#include "backend.h"
#include "cpu_backend.h"
#include "queue.h"
#include "priority_queue.h"
#include "deadline_queue.h"
#include "runtime/worker.h"
#include "scheduler/fcfs_scheduler.h"
#include "scheduler/priority_scheduler.h"
#include "scheduler/edf_scheduler.h"
#include "scheduler/rl_scheduler.h"
#include "metrics/metrics.h"

// 调度器类型枚举
enum class SchedulerType {
    FCFS,          // 先来先服务
    PRIORITY,      // 优先级调度
    EDF,           // 最早截止时间优先
    RL             // 强化学习自适应
};

// 后端类型枚举
enum class BackendType {
    CPU,           // CPU 后端
    CUDA           // CUDA GPU 后端
};

// Pipeline 配置
struct PipelineConfig {
    SchedulerType scheduler_type = SchedulerType::FCFS;
    BackendType backend_type = BackendType::CPU;
    int worker_threads = 1;
    bool enable_metrics = true;
    bool enable_memory_pool = true;
    size_t max_batch_size = 8;
    std::string name = "CudaPipeline";
    
    // RL 调度器专用配置
    struct RLConfig {
        double learning_rate = 0.1;
        double discount_factor = 0.95;
        double exploration_rate = 0.3;
    } rl_config;
};

// Pipeline 运行统计
struct PipelineStats {
    size_t total_requests = 0;
    size_t total_batches = 0;
    double avg_batch_size = 0;
    double avg_queue_latency_us = 0;
    double avg_execution_latency_us = 0;
    double p99_execution_latency_us = 0;
    double throughput_rps = 0;
    std::chrono::milliseconds total_runtime_ms{0};
};

// 请求生成器回调
using RequestGenerator = std::function<Request(size_t request_id)>;

// 简化的 Pipeline Runner - 产品化入口
class PipelineRunner {
public:
    explicit PipelineRunner(const PipelineConfig& config = PipelineConfig())
        : config_(config), running_(false) {
    }

    ~PipelineRunner() {
        if (running_) {
            stop();
        }
    }

    // 初始化 Pipeline
    bool initialize() {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "      " << config_.name << " Initializing      \n";
        std::cout << std::string(60, '=') << "\n\n";

        // 1. 创建后端
        create_backend();
        
        // 2. 创建队列
        create_queue();
        
        // 3. 创建调度器
        create_scheduler();
        
        // 4. 创建 Metrics
        if (config_.enable_metrics) {
            metrics_ = std::make_unique<Metrics>();
        }
        
        // 5. 初始化后端
        if (!backend_->initialize()) {
            std::cerr << "[ERROR] Backend initialization failed!\n";
            return false;
        }
        
        // 6. 创建 Worker
        worker_ = std::make_unique<Worker>(backend_.get(), queue_base_, scheduler_.get(), metrics_.get());
        
        print_config();
        return true;
    }

    // 启动 Pipeline
    void start() {
        if (!worker_) {
            std::cerr << "[ERROR] Pipeline not initialized!\n";
            return;
        }
        
        start_time_ = std::chrono::high_resolution_clock::now();
        worker_->start();
        running_ = true;
        
        std::cout << "[Pipeline] Started successfully!\n\n";
    }

    // 提交单个请求
    void submit(const Request& req) {
        if (queue_base_) {
            queue_base_->push(req);
        }
    }

    // 批量提交请求
    template<typename Iterator>
    void submit_batch(Iterator begin, Iterator end) {
        for (auto it = begin; it != end; ++it) {
            submit(*it);
        }
    }

    // 使用生成器提交 N 个请求
    void generate_requests(size_t count, RequestGenerator generator) {
        for (size_t i = 0; i < count; i++) {
            Request req = generator(i);
            submit(req);
        }
        std::cout << "[Pipeline] Generated and submitted " << count << " requests\n";
    }

    // 等待所有请求处理完成（简单实现：轮询+sleep）
    void wait_for_completion(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) {
        auto start = std::chrono::high_resolution_clock::now();
        
        while (running_) {
            if (queue_base_ && queue_base_->empty()) {
                // 队列为空，再等一下确保最后一个 batch 完成
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if (queue_base_->empty()) {
                    break;
                }
            }
            
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start);
            if (elapsed > timeout) {
                std::cout << "[Pipeline] Timeout waiting for completion\n";
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    // 停止 Pipeline
    void stop() {
        if (!running_) return;
        
        if (worker_) {
            worker_->stop();
        }
        if (backend_) {
            backend_->shutdown();
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        stats_.total_runtime_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time_);
        
        running_ = false;
        std::cout << "\n[Pipeline] Stopped gracefully\n";
    }

    // 打印性能报告
    void print_report() {
        if (metrics_) {
            metrics_->print();
        }
        
        print_rl_stats();
    }

    // 获取统计信息
    PipelineStats get_stats() const {
        return stats_;
    }

    // 一键运行：初始化 + 启动 + 生成请求 + 等待 + 报告 + 停止
    bool run_benchmark(size_t num_requests, 
                        RequestGenerator generator,
                        std::chrono::milliseconds wait_time = std::chrono::milliseconds(2000)) {
        if (!initialize()) {
            return false;
        }
        
        start();
        
        // 生成并提交请求
        for (size_t i = 0; i < num_requests; i++) {
            Request req = generator(i);
            submit(req);
            
            // 模拟请求间歇
            if (i % 10 == 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
        
        // 等待处理完成
        std::this_thread::sleep_for(wait_time);
        
        stop();
        print_report();
        
        return true;
    }

private:
    // 队列基类适配器
    class QueueBase {
    public:
        virtual ~QueueBase() = default;
        virtual void push(const Request& req) = 0;
        virtual bool empty() const = 0;
        virtual size_t size() const = 0;
    };

    template<typename QueueT>
    class QueueAdapter : public QueueBase {
    public:
        QueueT queue;
        void push(const Request& req) override { queue.push(req); }
        bool empty() const { return const_cast<QueueT&>(queue).empty(); }
        size_t size() const { return 0; }
    };

    PipelineConfig config_;
    bool running_;
    std::chrono::high_resolution_clock::time_point start_time_;
    PipelineStats stats_;

    std::unique_ptr<Backend> backend_;
    std::unique_ptr<QueueBase> queue_base_;
    std::unique_ptr<Scheduler> scheduler_;
    std::unique_ptr<Metrics> metrics_;
    std::unique_ptr<Worker> worker_;

    void create_backend() {
        // 默认 CPU 后端
        backend_ = std::make_unique<CPUBackend>();
    }

    void create_queue() {
        switch (config_.scheduler_type) {
            case SchedulerType::PRIORITY:
                queue_base_ = std::make_unique<QueueAdapter<PriorityRequestQueue>>();
                break;
            case SchedulerType::EDF:
                queue_base_ = std::make_unique<QueueAdapter<DeadlineQueue>>();
                break;
            default:
                queue_base_ = std::make_unique<QueueAdapter<RequestQueue>>();
                break;
        }
    }

    void create_scheduler() {
        switch (config_.scheduler_type) {
            case SchedulerType::FCFS:
                scheduler_ = std::make_unique<FCFS_Scheduler>();
                break;
            case SchedulerType::PRIORITY:
                scheduler_ = std::make_unique<PriorityScheduler>();
                break;
            case SchedulerType::EDF:
                scheduler_ = std::make_unique<EDF_Scheduler>();
                break;
            case SchedulerType::RL:
                scheduler_ = std::make_unique<RLScheduler>(
                    config_.rl_config.learning_rate,
                    config_.rl_config.discount_factor,
                    config_.rl_config.exploration_rate
                );
                break;
        }
    }

    void print_config() {
        std::cout << "=== Configuration ===\n";
        std::cout << "  Scheduler:   ";
        switch (config_.scheduler_type) {
            case SchedulerType::FCFS:     std::cout << "FCFS (先来先服务)\n"; break;
            case SchedulerType::PRIORITY: std::cout << "PRIORITY (优先级)\n"; break;
            case SchedulerType::EDF:      std::cout << "EDF (最早截止时间优先)\n"; break;
            case SchedulerType::RL:       std::cout << "RL (强化学习自适应)\n"; break;
        }
        std::cout << "  Backend:     CPU\n";
        std::cout << "  Max Batch:   " << config_.max_batch_size << "\n";
        std::cout << "  Metrics:     " << (config_.enable_metrics ? "Enabled" : "Disabled") << "\n";
        std::cout << "=====================\n\n";
    }

    void print_rl_stats() {
        if (config_.scheduler_type == SchedulerType::RL) {
            auto* rl = dynamic_cast<RLScheduler*>(scheduler_.get());
            if (rl) {
                rl->print_stats();
            }
        }
    }
};
