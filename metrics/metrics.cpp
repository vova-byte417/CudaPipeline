#include "metrics/metrics.h"
#include <iostream>
#include <numeric>

void Metrics::record_queue_latency(long long ns)
{
    std::lock_guard<std::mutex> lock(mutex_);
    queue_latencies_.push_back(ns);
}

void Metrics::record_execution_latency(long long ns)
{
    std::lock_guard<std::mutex> lock(mutex_);
    exec_latencies_.push_back(ns);
}

void Metrics::record_batch_size(int size)
{
    std::lock_guard<std::mutex> lock(mutex_);
    batch_sizes_.push_back(size);
}

void Metrics::increment_requests(int n)
{
    total_requests_ += n;
}

void Metrics::reset()
{
    std::lock_guard<std::mutex> lock(mutex_);
    queue_latencies_.clear();
    exec_latencies_.clear();
    batch_sizes_.clear();
    total_requests_ = 0;
}

void Metrics::print()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (exec_latencies_.empty()) {
        std::cout << "[Metrics] No data collected\n";
        return;
    }

    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "                    PERFORMANCE METRICS                     \n";
    std::cout << std::string(60, '=') << "\n\n";

    // ==================== Queue Latency ====================
    std::cout << "[Queue Latency] (us)\n";
    long long q_sum = std::accumulate(queue_latencies_.begin(), queue_latencies_.end(), 0LL);
    double q_avg = static_cast<double>(q_sum) / queue_latencies_.size() / 1000.0;
    double q_p50 = percentile(queue_latencies_, 0.50) / 1000.0;
    double q_p95 = percentile(queue_latencies_, 0.95) / 1000.0;
    double q_p99 = percentile(queue_latencies_, 0.99) / 1000.0;
    
    std::cout << "  Avg: " << std::fixed << std::setprecision(2) << q_avg << "  |  ";
    std::cout << "P50: " << q_p50 << "  |  P95: " << q_p95 << "  |  P99: " << q_p99 << "\n\n";

    // ==================== Execution Latency ====================
    std::cout << "[Execution Latency] (us)\n";
    long long e_sum = std::accumulate(exec_latencies_.begin(), exec_latencies_.end(), 0LL);
    double e_avg = static_cast<double>(e_sum) / exec_latencies_.size() / 1000.0;
    double e_p50 = percentile(exec_latencies_, 0.50) / 1000.0;
    double e_p95 = percentile(exec_latencies_, 0.95) / 1000.0;
    double e_p99 = percentile(exec_latencies_, 0.99) / 1000.0;
    
    std::cout << "  Avg: " << e_avg << "  |  P50: " << e_p50;
    std::cout << "  |  P95: " << e_p95 << "  |  P99: " << e_p99 << "\n\n";

    // ==================== Batch Size ====================
    if (!batch_sizes_.empty()) {
        std::cout << "[Batch Size Distribution]\n";
        int b_sum = std::accumulate(batch_sizes_.begin(), batch_sizes_.end(), 0);
        double b_avg = static_cast<double>(b_sum) / batch_sizes_.size();
        int b_min = *std::min_element(batch_sizes_.begin(), batch_sizes_.end());
        int b_max = *std::max_element(batch_sizes_.begin(), batch_sizes_.end());
        
        std::cout << "  Avg: " << std::fixed << std::setprecision(1) << b_avg;
        std::cout << "  |  Min: " << b_min << "  |  Max: " << b_max;
        std::cout << "  |  Total Batches: " << batch_sizes_.size() << "\n\n";
    }

    // ==================== Throughput ====================
    std::cout << "[Throughput]\n";
    std::cout << "  Total Requests: " << total_requests_ << "\n";
    std::cout << "  Total Batches:  " << batch_sizes_.size() << "\n";

    std::cout << std::string(60, '=') << "\n\n";
}