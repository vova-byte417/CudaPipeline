#pragma once

#include "backend.h"
#include "request.h"
#include "runtime/batch.h"
#include "memory_pool.h"
#include <string>
#include <vector>
#include <cstring>

class CPUBackend : public Backend
{
public:
    CPUBackend();
    virtual ~CPUBackend();

    bool initialize() override;
    bool submit_batch(Batch& batch) override;
    void shutdown() override;
    bool submit(Request& req) override;

private:
    // CPU 实现的 vector add
    void vector_add_cpu(float* a, float* b, float* c, int n);
    
    // 内存池（系统内存）
    MemoryPool mem_pool_;
};
