#pragma once

#include <string>
#include <cstdint>

// 优先级定义
enum class Priority : int {
    HIGH = 0,    // 高优先级：实时任务
    MEDIUM = 1,   // 中优先级：普通任务
    LOW = 2,      // 低优先级：批量任务
    COUNT = 3
};

class Request
{
public:

    // request tracing
    uint64_t request_id;

    // execution metadata
    int input_size;
    int output_size;

    Priority priority;    // 优先级（用于优先级调度）
    uint64_t deadline;    // 截止时间（纳秒时间戳，用于 EDF 调度）
    
    // 饥饿避免：入队时间戳，超过阈值自动升级
    uint64_t enqueue_ts;

    // operator dispatch
    std::string operator_name;

    // host buffers
    float* h_a;
    float* h_b;
    float* h_c;

    // timestamp
    uint64_t enqueue_ts;
};