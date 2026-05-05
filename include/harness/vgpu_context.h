#pragma once

#include <string>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <atomic>
#include <mutex>
#include <vector>
#include <chrono>
#include <unordered_map>

namespace harness {

/**
 * vGPU 虚拟上下文
 * 每个 Skill 独占一个 vGPU Context，实现资源隔离
 */
class vGPUContext {
public:
    struct Config {
        std::string skill_id;                    // 所属 Skill ID
        size_t memory_quota_bytes = 1024 * 1024 * 1024;  // 显存配额: 默认 1GB
        int max_streams = 4;                      // 最大 Stream 数量
        int max_kernels = 16;                     // 最大并发 Kernel
        std::chrono::milliseconds kernel_timeout{10000};  // Kernel 超时: 10秒
        bool enable_memory_guard = true;          // 启用内存保护
        bool enable_watchdog = true;              // 启用看门狗
        int priority = 50;                         // 调度优先级 (0-100)
    };

    // 资源使用统计（可拷贝版本，用于快照）
    struct Stats {
        size_t memory_used = 0;
        int active_streams = 0;
        int active_kernels = 0;
        uint64_t total_kernels = 0;
        uint64_t total_memory_alloc = 0;
        uint64_t timeout_count = 0;
    };

private:
    // 内部原子统计，不对外暴露
    struct AtomicStats {
        std::atomic<size_t> memory_used{0};
        std::atomic<int> active_streams{0};
        std::atomic<int> active_kernels{0};
        std::atomic<uint64_t> total_kernels{0};
        std::atomic<uint64_t> total_memory_alloc{0};
        std::atomic<uint64_t> timeout_count{0};

        Stats snapshot() const {
            Stats s;
            s.memory_used = memory_used.load();
            s.active_streams = active_streams.load();
            s.active_kernels = active_kernels.load();
            s.total_kernels = total_kernels.load();
            s.total_memory_alloc = total_memory_alloc.load();
            s.timeout_count = timeout_count.load();
            return s;
        }
    };

    explicit vGPUContext(const Config& config);
    ~vGPUContext();

    // 禁用拷贝
    vGPUContext(const vGPUContext&) = delete;
    vGPUContext& operator=(const vGPUContext&) = delete;

    // 内存分配（带配额检查）
    void* alloc_memory(size_t size);
    void free_memory(void* ptr, size_t size);

    // Stream 管理
    int acquire_stream();
    void release_stream(int stream_id);

    // Kernel 执行（带超时监控）
    template<typename Func>
    bool launch_kernel(Func&& kernel_func, const char* kernel_name = "unknown");

    // 状态检查
    bool is_healthy() const { return healthy_; }
    bool has_memory_overflow() const { return memory_overflow_; }

    // 获取统计快照
    Stats get_stats() const { return stats_.snapshot(); }
    const Config& get_config() const { return config_; }

    // 重置上下文（故障恢复）
    bool reset();

    // 获取 Skill ID
    const std::string& skill_id() const { return config_.skill_id; }

private:
    Config config_;
    AtomicStats stats_;

    std::mutex memory_mutex_;
    std::mutex stream_mutex_;
    std::mutex kernel_mutex_;

    // 内存追踪
    std::unordered_map<void*, size_t> memory_allocations_;

    // Stream 池
    std::vector<bool> stream_available_;
    std::atomic<bool> healthy_{true};
    std::atomic<bool> memory_overflow_{false};

    // 检查配额
    bool check_memory_quota(size_t size) const;
    bool check_kernel_quota() const;

    // 内存保护
    bool setup_memory_guard(void* ptr, size_t size);
    bool verify_memory_guard(void* ptr);
};

/**
 * vGPU 上下文管理器 - 单例
 * 管理所有虚拟上下文的生命周期
 */
class vGPUManager {
public:
    static vGPUManager& instance();

    // 创建/销毁上下文
    std::shared_ptr<vGPUContext> create_context(const vGPUContext::Config& config);
    void destroy_context(const std::string& skill_id);

    // 获取上下文
    std::shared_ptr<vGPUContext> get_context(const std::string& skill_id);

    // 全局统计
    struct GlobalStats {
        size_t total_memory_used = 0;
        int total_active_contexts = 0;
        int total_active_streams = 0;
        int total_active_kernels = 0;
        size_t total_memory_quota = 0;
    };

    GlobalStats get_global_stats() const;

    // 健康检查
    bool is_all_healthy() const;

    // 故障恢复：重置不健康的上下文
    int recover_unhealthy();

private:
    vGPUManager() = default;
    ~vGPUManager() = default;

    std::unordered_map<std::string, std::shared_ptr<vGPUContext>> contexts_;
    mutable std::mutex contexts_mutex_;
};

/**
 * Skill 沙箱执行器
 * 每个 Skill 在独立的沙箱环境中执行，错误不扩散
 */
class SkillSandbox {
public:
    struct Result {
        bool success = false;
        std::string error_message;
        std::chrono::microseconds execution_time;
        vGPUContext::Stats resource_usage;
    };

    explicit SkillSandbox(std::shared_ptr<vGPUContext> context);
    ~SkillSandbox();

    // 在沙箱中执行函数
    template<typename Func>
    Result execute(Func&& func, const char* operation_name = "unknown");

    // 强制终止沙箱
    void terminate();

    // 健康检查
    bool is_alive() const { return alive_; }

private:
    std::shared_ptr<vGPUContext> context_;
    std::atomic<bool> alive_{true};
    std::atomic<bool> running_{false};

    // 异常捕获
    template<typename Func>
    Result safe_execute(Func&& func);
};

} // namespace harness
