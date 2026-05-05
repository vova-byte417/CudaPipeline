#pragma once

#include <string>
#include <cstddef>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include <vector>
#include <functional>
#include <thread>

namespace harness {

/**
 * 资源类型枚举
 */
enum class ResourceType {
    GPU_MEMORY,      // GPU 显存
    CPU_MEMORY,      // CPU 内存
    CUDA_STREAMS,    // CUDA 流
    CUDA_KERNELS,    // CUDA 内核并发数
    THREADS,         // 线程数
    EXECUTION_TIME,  // 执行时间
    IO_BANDWIDTH,    // IO 带宽
};

/**
 * 资源配额配置
 */
struct ResourceQuota {
    // 硬限制：超过直接拒绝
    size_t hard_limit = 0;

    // 软限制：超过警告但允许
    size_t soft_limit = 0;

    // 是否启用
    bool enabled = false;

    ResourceQuota() = default;
    ResourceQuota(size_t hard, size_t soft = 0)
        : hard_limit(hard), soft_limit(soft > 0 ? soft : hard * 0.8), enabled(true) {}
};

/**
 * 资源使用统计快照（可拷贝）
 */
struct ResourceUsageSnapshot {
    size_t current = 0;
    size_t peak = 0;
    uint64_t total_allocated = 0;
    uint64_t soft_limit_violations = 0;
    uint64_t hard_limit_violations = 0;
};

/**
 * 资源使用状态（内部原子版本）
 */
class ResourceUsage {
public:
    void record_alloc(size_t size) {
        size_t cur = current.fetch_add(size) + size;
        size_t old_peak = peak.load();
        while (cur > old_peak && !peak.compare_exchange_weak(old_peak, cur)) {
            // 重试直到成功
        }
        total_allocated += size;
    }

    void record_free(size_t size) {
        current.fetch_sub(size);
    }

    bool would_exceed_hard(size_t size, size_t limit) const {
        return current.load() + size > limit;
    }

    bool would_exceed_soft(size_t size, size_t limit) const {
        return current.load() + size > limit;
    }

    // 获取快照（可安全拷贝）
    ResourceUsageSnapshot snapshot() const {
        ResourceUsageSnapshot s;
        s.current = current.load();
        s.peak = peak.load();
        s.total_allocated = total_allocated.load();
        s.soft_limit_violations = soft_limit_violations.load();
        s.hard_limit_violations = hard_limit_violations.load();
        return s;
    }

    std::atomic<size_t> current{0};
    std::atomic<size_t> peak{0};
    std::atomic<uint64_t> total_allocated{0};
    std::atomic<uint64_t> soft_limit_violations{0};
    std::atomic<uint64_t> hard_limit_violations{0};
};

/**
 * 资源配额管理器
 * 实现 Skill 级别的资源隔离和限流
 */
class ResourceQuotaManager {
public:
    static ResourceQuotaManager& instance();

    // 为 Skill 设置配额
    void set_quota(const std::string& skill_id, ResourceType type, const ResourceQuota& quota);

    // 获取 Skill 的配额
    ResourceQuota get_quota(const std::string& skill_id, ResourceType type) const;

    // 尝试分配资源
    // 返回: true=成功，false=超出配额
    bool try_allocate(const std::string& skill_id, ResourceType type, size_t amount);

    // 释放资源
    void release(const std::string& skill_id, ResourceType type, size_t amount);

    // 获取 Skill 资源使用情况
    ResourceUsageSnapshot get_usage(const std::string& skill_id, ResourceType type) const;

    // 检查是否有足够资源
    bool has_available(const std::string& skill_id, ResourceType type, size_t amount) const;

    // 全局资源统计
    struct GlobalSummary {
        size_t total_allocated = 0;
        size_t total_quota = 0;
        size_t active_skills = 0;
        size_t limit_violations = 0;
    };

    GlobalSummary get_global_summary(ResourceType type) const;

    // 超限回调
    using ViolationCallback = std::function<void(const std::string& skill_id, ResourceType type, size_t requested, size_t limit)>;

    void set_soft_limit_callback(ViolationCallback cb);
    void set_hard_limit_callback(ViolationCallback cb);

