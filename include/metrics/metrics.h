#pragma once

#include <atomic>
#include <chrono>

class Metrics
{
public:

    void record_queue_latency(
        long long ns
    );

    void record_execution_latency(
        long long ns
    );

    void print();

private:

    std::atomic<long long> queue_latency_sum_ = 0;
    std::atomic<long long> exec_latency_sum_ = 0;
    std::atomic<int> count_ = 0;
};