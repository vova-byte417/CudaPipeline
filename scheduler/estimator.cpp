#include "estimator.h"

// 静态成员初始化
std::unordered_map<std::string, Estimator::OperatorProfile> Estimator::profiles_ = {
    {"vector_add", {0.5, 0}},
    {"matmul", {50.0, 1024}},
    {"conv2d", {100.0, 4096}}
};

void Estimator::estimate(Request& req) {
    auto it = profiles_.find(req.operator_name);
    if (it != profiles_.end()) {
        req.estimated_exec_time = req.input_size * it->second.time_per_element_ns / 1000000.0;
        req.gpu_memory_estimate = req.input_size * sizeof(float) * 3 + it->second.memory_overhead_bytes;
    } else {
        req.estimated_exec_time = req.input_size * 1.0 / 1000000.0;
        req.gpu_memory_estimate = req.input_size * sizeof(float) * 3;
    }

    req.arrival_time = 0;
    req.compute_intensity = 1;
    req.is_preemptible = true;
}

void Estimator::register_profile(const std::string& op_name, const OperatorProfile& profile) {
    profiles_[op_name] = profile;
}
