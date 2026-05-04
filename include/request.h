#pragma once

#include <string>
#include <cstdint>

// 在 request.h 顶部添加
using Timestamp = uint64_t;  // 微秒（us）



class Request
{
public:

    // request tracing
    uint64_t request_id;

    // execution metadata
    int input_size;
    int output_size;

    int priority;

    // operator dispatch
    std::string operator_name;

    // host buffers
    float* h_a;
    float* h_b;
    float* h_c;

    // timestamp
    Timestamp enqueue_ts;

    Timestamp arrival_time;        // 请求到达系统的时间（us或ms）
    int gpu_memory_estimate;      // 估算显存占用 (bytes 或 MB)
    int compute_intensity;        // 计算强度 (FLOPs, 或归一化 1~10)
    float estimated_exec_time;    // 模型预测的执行时间 (ms)

    int task_type;                    // 0=short, 1=medium, 2=long
    float data_intensity;             // 内存密集度 (bytes/FLOP)
    uint32_t estimated_flops;         // 精确 FLOPs（用于 SJF/ML调度）
    bool is_preemptible;              // 是否允许抢占
};