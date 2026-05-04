#pragma once

#include <cuda_runtime.h>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <iostream>
#include <algorithm>

class MemoryPool
{
public:
    MemoryPool() = default;
    ~MemoryPool()
    {
        clear();
    }

    // 禁用拷贝
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    // 分配内存：优先从池里取，没有则新分配
    void* alloc(size_t size)
    {
        if (size == 0) return nullptr;

        std::lock_guard<std::mutex> lock(mutex_);

        // 向上取整到最近的 bucket 大小
        size_t bucket_size = get_bucket_size(size);

        // 看看池里有没有可用的
        auto& bucket = pools_[bucket_size];
        if (!bucket.empty()) {
            void* ptr = bucket.back();
            bucket.pop_back();
            return ptr;
        }

        // 池里没有，新分配
        void* ptr = nullptr;
        cudaError_t err = cudaMalloc(&ptr, bucket_size);
        if (err != cudaSuccess) {
            std::cerr << "[MemoryPool] cudaMalloc failed for size " << bucket_size 
                      << ": " << cudaGetErrorString(err) << std::endl;
            return nullptr;
        }

        total_allocated_ += bucket_size;
        stats_.alloc_count++;

        return ptr;
    }

    // 释放内存：归还到池里
    void free(void* ptr, size_t size)
    {
        if (!ptr || size == 0) return;

        std::lock_guard<std::mutex> lock(mutex_);

        size_t bucket_size = get_bucket_size(size);
        pools_[bucket_size].push_back(ptr);
        stats_.free_count++;
    }

    // 清空池，释放所有内存
    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        for (auto& [size, bucket] : pools_) {
            for (void* ptr : bucket) {
                cudaFree(ptr);
                total_allocated_ -= size;
            }
        }
        pools_.clear();
    }

    // 打印统计信息
    void print_stats()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        std::cout << "\n=== Memory Pool Stats ===\n";
        std::cout << "Total allocated: " << total_allocated_ / 1024 << " KB\n";
        std::cout << "Alloc calls: " << stats_.alloc_count << "\n";
        std::cout << "Free calls: " << stats_.free_count << "\n";
        std::cout << "Pooled blocks:\n";
        for (const auto& [size, bucket] : pools_) {
            if (!bucket.empty()) {
                std::cout << "  Size " << size << " B: " << bucket.size() << " blocks\n";
            }
        }
        std::cout << "=========================\n\n";
    }

private:
    // bucket 大小：2 的幂次
    static constexpr size_t BUCKET_SIZES[] = {
        64, 256, 1024, 4096, 16384, 65536, 262144, 
        1048576, 4194304, 16777216, 67108864
    };
    static constexpr int NUM_BUCKETS = sizeof(BUCKET_SIZES) / sizeof(BUCKET_SIZES[0]);

    // 向上取整到最近的 bucket 大小
    size_t get_bucket_size(size_t size)
    {
        for (int i = 0; i < NUM_BUCKETS; i++) {
            if (BUCKET_SIZES[i] >= size) {
                return BUCKET_SIZES[i];
            }
        }
        // 超过最大 bucket，按实际大小对齐到 256B
        return (size + 255) & ~255;
    }

    std::mutex mutex_;
    std::unordered_map<size_t, std::vector<void*>> pools_;
    size_t total_allocated_ = 0;

    struct {
        size_t alloc_count = 0;
        size_t free_count = 0;
    } stats_;
};
