#pragma once

#include "backend.h"
#include "request.h"
#include "runtime/batch.h"
#include <cuda_runtime.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <memory>

// 内存块
struct MemoryBlock {
    void* ptr = nullptr;
    size_t size = 0;
    bool in_use = false;
};

// 算子函数签名
using OperatorFunc = void (*)(float*, float*, float*, int);

class CUDABackend : public Backend
{
public:
    CUDABackend(int device_id = 0, int num_streams = 4);
    virtual ~CUDABackend();

    bool initialize() override;
    bool submit(Request& req) override;
    bool submit_batch(Batch& batch) override;
    void shutdown() override;

    // 新增：异步提交接口
    struct AsyncResult {
        cudaEvent_t event;
        bool completed = false;
        std::vector<float> result;
    };
    std::shared_ptr<AsyncResult> submit_async(Request& req);
    
    // 新增：算子注册
    bool register_operator(const std::string& name, const std::string& library_path);
    bool has_operator(const std::string& name) const;

private:
    int device_id_;
    int num_streams_;
    std::vector<cudaStream_t> streams_;
    int next_stream_ = 0;
    mutable std::mutex stream_mutex_;

    // 内存池
    std::unordered_map<size_t, std::vector<void*>> memory_pool_;
    mutable std::mutex pool_mutex_;
    
    void* pool_allocate(size_t bytes);
    void pool_free(void* ptr, size_t bytes);
    void pool_clear();

    // 算子管理
    std::unordered_map<std::string, void*> library_handles_;
    std::unordered_map<std::string, OperatorFunc> operators_;
    bool load_operator_internal(const std::string& name, const std::string& path);

    cudaStream_t get_next_stream();
};
