#include "../test_common.h"
#include "harness/resource_quota.h"

using namespace harness;
using namespace test;

// ========== ResourceQuota 基本测试 ==========

TEST_CATEGORY(ResourceQuota, Basic) {
    TestResult result;
    result.passed = true;

    ResourceQuota quota(100 * 1024 * 1024, 80 * 1024 * 1024);
    TEST_ASSERT(quota.enabled == true, "配额应该启用");
    TEST_ASSERT_EQ(quota.hard_limit, static_cast<size_t>(100 * 1024 * 1024), "硬限制设置错误");
    TEST_ASSERT_EQ(quota.soft_limit, static_cast<size_t>(80 * 1024 * 1024), "软限制设置错误");

    return result;
}

TEST_CATEGORY(ResourceQuota, DefaultConstructor) {
    TestResult result;
    result.passed = true;

    ResourceQuota quota;
    TEST_ASSERT(quota.enabled == false, "默认配额应该未启用");
    TEST_ASSERT_EQ(quota.hard_limit, static_cast<size_t>(0), "默认硬限制应该为0");

    return result;
}

// ========== ResourceUsage 统计测试 ==========

TEST_CATEGORY(ResourceUsage, AllocFreeTracking) {
    TestResult result;
    result.passed = true;

    ResourceUsage usage;
    usage.record_alloc(1024);
    TEST_ASSERT_EQ(usage.current.load(), static_cast<size_t>(1024), "current 统计错误");
    TEST_ASSERT_EQ(usage.total_allocated.load(), static_cast<uint64_t>(1024), "total_allocated 统计错误");

    usage.record_free(512);
    TEST_ASSERT_EQ(usage.current.load(), static_cast<size_t>(512), "current 释放后统计错误");

    auto snapshot = usage.snapshot();
    TEST_ASSERT_EQ(snapshot.current, static_cast<size_t>(512), "snapshot current 错误");
    TEST_ASSERT_EQ(snapshot.total_allocated, static_cast<uint64_t>(1024), "snapshot total_allocated 错误");

    return result;
}

TEST_CATEGORY(ResourceUsage, PeakTracking) {
    TestResult result;
    result.passed = true;

    ResourceUsage usage;

    usage.record_alloc(1000);  // peak = 1000
    usage.record_alloc(500);   // peak = 1500
    usage.record_free(800);    // current = 700, peak 保持 1500
    usage.record_alloc(300);   // current = 1000, peak 仍为 1500

    auto snapshot = usage.snapshot();
    TEST_ASSERT_EQ(snapshot.peak, static_cast<size_t>(1500), "峰值统计错误");
    TEST_ASSERT_EQ(snapshot.current, static_cast<size_t>(1000), "当前值错误");

    return result;
}

// ========== ResourceQuotaManager 单例测试 ==========

TEST_CATEGORY(QuotaManager, Singleton) {
    TestResult result;
    result.passed = true;

    auto& mgr1 = ResourceQuotaManager::instance();
    auto& mgr2 = ResourceQuotaManager::instance();
    TEST_ASSERT(&mgr1 == &mgr2, "单例模式应该返回相同的实例");

    return result;
}

// ========== ResourceQuotaManager 配额设置测试 ==========

TEST_CATEGORY(QuotaManager, SetAndGetQuota) {
    TestResult result;
    result.passed = true;

    auto& mgr = ResourceQuotaManager::instance();
    mgr.set_quota("test-skill-1", ResourceType::GPU_MEMORY, ResourceQuota(1024 * 1024));

    auto quota = mgr.get_quota("test-skill-1", ResourceType::GPU_MEMORY);
    TEST_ASSERT(quota.enabled == true, "配额应该启用");
    TEST_ASSERT_EQ(quota.hard_limit, static_cast<size_t>(1024 * 1024), "获取的硬限制错误");

    return result;
}

// ========== ResourceQuotaManager 分配/释放测试 ==========

