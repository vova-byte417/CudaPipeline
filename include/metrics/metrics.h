#pragma once

#include <atomic>
#include <chrono>
#include <vector>
#include <mutex>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <iomanip>

class Metrics
{
public:
    // 记录单次延迟（纳秒）
    void record_queue_latency(long long ns);
    void record_execution_latency(long long ns);
    void record_batch_size(int size);

    // 记录吞吐量
    void increment_requests(int n = 1);

    // 打印完整统计报告
    void print();

    // 重置所有统计
    void reset();

private:
    // 计算百分位
    template<typename T>
    T percentile(std::vector<T>& data, double p) {
        if (data.empty()) return 0;
        std::sort(data.begin(), data.end());
        size_t idx = static_cast<size_t>(std::ceil(p * data.size())) - 1;
        idx = std::min(idx, data.size() - 1);
        return data[idx];
    }

    // 延迟数据（保留所有样本用于百分位计算）
    std::mutex mutex_;
    std::vector<long long> queue_latencies_;
    std::vector<long long> exec_latencies_;
    std::vector<int> batch_sizes_;

    std::atomic<int> total_requests_ = 0;
    std::atomic<long long> start_time_ns_ = 0;
};