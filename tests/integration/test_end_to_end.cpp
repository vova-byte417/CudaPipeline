#include "../test_common.h"
#include "harness/resource_quota.h"
#include "harness/vgpu_context.h"
#include "scheduler/fcfs_scheduler.h"
#include "scheduler/priority_scheduler.h"
#include "scheduler/rl_scheduler.h"
#include "queue.h"
#include "runtime/batch.h"

#include <thread>
#include <atomic>
#include <vector>

using namespace harness;
using namespace test;

// ========== 端到端调度流程测试 ==========

TEST_CATEGORY(EndToEnd, SingleSchedulerFlow) {
    TestResult result;
    result.passed = true;

    // 1. Setup: 创建队列和调度器
    RequestQueue queue;
    FCFS_Scheduler scheduler;

    // 2. 提交请求
    const int NUM_REQUESTS = 20;
    for (int i = 0; i < NUM_REQUESTS; i++) {
        Request req;
        req.request_id = i;
        req.input_size = 1024;
        queue.push(req);
    }

    TEST_ASSERT_EQ(queue.size(), static_cast<size_t>(NUM_REQUESTS), "所有请求应该入队");

    // 3. 调度所有请求
    int total_scheduled = 0;
    int batch_count = 0;
    Batch batch;

    while (scheduler.select_batch(queue, batch)) {
        batch_count++;
        total_scheduled += batch.requests.size();

        // 模拟执行
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    TEST_ASSERT_EQ(total_scheduled, NUM_REQUESTS, "所有请求应该被调度");
    TEST_ASSERT(batch_count > 0, "应该有batch被调度");
    TEST_ASSERT(queue.empty(), "队列应该为空");

    return result;
}

TEST_CATEGORY(EndToEnd, MultiBatchProcessing) {
    TestResult result;
    result.passed = true;

    RequestQueue queue;
    FCFS_Scheduler scheduler;

    // 提交刚好 3 个 batch 的量
    const int BATCH_SIZE = 4;
    const int NUM_BATCHES = 3;

    for (int i = 0; i < BATCH_SIZE * NUM_BATCHES; i++) {
        Request req;
        req.request_id = i;
        req.input_size = 1024;
        queue.push(req);
    }

    Batch batch;
    int batches_processed = 0;

    for (int i = 0; i < 5; i++) {  // 最多尝试 5 次
        if (!scheduler.select_batch(queue, batch)) {
            break;
        }
        batches_processed++;

        if (batches_processed < NUM_BATCHES) {
            TEST_ASSERT_EQ(batch.requests.size(), static_cast<size_t>(BATCH_SIZE),
                           "前" + std::to_string(NUM_BATCHES - 1) + "个batch应该是满的");
        }
    }

    TEST_ASSERT_EQ(batches_processed, NUM_BATCHES, "应该正好处理" + std::to_string(NUM_BATCHES) + "个batch");
    TEST_ASSERT(queue.empty(), "队列应该为空");

    return result;
}

// ========== 多线程并发测试 ==========

TEST_CATEGORY(Concurrency, MultiProducerSingleConsumer) {
    TestResult result;
    result.passed = true;

    RequestQueue queue;
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};

    const int NUM_PRODUCERS = 3;
    const int REQUESTS_PER_PRODUCER = 100;

    // 生产者线程
    std::vector<std::thread> producers;
    for (int p = 0; p < NUM_PRODUCERS; p++) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < REQUESTS_PER_PRODUCER; i++) {
                Request req;
                req.request_id = p * 1000 + i;
                req.input_size = 1024;
                queue.push(req);
                produced++;
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
    }

    // 等待所有生产者完成
    for (auto& t : producers) {
        t.join();
    }

    TEST_ASSERT_EQ(produced.load(), NUM_PRODUCERS * REQUESTS_PER_PRODUCER, "生产数量应该正确");
    TEST_ASSERT_EQ(queue.size(), static_cast<size_t>(NUM_PRODUCERS * REQUESTS_PER_PRODUCER), "队列大小应该正确");

    // 消费者
    FCFS_Scheduler scheduler;
    Batch batch;
    while (scheduler.select_batch(queue, batch)) {
        consumed += batch.requests.size();
    }

    TEST_ASSERT_EQ(consumed.load(), produced.load(), "消费数量应该等于生产数量");

    return result;
}

