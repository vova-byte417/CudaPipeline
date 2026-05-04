#pragma once

#include <atomic>
#include <chrono>
#include <vector>
#include <mutex>
#include <algorithm>

class Metrics
{
public:
    void record_queue_latency(long long ns);
    void record_execution_latency(long long ns);
    void record_batch_size(int size);
    
    void print() const;
    void reset();

    struct Summary {
        double avg_ms;
        double p50_ms;
        double p95_ms;
        double p99_ms;
        long long min_ns;
        long long max_ns;
        size_t count;
    };
    
    Summary get_queue_latency_summary() const;
    Summary get_execution_latency_summary() const;

private:
    mutable std::mutex mutex_;
    std::vector<long long> queue_latencies_;
    std::vector<long long> exec_latencies_;
    std::vector<int> batch_sizes_;

    static Summary compute_summary(const std::vector<long long>& data);
};
