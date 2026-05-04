#include "cpu_backend.h"
#include "request.h"
#include "runtime/batch.h"

#include <iostream>
#include <chrono>

CPUBackend::CPUBackend()
{
}

CPUBackend::~CPUBackend()
{
    shutdown();
}

bool CPUBackend::initialize()
{
    std::cout << "[CPUBackend] initialized successfully!" << std::endl;
    return true;
}

void CPUBackend::vector_add_cpu(float* a, float* b, float* c, int n)
{
    for (int i = 0; i < n; i++) {
        c[i] = a[i] + b[i];
    }
}

bool CPUBackend::submit(Request& req)
{
    int n = req.input_size;
    if (n <= 0 || !req.h_a || !req.h_b || !req.h_c) {
        return false;
    }

    std::cout << "[CPUBackend] executing request " << req.request_id 
              << " with n = " << n << std::endl;

    vector_add_cpu(req.h_a, req.h_b, req.h_c, n);

    std::cout << "[CPUBackend] result[0] = " << req.h_c[0] << std::endl;
    std::cout << "[CPUBackend] result[1] = " << req.h_c[1] << std::endl;
    std::cout << "[CPUBackend] result[2] = " << req.h_c[2] << std::endl;

    return true;
}

bool CPUBackend::submit_batch(Batch& batch)
{
    if (batch.requests.empty()) {
        return false;
    }

    std::cout << "[CPUBackend] executing batch with " << batch.requests.size() 
              << " requests" << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    // 批量处理所有请求
    for (auto& req : batch.requests) {
        int n = req.input_size;
        if (n > 0 && req.h_a && req.h_b && req.h_c) {
            vector_add_cpu(req.h_a, req.h_b, req.h_c, n);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // 显示第一个请求的结果
    if (!batch.requests.empty()) {
        auto& req0 = batch.requests[0];
        std::cout << "[CPUBackend] req[" << req0.request_id << "] result[0] = " << req0.h_c[0] << std::endl;
        std::cout << "[CPUBackend] req[" << req0.request_id << "] result[1] = " << req0.h_c[1] << std::endl;
        std::cout << "[CPUBackend] req[" << req0.request_id << "] result[2] = " << req0.h_c[2] << std::endl;
    }

    std::cout << "[CPUBackend] batch completed in " << duration.count() << " us" << std::endl;

    return true;
}

void CPUBackend::shutdown()
{
    mem_pool_.print_stats();
    mem_pool_.clear();
    std::cout << "[CPUBackend] shutdown complete" << std::endl;
}
