#pragma once

#include "harness/skill_harness.h"

namespace examples {

/**
 * 示例 Skill: 向量加法计算
 * 展示如何使用 Harness 框架
 */
class VectorAddSkill : public harness::Skill {
public:
    static constexpr const char* SKILL_ID = "com.cudapipeline.vector_add";
    static constexpr const char* SKILL_NAME = "Vector Add Skill";

    VectorAddSkill() : Skill(SKILL_ID, SKILL_NAME) {
        config_.skill_id = SKILL_ID;
        config_.skill_name = SKILL_NAME;
        config_.memory_quota = 512 * 1024 * 1024;  // 512MB
        config_.max_streams = 2;
        config_.timeout = std::chrono::milliseconds(10000);  // 10s 超时
        config_.priority = 60;
    }

    harness::SkillResult run(harness::vGPUContext& context) override {
        harness::SkillResult result;
        auto start = std::chrono::high_resolution_clock::now();

        log("开始执行向量加法 Skill");

        try {
            const size_t N = 1024 * 1024;  // 1M 元素
            const size_t size_bytes = N * sizeof(float);

            report_progress(0.1f);
            log("分配内存: " + std::to_string(size_bytes) + " bytes");

            // 使用 vGPU 上下文分配内存（带配额检查）
            void* d_a = context.alloc_memory(size_bytes);
            void* d_b = context.alloc_memory(size_bytes);
            void* d_c = context.alloc_memory(size_bytes);

            if (!d_a || !d_b || !d_c) {
                result.success = false;
                result.error_message = "内存分配失败: 超出配额";
                result.status = harness::SkillStatus::FAILED;
                return result;
            }

            report_progress(0.3f);
            log("内存分配成功");

            // ... 这里执行实际的 CUDA 计算 ...

            // 模拟计算
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            report_progress(0.8f);

            // 释放内存
            context.free_memory(d_a, size_bytes);
            context.free_memory(d_b, size_bytes);
            context.free_memory(d_c, size_bytes);

            report_progress(1.0f);

            auto end = std::chrono::high_resolution_clock::now();
            result.execution_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            result.success = true;
            result.status = harness::SkillStatus::COMPLETED;
            // 资源使用统计在返回端填充

            log("执行完成，耗时: " + std::to_string(result.execution_time.count()) + " us");

        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = e.what();
            result.status = harness::SkillStatus::FAILED;
            log("执行失败: " + result.error_message);
        }

        return result;
    }

    // 简化的入口，直接返回 bool
    bool run_simple() {
        try {
            // 模拟计算
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return true;
        } catch (...) {
            return false;
        }
    }
};

} // namespace examples