TEST_CATEGORY(QuotaManager, AllocateWithinLimit) {
    TestResult result;
    result.passed = true;

    auto& mgr = ResourceQuotaManager::instance();
    const std::string skill_id = "test-skill-within";
    mgr.reset_stats(skill_id);
    mgr.set_quota(skill_id, ResourceType::GPU_MEMORY, ResourceQuota(1024 * 1024));

    bool success = mgr.try_allocate(skill_id, ResourceType::GPU_MEMORY, 512 * 1024);
    TEST_ASSERT(success == true, "限额内的分配应该成功");

    auto usage = mgr.get_usage(skill_id, ResourceType::GPU_MEMORY);
    TEST_ASSERT_EQ(usage.current, static_cast<size_t>(512 * 1024), "分配后使用量错误");

    mgr.release(skill_id, ResourceType::GPU_MEMORY, 512 * 1024);
    auto usage_after = mgr.get_usage(skill_id, ResourceType::GPU_MEMORY);
    TEST_ASSERT_EQ(usage_after.current, static_cast<size_t>(0), "释放后使用量应该为0");

    return result;
}

TEST_CATEGORY(QuotaManager, ExceedHardLimitRejected) {
    TestResult result;
    result.passed = true;

    auto& mgr = ResourceQuotaManager::instance();
    const std::string skill_id = "test-skill-exceed";
    mgr.reset_stats(skill_id);
    mgr.set_quota(skill_id, ResourceType::GPU_MEMORY, ResourceQuota(1024));

    // 第一次分配应该成功
    bool success1 = mgr.try_allocate(skill_id, ResourceType::GPU_MEMORY, 800);
    TEST_ASSERT(success1 == true, "限额内的分配应该成功");

    // 超出硬限制，应该失败
    bool success2 = mgr.try_allocate(skill_id, ResourceType::GPU_MEMORY, 300);
    TEST_ASSERT(success2 == false, "超出硬限制的分配应该失败");

    auto usage = mgr.get_usage(skill_id, ResourceType::GPU_MEMORY);
    TEST_ASSERT_EQ(usage.hard_limit_violations, static_cast<uint64_t>(1), "超限次数统计错误");

    return result;
}

TEST_CATEGORY(QuotaManager, NoQuotaUnlimited) {
    TestResult result;
    result.passed = true;

    auto& mgr = ResourceQuotaManager::instance();
    const std::string skill_id = "test-skill-no-quota";
    mgr.reset_stats(skill_id);
    // 注意：不设置配额 = 无限制

    bool success = mgr.try_allocate(skill_id, ResourceType::GPU_MEMORY, 1024 * 1024 * 1024);  // 1GB
    TEST_ASSERT(success == true, "无配额时分配应该总是成功");

    return result;
}

// ========== ResourceQuotaManager has_available 测试 ==========

TEST_CATEGORY(QuotaManager, HasAvailable) {
    TestResult result;
    result.passed = true;

    auto& mgr = ResourceQuotaManager::instance();
    const std::string skill_id = "test-available";
    mgr.reset_stats(skill_id);
    mgr.set_quota(skill_id, ResourceType::GPU_MEMORY, ResourceQuota(1024));

    mgr.try_allocate(skill_id, ResourceType::GPU_MEMORY, 500);

    TEST_ASSERT(mgr.has_available(skill_id, ResourceType::GPU_MEMORY, 500) == true, "剩余空间足够时应该返回true");
    TEST_ASSERT(mgr.has_available(skill_id, ResourceType::GPU_MEMORY, 600) == false, "剩余空间不足时应该返回false");

    return result;
}

// ========== ResourceQuotaManager 全局统计测试 ==========

