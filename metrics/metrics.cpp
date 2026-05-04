#include "metrics/metrics.h"
#include <iostream>
#include <iomanip>
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

Metrics::Summary Metrics::compute_summary(const std::vector<long long>& data)
{
    Summary s = {0, 0, 0, 0, 0, 0, 0};
    if (data.empty()) return s;

    std::vector<long long> sorted = data;
    std::sort(sorted.begin(), sorted.end());

    s.count = sorted.size();
    s.min_ns = sorted.front();
    s.max_ns = sorted.back();
    
    long long sum = std::accumulate(sorted.begin(), sorted.end(), 0LL);
    s.avg_ms = static_cast<double>(sum) / s.count / 1000000.0;
    
    s.p50_ms = static_cast<double>(sorted[s.count * 50 / 100]) / 1000000.0;
    s.p95_ms = static_cast<double>(sorted[s.count * 95 / 100]) / 1000000.0;
    s.p99_ms = static_cast<double>(sorted[s.count * 99 / 100]) / 1000000.0;

    return s;
}

Metrics::Summary Metrics::get_queue_latency_summary() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return compute_summary(queue_latencies_);
}

Metrics::Summary Metrics::get_execution_latency_summary() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return compute_summary(exec_latencies_);
}

void Metrics::print() const
{
    auto queue_s = get_queue_latency_summary();
    auto exec_s = get_execution_latency_summary();

    std::cout << std::fixed << std::setprecision(3);
    
    std::cout << "\n┌─────────────────────────────────────────────────────┐\n";
    std::cout << "│              Performance Metrics                    │\n";
    std::cout << "├──────────────┬─────────┬─────────┬─────────┬─────────┤\n";
    std::cout << "│ Metric       │ Avg(ms) │ P50(ms) │ P95(ms) │ P99(ms) │\n";
    std::cout << "├──────────────┼─────────┼─────────┼─────────┼─────────┤\n";
    
    if (queue_s.count > 0) {
        std::cout << "│ Queue Latency│ " 
                  << std::setw(7) << queue_s.avg_ms << " │ "
                  << std::setw(7) << queue_s.p50_ms << " │ "
                  << std::setw(7) << queue_s.p95_ms << " │ "
                  << std::setw(7) << queue_s.p99_ms << " │\n";
    }
    
    if (exec_s.count > 0) {
        std::cout << "│ Exec Latency │ " 
                  << std::setw(7) << exec_s.avg_ms << " │ "
                  << std::setw(7) << exec_s.p50_ms << " │ "
                  << std::setw(7) << exec_s.p95_ms << " │ "
                  << std::setw(7) << exec_s.p99_ms << " │\n";
    }
    
    std::cout << "└──────────────┴─────────┴─────────┴─────────┴─────────┘\n";
    
    std::cout << "\n  Total Samples: Queue=" << queue_s.count 
              << ", Exec=" << exec_s.count << "\n";
}

void Metrics::reset()
{
    std::lock_guard<std::mutex> lock(mutex_);
    queue_latencies_.clear();
    exec_latencies_.clear();
    batch_sizes_.clear();
}
