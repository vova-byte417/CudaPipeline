#include "cuda_backend.h"
#include "request.h"
#include "runtime/batch.h"

#include <cuda_runtime.h>
#include <dlfcn.h>

#include <iostream>
#include <vector>
#include <cstring>
#include <algorithm>

// 基础错误处理宏 - 区分返回bool和其他返回类型
#define CHECK_CUDA_RET_FALSE(x)                              \
do {                                                        \
    cudaError_t err = (x);                                  \
    if (err != cudaSuccess) {                               \
        std::cerr << "[CUDA Error] " << __FILE__ << ":"     \
                  << __LINE__ << ": " << cudaGetErrorString(err) << std::endl; \
        return false;                                       \
    }                                                       \
} while(0)

#define CHECK_CUDA_RET_NULL(x)                               \
do {                                                        \
    cudaError_t err = (x);                                  \
    if (err != cudaSuccess) {                               \
        std::cerr << "[CUDA Error] " << __FILE__ << ":"     \
                  << __LINE__ << ": " << cudaGetErrorString(err) << std::endl; \
        return nullptr;                                     \
    }                                                       \
} while(0)

#define CHECK_CUDA_VOID(x)                                  \
do {                                                        \
    cudaError_t err = (x);                                  \
    if (err != cudaSuccess) {                               \
        std::cerr << "[CUDA Error] " << __FILE__ << ":"     \
                  << __LINE__ << ": " << cudaGetErrorString(err) << std::endl; \
    }                                                       \
} while(0)

CUDABackend::CUDABackend(int device_id, int num_streams)
    : device_id_(device_id), num_streams_(num_streams)
{
}

CUDABackend::~CUDABackend()
{
    shutdown();
}

bool CUDABackend::initialize()
{
    CHECK_CUDA_RET_FALSE(cudaSetDevice(device_id_));

    streams_.resize(num_streams_);
    for (int i = 0; i < num_streams_; ++i) {
        CHECK_CUDA_RET_FALSE(cudaStreamCreate(&streams_[i]));
    }

    if (!register_operator("vector_add", "./build/operators/libvector_add.so")) {
        std::cerr << "Warning: Failed to load default operator vector_add" << std::endl;
    }

    return true;
}

void* CUDABackend::pool_allocate(size_t bytes)
{
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    size_t aligned_bytes = (bytes + 255) & ~255;
    
    auto it = memory_pool_.find(aligned_bytes);
    if (it != memory_pool_.end() && !it->second.empty()) {
        void* ptr = it->second.back();
        it->second.pop_back();
        return ptr;
    }
    
    void* ptr = nullptr;
    cudaError_t err = cudaMalloc(&ptr, aligned_bytes);
    if (err != cudaSuccess) {
        std::cerr << "cudaMalloc failed: " << cudaGetErrorString(err) << std::endl;
        return nullptr;
    }
    return ptr;
}

void CUDABackend::pool_free(void* ptr, size_t bytes)
{
    if (!ptr) return;
    
    std::lock_guard<std::mutex> lock(pool_mutex_);
    size_t aligned_bytes = (bytes + 255) & ~255;
    memory_pool_[aligned_bytes].push_back(ptr);
}

void CUDABackend::pool_clear()
{
    std::lock_guard<std::mutex> lock(pool_mutex_);
    for (auto& pair : memory_pool_) {
        for (void* ptr : pair.second) {
            CHECK_CUDA_VOID(cudaFree(ptr));
        }
    }
    memory_pool_.clear();
}

bool CUDABackend::register_operator(const std::string& name, const std::string& library_path)
{
    return load_operator_internal(name, library_path);
}

bool CUDABackend::load_operator_internal(const std::string& name, const std::string& path)
{
    void* handle = dlopen(path.c_str(), RTLD_NOW);
    if (!handle) {
        std::cerr << "dlopen failed for " << path << ": " << dlerror() << std::endl;
        return false;
    }

    void* sym = dlsym(handle, name.c_str());
    if (!sym) {
        std::cerr << "dlsym failed for " << name << ": " << dlerror() << std::endl;
        dlclose(handle);
        return false;
    }

    library_handles_[name] = handle;
    operators_[name] = reinterpret_cast<OperatorFunc>(sym);
    std::cout << "Loaded operator: " << name << " from " << path << std::endl;
    return true;
}

bool CUDABackend::has_operator(const std::string& name) const
{
    return operators_.find(name) != operators_.end();
}