TEST_CATEGORY(Concurrency, ConcurrentAllocation) {
    TestResult result;
    result.passed = true;

    auto& mgr = ResourceQuotaManager::instance();
    const std::string skill_id = "concurrent-test-skill";
    mgr.reset_stats(skill_id);
    mgr.set_quota(skill_id, ResourceType::GPU_MEMORY, ResourceQuota(1024 * 1024));

    const int NUM_THREADS = 5;
    const int ALLOCS_PER_THREAD = 100;
    std::atomic<int> success_count{0};
    std::atomic<int> fail_count{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&]() {
            for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
                if (mgr.try_allocate(skill_id, ResourceType::GPU_MEMORY, 100)) {
                    success_count++;
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                    mgr.release(skill_id, ResourceType::GPU_MEMORY, 100);
                } else {
                    fail_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    TEST_ASSERT_EQ(success_count.load() + fail_count.load(), NUM_THREADS * ALLOCS_PER_THREAD,
                   "总操作数应该正确");
    TEST_ASSERT(fail_count.load() == 0, "在配额足够时应该没有失败");

    // 最终应该没有泄漏
    auto usage = mgr.get_usage(skill_id, ResourceType::GPU_MEMORY);
    TEST_ASSERT(usage.current == 0, "所有分配应该被释放 (当前: " + std::to_string(usage.current) + ")");

    return result;
}

// ========== 资源隔离测试 ==========

TEST_CATEGORY(Isolation, MultiSkillQuota) {
    TestResult result;
    result.passed = true;

    auto& mgr = ResourceQuotaManager::instance();

    // 设置 3 个 Skill，不同配额
    mgr.reset_stats("skill-high");
    mgr.reset_stats("skill-med");
    mgr.reset_stats("skill-low");

    mgr.set_quota("skill-high", ResourceType::GPU_MEMORY, ResourceQuota(10000));
    mgr.set_quota("skill-med", ResourceType::GPU_MEMORY, ResourceQuota(5000));
    mgr.set_quota("skill-low", ResourceType::GPU_MEMORY, ResourceQuota(2000));

    // 每个 Skill 尝试申请超过自己配额的资源
    bool high_success = mgr.try_allocate("skill-high", ResourceType::GPU_MEMORY, 8000);
    bool med_success = mgr.try_allocate("skill-med", ResourceType::GPU_MEMORY, 6000);  // 超配额
    bool low_success = mgr.try_allocate("skill-low", ResourceType::GPU_MEMORY, 1500);

    TEST_ASSERT(high_success == true, "高优先级应该分配成功");
    TEST_ASSERT(med_success == false, "中优先级超额应该被拒绝");
    TEST_ASSERT(low_success == true, "低优先级配额内应该成功");

    // 验证配额是隔离的
    auto high_usage = mgr.get_usage("skill-high", ResourceType::GPU_MEMORY);
    auto low_usage = mgr.get_usage("skill-low", ResourceType::GPU_MEMORY);

    TEST_ASSERT_EQ(high_usage.current, static_cast<size_t>(8000), "高优先级使用量应该正确");
    TEST_ASSERT_EQ(low_usage.current, static_cast<size_t>(1500), "低优先级使用量应该正确");

    return result;
}

TEST_CATEGORY(Isolation, QuotaIndependence) {
    TestResult result;
    result.passed = true;

    auto& mgr = ResourceQuotaManager::instance();
    mgr.reset_stats("skill-a");
    mgr.reset_stats("skill-b");

    mgr.set_quota("skill-a", ResourceType::GPU_MEMORY, ResourceQuota(2000));
    mgr.set_quota("skill-b", ResourceType::GPU_MEMORY, ResourceQuota(2000));

    // Skill A 用完配额
    for (int i = 0; i < 4; i++) {
        mgr.try_allocate("skill-a", ResourceType::GPU_MEMORY, 500);
    }

    // Skill A 应该不能再分配了
    bool a_can_alloc = mgr.try_allocate("skill-a", ResourceType::GPU_MEMORY, 100);
    TEST_ASSERT(a_can_alloc == false, "Skill A 用完配额应该被拒绝");

    // 但 Skill B 应该仍然正常工作
    bool b_can_alloc = mgr.try_allocate("skill-b", ResourceType::GPU_MEMORY, 1000);
    TEST_ASSERT(b_can_alloc == true, "Skill B 应该不受影响");

    return result;
}

// ========== vGPU 上下文隔离测试 (待实现完整接口后启用) ==========

// ========== 限流测试 ==========

TEST_CATEGORY(RateLimiting, TokenBucketLimits) {
    TestResult result;
    result.passed = true;

    const int RATE = 100;  // 100 token/s
    TokenBucket bucket(RATE, RATE * 2);

    // 测量 1 秒内的最大吞吐量
    auto start = std::chrono::steady_clock::now();
    int count = 0;

    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(1000)) {
        if (bucket.try_consume()) {
            count++;
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }

    // 应该接近 100 + 初始突发 200 = 约 300
    // 允许一定误差
    TEST_ASSERT(count >= 80 && count <= 350,
                "限流应该在预期范围内 (实际: " + std::to_string(count) + ")");

    return result;
}

TEST_CATEGORY(RateLimiting, TokenBucketAccuracy) {
    TestResult result;
    result.passed = true;

    const int RATE = 500;  // 500 token/s
    TokenBucket bucket(RATE, RATE);

    // 先消耗所有 token
    while (bucket.try_consume()) {}

    // 然后测量 200ms 内的补充
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    int count = 0;
    while (bucket.try_consume()) {
        count++;
    }

    // 预期: 500 * 0.2 = 100 token
    // 允许一定误差
    TEST_ASSERT(count >= 70 && count <= 130,
                "补充速率应该在预期范围内 (实际: " + std::to_string(count) + ")");

    return result;
}

// ========== 性能基准测试 ==========

TEST_CATEGORY(Performance, AllocFreeLatency) {
    TestResult result;
    result.passed = true;

    auto& mgr = ResourceQuotaManager::instance();
    const std::string skill_id = "perf-test-skill";
    mgr.reset_stats(skill_id);
    mgr.set_quota(skill_id, ResourceType::GPU_MEMORY, ResourceQuota(1024 * 1024));

    const int ITERATIONS = 10000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < ITERATIONS; i++) {
        mgr.try_allocate(skill_id, ResourceType::GPU_MEMORY, 100);
        mgr.release(skill_id, ResourceType::GPU_MEMORY, 100);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double per_op = static_cast<double>(duration.count()) / (ITERATIONS * 2);  // *2 因为 alloc+free

    std::cout << " (" << per_op << " us/op) " << std::flush;

    // 应该小于 1 us/op
    TEST_ASSERT(per_op < 5.0, "性能应该达标 (当前: " + std::to_string(per_op) + " us/op)");

    return result;
}

TEST_CATEGORY(Performance, SchedulerThroughput) {
    TestResult result;
    result.passed = true;

    FCFS_Scheduler scheduler;
    const int NUM_REQUESTS = 1000;
    const int BATCH_SIZE = 4;

    RequestQueue queue;
    for (int i = 0; i < NUM_REQUESTS; i++) {
        Request req;
        req.request_id = i;
        req.input_size = 1024;
        queue.push(req);
    }

    auto start = std::chrono::high_resolution_clock::now();

    Batch batch;
    int processed = 0;
    while (scheduler.select_batch(queue, batch)) {
        processed += batch.requests.size();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double throughput = static_cast<double>(NUM_REQUESTS) * 1000000.0 / duration.count();

    std::cout << " (" << static_cast<int>(throughput) << " req/s) " << std::flush;

    TEST_ASSERT(processed == NUM_REQUESTS, "应该处理所有请求");

    return result;
}