TEST_CATEGORY(QuotaManager, GlobalSummary) {
    TestResult result;
    result.passed = true;

    auto& mgr = ResourceQuotaManager::instance();
    mgr.reset_all_stats();

    mgr.set_quota("skill-gs-1", ResourceType::GPU_MEMORY, ResourceQuota(1024));
    mgr.set_quota("skill-gs-2", ResourceType::GPU_MEMORY, ResourceQuota(2048));

    mgr.try_allocate("skill-gs-1", ResourceType::GPU_MEMORY, 512);
    mgr.try_allocate("skill-gs-2", ResourceType::GPU_MEMORY, 1024);

    auto summary = mgr.get_global_summary(ResourceType::GPU_MEMORY);
    TEST_ASSERT_EQ(summary.active_skills, static_cast<size_t>(2), "活跃Skill数量错误");
    TEST_ASSERT_EQ(summary.total_allocated, static_cast<size_t>(512 + 1024), "总分配量错误");

    return result;
}

// ========== ResourceAllocation RAII 测试 ==========

TEST_CATEGORY(ResourceAllocation, AutoRelease) {
    TestResult result;
    result.passed = true;

    auto& mgr = ResourceQuotaManager::instance();
    const std::string skill_id = "test-raii";
    mgr.reset_stats(skill_id);
    mgr.set_quota(skill_id, ResourceType::GPU_MEMORY, ResourceQuota(1024));

    {
        ResourceAllocation alloc(skill_id, ResourceType::GPU_MEMORY, 512);
        TEST_ASSERT(alloc.is_allocated() == true, "RAII 分配应该成功");
        TEST_ASSERT((bool)alloc == true, "operator bool 应该返回true");

        auto usage = mgr.get_usage(skill_id, ResourceType::GPU_MEMORY);
        TEST_ASSERT_EQ(usage.current, static_cast<size_t>(512), "RAII 分配后使用量错误");
    }

    // 离开作用域后应该自动释放
    auto usage_after = mgr.get_usage(skill_id, ResourceType::GPU_MEMORY);
    TEST_ASSERT_EQ(usage_after.current, static_cast<size_t>(0), "RAII 应该自动释放资源");

    return result;
}

// ========== TokenBucket 测试 ==========

TEST_CATEGORY(TokenBucket, InitialBurst) {
    TestResult result;
    result.passed = true;

    TokenBucket bucket(100, 100);  // 100/s, burst 100

    int success = 0;
    for (int i = 0; i < 150; i++) {
        if (bucket.try_consume()) success++;
    }

    TEST_ASSERT_EQ(success, 100, "初始突发应该正好消耗所有token");
    TEST_ASSERT_EQ(bucket.available(), static_cast<size_t>(0), "bucket 应该为空");

    return result;
}

TEST_CATEGORY(TokenBucket, RefillOverTime) {
    TestResult result;
    result.passed = true;

    TokenBucket bucket(100, 200);  // 100/s

    // 清空 bucket
    for (int i = 0; i < 300; i++) {
        bucket.try_consume();
    }

    TEST_ASSERT_EQ(bucket.available(), static_cast<size_t>(0), "初始应该为空");

    // 等待 200ms，应该补充 20 个 token
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    size_t available = bucket.available();
    TEST_ASSERT(available >= 15 && available <= 25, "200ms 后应该有约20个token (实际: " + std::to_string(available) + ")");

    return result;
}

TEST_CATEGORY(TokenBucket, DynamicRate) {
    TestResult result;
    result.passed = true;

    TokenBucket bucket(100, 200);

    // 清空
    for (int i = 0; i < 300; i++) {
        bucket.try_consume();
    }

    // 改变速率为 200/s
    bucket.set_rate(200);

    // 等待 100ms
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    size_t available = bucket.available();
    TEST_ASSERT(available >= 15 && available <= 30, "100ms 后应该有约20个token (实际: " + std::to_string(available) + ")");

    return result;
}

TEST_CATEGORY(TokenBucket, BlockingConsume) {
    TestResult result;
    result.passed = true;

    TokenBucket bucket(1000, 100);  // 1000/s = 1/ms

    // 清空
    for (int i = 0; i < 150; i++) {
        bucket.try_consume();
    }

    // 等待 2ms 确保有 token 补充
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    // 现在应该能获取到 token
    bool success = bucket.try_consume();
    TEST_ASSERT(success == true, "等待后应该能获取到token");

    return result;
}