cudaStream_t CUDABackend::get_next_stream()
{
    std::lock_guard<std::mutex> lock(stream_mutex_);
    cudaStream_t stream = streams_[next_stream_];
    next_stream_ = (next_stream_ + 1) % streams_.size();
    return stream;
}

bool CUDABackend::submit(Request& req)
{
    int n = req.input_size;
    if (n <= 0) return false;

    auto op_it = operators_.find(req.operator_name);
    if (op_it == operators_.end()) {
        std::cerr << "Operator not found: " << req.operator_name << std::endl;
        return false;
    }
    OperatorFunc op_func = op_it->second;

    const size_t data_size = n * sizeof(float);
    
    float* d_a = static_cast<float*>(pool_allocate(data_size));
    float* d_b = static_cast<float*>(pool_allocate(data_size));
    float* d_c = static_cast<float*>(pool_allocate(data_size));
    
    if (!d_a || !d_b || !d_c) {
        pool_free(d_a, data_size);
        pool_free(d_b, data_size);
        pool_free(d_c, data_size);
        return false;
    }

    cudaStream_t stream = get_next_stream();

    CHECK_CUDA_RET_FALSE(cudaMemcpyAsync(d_a, req.h_a, data_size, cudaMemcpyHostToDevice, stream));
    CHECK_CUDA_RET_FALSE(cudaMemcpyAsync(d_b, req.h_b, data_size, cudaMemcpyHostToDevice, stream));

    op_func(d_a, d_b, d_c, n);

    CHECK_CUDA_RET_FALSE(cudaMemcpyAsync(req.h_c, d_c, data_size, cudaMemcpyDeviceToHost, stream));

    cudaError_t err = cudaStreamSynchronize(stream);
    if (err != cudaSuccess) {
        std::cerr << "cudaStreamSynchronize failed: " << cudaGetErrorString(err) << std::endl;
        pool_free(d_a, data_size);
        pool_free(d_b, data_size);
        pool_free(d_c, data_size);
        return false;
    }

    pool_free(d_a, data_size);
    pool_free(d_b, data_size);
    pool_free(d_c, data_size);

    return true;
}

std::shared_ptr<CUDABackend::AsyncResult> CUDABackend::submit_async(Request& req)
{
    int n = req.input_size;
    if (n <= 0) return nullptr;

    auto op_it = operators_.find(req.operator_name);
    if (op_it == operators_.end()) {
        std::cerr << "Operator not found: " << req.operator_name << std::endl;
        return nullptr;
    }
    OperatorFunc op_func = op_it->second;

    const size_t data_size = n * sizeof(float);
    
    float* d_a = static_cast<float*>(pool_allocate(data_size));
    float* d_b = static_cast<float*>(pool_allocate(data_size));
    float* d_c = static_cast<float*>(pool_allocate(data_size));
    
    if (!d_a || !d_b || !d_c) {
        pool_free(d_a, data_size);
        pool_free(d_b, data_size);
        pool_free(d_c, data_size);
        return nullptr;
    }

    cudaStream_t stream = get_next_stream();
    
    auto result = std::make_shared<AsyncResult>();
    result->result.resize(n);
    
    // 使用 CHECK_CUDA_RET_NULL 宏（返回nullptr而不是false）
    CHECK_CUDA_RET_NULL(cudaEventCreate(&result->event));
    
    CHECK_CUDA_RET_NULL(cudaMemcpyAsync(d_a, req.h_a, data_size, cudaMemcpyHostToDevice, stream));
    CHECK_CUDA_RET_NULL(cudaMemcpyAsync(d_b, req.h_b, data_size, cudaMemcpyHostToDevice, stream));
    
    op_func(d_a, d_b, d_c, n);
    
    CHECK_CUDA_RET_NULL(cudaMemcpyAsync(result->result.data(), d_c, data_size, cudaMemcpyDeviceToHost, stream));
    CHECK_CUDA_RET_NULL(cudaEventRecord(result->event, stream));

    return result;
}

bool CUDABackend::submit_batch(Batch& batch)
{
    std::cout << "[backend] batch execute size = " << batch.requests.size() << std::endl;

    bool all_success = true;
    for (auto& req : batch.requests) {
        std::cout << "[backend] dispatch req " << req.request_id << std::endl;
        if (!submit(req)) {
            all_success = false;
        }
    }

    return all_success;
}

void CUDABackend::shutdown()
{
    for (auto& stream : streams_) {
        CHECK_CUDA_VOID(cudaStreamDestroy(stream));
    }
    streams_.clear();

    pool_clear();

    for (auto& pair : library_handles_) {
        dlclose(pair.second);
    }
    library_handles_.clear();
    operators_.clear();
}
