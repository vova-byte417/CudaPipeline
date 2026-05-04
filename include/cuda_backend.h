#pragma once

#include "backend.h"
#include "request.h"
#include "runtime/batch.h"
#include <cuda_runtime.h>
#include <string>

class CUDABackend : public Backend
{
public:
    CUDABackend();
    virtual ~CUDABackend();

    bool initialize() override;
    bool submit_batch(Batch& batch) override;
    void shutdown() override;
    bool submit(Request& req) override;

private:
    using operator_func_t = void (*)(float*, float*, float*, int);

    cudaStream_t stream_ = nullptr;
    void* operator_handle_ = nullptr;
    operator_func_t vector_add_ = nullptr;

    bool load_operator(const std::string& path);
};