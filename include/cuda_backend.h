#pragma once

#include "backend.h"
#include "request.h"
#include "runtime/batch.h"
#include <cuda_runtime.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

class CUDABackend : public Backend
{
public:
    CUDABackend(int device_id = 0, int num_streams = 4);
    virtual ~CUDABackend();

    bool initialize() override;
    bool submit(Request& req) override;
    bool submit_batch(Batch& batch) override;
    void shutdown() override;

private:
    int device_id_;
    std::vector<cudaStream_t> streams_;
    int next_stream_ = 0;
    mutable std::mutex stream_mutex_;

    // 内存池
    std::unordered_map<size_t, std::vector<void*>> memory_pool_;
    mutable std::mutex pool_mutex_;
    
    void* pool_allocate(size_t bytes);
    void pool_free(void* ptr, size_t bytes);
    void pool_clear();

    // 算子句柄（简化，就一个）
    void* operator_handle_ = nullptr;
    using sgemm_func_t = void(*)(float*, float*, float*, int);
    sgemm_func_t sgemm_func_ = nullptr;

    cudaStream_t get_next_stream();
};
