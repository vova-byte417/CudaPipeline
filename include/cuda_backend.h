#pragma once

#include "backend.h"
#include "request.h"
#include "runtime/batch.h"
#include "memory_pool.h"
#include <cuda_runtime.h>
#include <string>
#include <vector>

class CUDABackend : public Backend
{
public:
    CUDABackend();
    virtual ~CUDABackend();

    bool initialize() override;
    bool submit_batch(Batch& batch) override;
    void shutdown() override;
    bool submit(Request& req) override;

    // Batch execution optimization: true batch processing
    bool submit_batch_optimized(Batch& batch);

private:
    using operator_func_t = void (*)(float*, float*, float*, int);

    cudaStream_t stream_ = nullptr;
    void* operator_handle_ = nullptr;
    operator_func_t vector_add_ = nullptr;
    MemoryPool mem_pool_;  // GPU 内存池

    bool load_operator(const std::string& path);

    // Helper for batch memory management
    struct BatchMemory {
        float* d_a = nullptr;
        float* d_b = nullptr;
        float* d_c = nullptr;
        std::vector<float> h_c_batch;
        size_t total_elements = 0;
    };

    bool alloc_batch_memory(Batch& batch, BatchMemory& mem);
    void free_batch_memory(BatchMemory& mem);
};