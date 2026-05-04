#pragma once
#include <cuda_runtime.h>
#include <dlfcn.h>
#include "cuda_backend.h"

class SGEMMOperator : public IOperator {
public:
    SGEMMOperator() {
        // 动态加载算子（和原来的vector_add加载方式一样）
        void* handle = dlopen("./build/operators/libsgemm.so", RTLD_NOW);
        if (handle) {
            sgemm_func_ = reinterpret_cast<decltype(sgemm_func_)>(dlsym(handle, "sgemm"));
        }
    }
    
    bool execute(int n, float* A, float* B, float* C, cudaStream_t stream) override {
        if (sgemm_func_) {
            sgemm_func_(n, A, B, C, stream);
            return cudaGetLastError() == cudaSuccess;
        }
        return false;
    }

private:
    using sgemm_func_t = void (*)(int, float*, float*, float*, cudaStream_t);
    sgemm_func_t sgemm_func_ = nullptr;
};
