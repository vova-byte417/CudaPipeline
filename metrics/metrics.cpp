#include "metrics/metrics.h"
#include <iostream>

void Metrics::record_queue_latency(long long ns)
{
    queue_latency_sum_ += ns;
}

void Metrics::record_execution_latency(long long ns)
{
    exec_latency_sum_ += ns;
    count_++;
}

void Metrics::print()
{
    if (count_ == 0)
        return;

    std::cout << "Avg Queue Latency: "
              << queue_latency_sum_ / count_
              << " ns\n";

    std::cout << "Avg Exec Latency: "
              << exec_latency_sum_ / count_
              << " ns\n";
}