    // 重置所有统计
    void reset_stats(const std::string& skill_id);
    void reset_all_stats();

    // 移除 Skill 配额
    void remove_skill(const std::string& skill_id);

private:
    ResourceQuotaManager() = default;
    ~ResourceQuotaManager() = default;

    // 确保 Skill 有使用记录
    void ensure_skill_exists(const std::string& skill_id);

    mutable std::mutex mutex_;

    // Skill -> (ResourceType -> Usage)
    using UsageMap = std::unordered_map<ResourceType, ResourceUsage>;
    std::unordered_map<std::string, UsageMap> usage_;

    // Skill -> (ResourceType -> Quota)
    using QuotaMap = std::unordered_map<ResourceType, ResourceQuota>;
    std::unordered_map<std::string, QuotaMap> quotas_;

    // 超限回调
    ViolationCallback soft_limit_callback_;
    ViolationCallback hard_limit_callback_;
};

/**
 * RAII 资源分配器
 * 自动管理资源的分配和释放
 */
class ResourceAllocation {
public:
    ResourceAllocation(std::string skill_id, ResourceType type, size_t amount)
        : skill_id_(std::move(skill_id)), type_(type), amount_(amount) {

        auto& mgr = ResourceQuotaManager::instance();
        allocated_ = mgr.try_allocate(skill_id_, type_, amount_);
    }

    ~ResourceAllocation() {
        if (allocated_) {
            auto& mgr = ResourceQuotaManager::instance();
            mgr.release(skill_id_, type_, amount_);
        }
    }

    // 禁用拷贝
    ResourceAllocation(const ResourceAllocation&) = delete;
    ResourceAllocation& operator=(const ResourceAllocation&) = delete;

    // 允许移动
    ResourceAllocation(ResourceAllocation&& other) noexcept
        : skill_id_(std::move(other.skill_id_)),
          type_(other.type_),
          amount_(other.amount_),
          allocated_(other.allocated_) {
        other.allocated_ = false;
    }

    bool is_allocated() const { return allocated_; }
    operator bool() const { return allocated_; }

private:
    std::string skill_id_;
    ResourceType type_;
    size_t amount_;
    bool allocated_ = false;
};

/**
 * 限流令牌桶
 * 用于控制 Skill 的执行频率
 */
class TokenBucket {
public:
    TokenBucket(size_t rate, size_t burst_size)
        : rate_(rate), burst_size_(burst_size), tokens_(burst_size) {
        last_refill_ = std::chrono::steady_clock::now();
    }

    // 尝试获取 token
    bool try_consume(size_t tokens = 1) {
        refill();

        std::lock_guard<std::mutex> lock(mutex_);
        if (tokens_ >= tokens) {
            tokens_ -= tokens;
            return true;
        }
        return false;
    }

    // 阻塞等待 token
    template<typename Rep, typename Period>
    bool consume(size_t tokens = 1, const std::chrono::duration<Rep, Period>& timeout = std::chrono::milliseconds(100)) {
        auto start = std::chrono::steady_clock::now();
        while (!try_consume(tokens)) {
            if (std::chrono::steady_clock::now() - start > timeout) {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        return true;
    }

    // 获取当前可用 token 数
    size_t available() {
        refill();
        std::lock_guard<std::mutex> lock(mutex_);
        return tokens_;
    }

    // 动态调整速率
    void set_rate(size_t new_rate) {
        std::lock_guard<std::mutex> lock(mutex_);
        rate_ = new_rate;
        burst_size_ = new_rate; // 简单策略：突发 = 速率
    }

private:
    void refill() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_refill_).count();

        if (elapsed_ms > 0) {
            std::lock_guard<std::mutex> lock(mutex_);
            size_t new_tokens = (rate_ * elapsed_ms) / 1000;
            tokens_ = std::min(tokens_ + new_tokens, burst_size_);
            last_refill_ = now;
        }
    }

    size_t rate_;           // 每秒生成的 token 数
    size_t burst_size_;     // 最大突发量
    size_t tokens_;         // 当前可用 token
    std::chrono::steady_clock::time_point last_refill_;
    std::mutex mutex_;
};

} // namespace harness
