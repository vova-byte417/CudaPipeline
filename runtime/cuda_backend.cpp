#include "cuda_backend.h"
#include "request.h"
#include "runtime/batch.h"

#include <cuda_runtime.h>
#include <dlfcn.h>
#include <iostream>
#include <vector>
#include <algorithm>

#define CHECK_CUDA_RET_FALSE(x)                           \
do {                                                      \
    cudaError_t err = (x);                                \
    if (err != cudaSuccess) {                             \
        std::cerr << "[CUDA Error] " << __FILE__ << ":"   \
                  << __LINE__ << ": " << cudaGetErrorString(err) << std::endl; \
        return false;                                     \
    }                                                     \
} while(0)

CUDABackend::CUDABackend(int device_id, int num_streams)
    : device_id_(device_id) {
    streams_.resize(num_streams);
}

CUDABackend::~CUDABackend() {
    shutdown();
}

bool CUDABackend::initialize() {
    CHECK_CUDA_RET_FALSE(cudaSetDevice(device_id_));

    for (size_t i = 0; i < streams_.size(); ++i) {
        CHECK_CUDA_RET_FALSE(cudaStreamCreate(&streams_[i]));
    }

    // 加载sgemm算子
    operator_handle_ = dlopen("./build/operators/libsgemm.so", RTLD_NOW);
    if (!operator_handle_) {
        std::cerr << "Failed to load libsgemm.so: " << dlerror() << std::endl;
        return false;
    }

    sgemm_func_ = reinterpret_cast<sgemm_func_t>(dlsym(operator_handle_, "sgemm_tiled"));
    if (!sgemm_func_) {
        std::cerr << "Failed to find sgemm symbol: " << dlerror() << std::endl;
        return false;
    }

    std::cout << "CUDA backend initialized successfully!\n";
    return true;
}

void* CUDABackend::pool_allocate(size_t bytes) {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    size_t aligned_bytes = (bytes + 255) & ~255;
    
    auto it = memory_pool_.find(aligned_bytes);
    if (it != memory_pool_.end() && !it->second.empty()) {
        void* ptr = it->second.back();
        it->second.pop_back();
        return ptr;
    }
    
    void* ptr = nullptr;
    cudaMalloc(&ptr, aligned_bytes);
    return ptr;
}

void CUDABackend::pool_free(void* ptr, size_t bytes) {
    if (!ptr) return;
    std::lock_guard<std::mutex> lock(pool_mutex_);
    size_t aligned_bytes = (bytes + 255) & ~255;
    memory_pool_[aligned_bytes].push_back(ptr);
}

void CUDABackend::pool_clear() {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    for (auto& pair : memory_pool_) {
        for (void* ptr : pair.second) {
            cudaFree(ptr);
        }
    }
    memory_pool_.clear();
}

cudaStream_t CUDABackend::get_next_stream() {
    std::lock_guard<std::mutex> lock(stream_mutex_);
    cudaStream_t stream = streams_[next_stream_];
    next_stream_ = (next_stream_ + 1) % streams_.size();
    return stream;
}

bool CUDABackend::submit(Request& req) {
    int n = req.input_size;
    if (n <= 0) return false;

    const size_t matrix_elements = n * n;
    const size_t data_size = matrix_elements * sizeof(float);
    
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

    // 直接调用sgemm！
    // sgemm_func_(d_a, d_b, d_c, n);
    if (op_impl_) {
    op_impl_->execute(n, d_a, d_b, d_c, streams_[0]);
    } else if (sgemm_func_) {
        sgemm_func_(d_a, d_b, d_c, n);
    }

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

bool CUDABackend::submit_batch(Batch& batch) {
    std::cout << "[backend] batch execute size = " << batch.requests.size() << std::endl;
    for (auto& req : batch.requests) {
        submit(req);
    }
    return true;
}

void CUDABackend::shutdown() {
    for (auto& stream : streams_) {
        cudaStreamDestroy(stream);
    }
    streams_.clear();

    pool_clear();

    if (operator_handle_) {
        dlclose(operator_handle_);
        operator_handle_ = nullptr;
    }
}
