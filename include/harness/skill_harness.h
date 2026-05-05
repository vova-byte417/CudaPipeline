#pragma once

#include "vgpu_context.h"
#include "resource_quota.h"

#include <string>
#include <functional>
#include <memory>
#include <future>
#include <atomic>
#include <chrono>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <any>

namespace harness {

/**
 * Skill 执行状态
 */
enum class SkillStatus {
    IDLE,           // 空闲
    RUNNING,        // 运行中
    PAUSED,         // 已暂停
    COMPLETED,      // 已完成
    FAILED,         // 失败
    TIMEOUT,        // 超时
    KILLED,         // 被强制终止
};

/**
 * Skill 执行结果
 */
struct SkillResult {
    SkillStatus status = SkillStatus::IDLE;
    bool success = false;
    std::string error_message;
    std::chrono::microseconds execution_time{0};
    vGPUContext::Stats resource_usage;
    std::any output_data;
    int exit_code = 0;
};

/**
 * Skill 执行配置
 */
struct SkillConfig {
    std::string skill_id;
    std::string skill_name;
    std::string version = "1.0.0";

    // 执行配置
    std::chrono::milliseconds timeout{30000};  // 默认 30s 超时
    int max_retries = 3;                         // 最大重试次数
    bool enable_checkpoint = false;              // 启用检查点
    bool enable_sandbox = true;                  // 启用沙箱

    // 资源配额
    size_t memory_quota = 1024 * 1024 * 1024;    // 1GB 显存
    int max_streams = 4;
    int max_concurrent_kernels = 16;

    // 调度优先级
    int priority = 50;                            // 0-100

    // 监控回调
    std::function<void(const std::string&, const std::string&)> on_log;
    std::function<void(const std::string&, float)> on_progress;
    std::function<void(const SkillResult&)> on_complete;
};

/**
 * Skill 任务
 */
struct SkillTask {
    std::string task_id;
    SkillConfig config;
    std::function<SkillResult(vGPUContext&)> entry_point;

    // 任务状态
    std::atomic<SkillStatus> status{SkillStatus::IDLE};
    std::promise<SkillResult> result_promise;
    std::future<SkillResult> result_future;

    SkillTask() : result_future(result_promise.get_future()) {}
};

/**
 * Skill Harness - 主执行框架
 * 实现：
 *   - Skill 级别的资源隔离
 *   - 超时和重试机制
 *   - 检查点和恢复
 *   - 监控和统计
 */
class SkillHarness {
public:
    static SkillHarness& instance();

    // 注册 Skill
    bool register_skill(const std::string& skill_id, const SkillConfig& config);

    // 卸载 Skill
    void unregister_skill(const std::string& skill_id);

    // 提交任务
    std::future<SkillResult> submit_task(
        const std::string& skill_id,
        std::function<SkillResult(vGPUContext&)> entry_point
    );

    // 同步执行
    SkillResult execute_sync(
        const std::string& skill_id,
        std::function<SkillResult(vGPUContext&)> entry_point
    );

    // 暂停/恢复 Skill
    bool pause_skill(const std::string& skill_id);
    bool resume_skill(const std::string& skill_id);

    // 强制终止 Skill
    bool kill_skill(const std::string& skill_id);

    // 获取 Skill 状态
    SkillStatus get_skill_status(const std::string& skill_id) const;

    // 获取统计
    struct HarnessStats {
        int total_skills_registered = 0;
        int total_tasks_submitted = 0;
        int total_tasks_completed = 0;
        int total_tasks_failed = 0;
        int total_tasks_timeout = 0;
        size_t total_memory_used = 0;
        int running_tasks = 0;
        int queued_tasks = 0;
    };

    HarnessStats get_stats() const;

    // 启动/停止 Harness
    void start(int worker_threads = 4);
    void stop();

    // 健康检查
    bool is_healthy() const;

    // 生成完整的监控报告
    std::string generate_report() const;

private:
    SkillHarness();
    ~SkillHarness();

    // 工作线程
    void worker_loop();

    // 执行任务
    SkillResult execute_task(std::shared_ptr<SkillTask> task);

    // 带超时的安全执行
    SkillResult execute_with_timeout(
        std::shared_ptr<SkillTask> task,
        vGPUContext& context
    );

    // 重试逻辑
    SkillResult execute_with_retry(
        std::shared_ptr<SkillTask> task,
        vGPUContext& context
    );

    // 故障恢复
    bool recover_skill(const std::string& skill_id);

    // 运行状态
    std::atomic<bool> running_{false};
    std::vector<std::thread> workers_;

    // 任务队列
    std::queue<std::shared_ptr<SkillTask>> task_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    // Skill 注册表
    struct RegisteredSkill {
        SkillConfig config;
        std::shared_ptr<vGPUContext> context;
        std::atomic<SkillStatus> status{SkillStatus::IDLE};
    };

    std::unordered_map<std::string, std::shared_ptr<RegisteredSkill>> skills_;
    mutable std::mutex skills_mutex_;

    // 统计
    mutable std::atomic<int> total_skills_{0};
    mutable std::atomic<int> total_tasks_{0};
    mutable std::atomic<int> completed_tasks_{0};
    mutable std::atomic<int> failed_tasks_{0};
    mutable std::atomic<int> timeout_tasks_{0};
};

/**
 * Skill 基类
 * 用户自定义 Skill 继承此类
 */
class Skill {
public:
    explicit Skill(std::string id, std::string name)
        : id_(std::move(id)), name_(std::move(name)) {}

    virtual ~Skill() = default;

    // Skill 主入口
    virtual SkillResult run(vGPUContext& context) = 0;

    // 暂停回调
    virtual void on_pause() {}

    // 恢复回调
    virtual void on_resume() {}

    // 终止回调
    virtual void on_kill() {}

    // 获取配置
    const SkillConfig& get_config() const { return config_; }
    const std::string& id() const { return id_; }
    const std::string& name() const { return name_; }

protected:
    std::string id_;
    std::string name_;
    SkillConfig config_;

    // 日志工具
    void log(const std::string& message) {
        if (config_.on_log) {
            config_.on_log(id_, message);
        }
    }

    // 进度报告
    void report_progress(float percent) {
        if (config_.on_progress) {
            config_.on_progress(id_, percent);
        }
    }
};

/**
 * Skill 注册宏
 */
#define REGISTER_SKILL(SkillClass) \
    namespace { \
        bool _registered_##SkillClass = []() { \
            SkillHarness::instance().register_skill( \
                SkillClass::SKILL_ID, \
                SkillClass().get_config() \
            ); \
            return true; \
        }(); \
    }

} // namespace harness
