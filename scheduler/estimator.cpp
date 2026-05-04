#include "estimator.h"
#include "util.h"
#include <cmath>

void Estimator::estimate(Request& req)
{
    req.arrival_time = get_current_timestamp();  // 你之前实现的函数
    req.gpu_memory_estimate = (req.input_size * 3 * 4) / (1024 * 1024) + 10; // MB
    req.estimated_flops = flops_model(req.input_size, req.operator_name);
    req.estimated_exec_time = simple_exec_time_model(req.input_size, req.operator_name);
    req.task_type = (req.input_size > 8192) ? 2 : (req.input_size > 2048 ? 1 : 0);
}

float Estimator::simple_exec_time_model(int input_size, const std::string& op_name)
{
    float base = input_size / 1000000.0f;  // 归一化
    if (op_name == "vector_add") return base * 0.08f + 0.5f;
    if (op_name == "matmul")     return base * 0.6f + 2.0f;
    return base * 0.3f + 1.0f;
}

uint32_t Estimator::flops_model(int input_size, const std::string& op_name)
{
    if (op_name == "matmul") return static_cast<uint32_t>(input_size * input_size * 2LL);
    return static_cast<uint32_t>(input_size * 10);
}