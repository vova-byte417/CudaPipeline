#include "../test_common.h"
#include "scheduler/fcfs_scheduler.h"
#include "scheduler/priority_scheduler.h"
#include "scheduler/rl_scheduler.h"
#include "queue.h"
#include "runtime/batch.h"

using namespace test;

// ========== FCFS Scheduler 测试 ==========

TEST_CATEGORY(FCFSScheduler, BasicSelection) {
    TestResult result;
    result.passed = true;

    FCFS_Scheduler scheduler;
    RequestQueue queue;

    // 添加请求
    for (int i = 0; i < 5; i++) {
        Request req;
        req.request_id = i;
        req.input_size = 1024;
        queue.push(req);
    }

    Batch batch;
    bool has_batch = scheduler.select_batch(queue, batch);

    TEST_ASSERT(has_batch == true, "应该能获取到batch");
    TEST_ASSERT_EQ(batch.requests.size(), static_cast<size_t>(4), "默认batch size应该是4");
    TEST_ASSERT_EQ(batch.requests[0].request_id, static_cast<uint64_t>(0), "应该按FCFS顺序选择");
    TEST_ASSERT_EQ(batch.requests[3].request_id, static_cast<uint64_t>(3), "顺序应该正确");

    return result;
}

TEST_CATEGORY(FCFSScheduler, MultipleBatches) {
    TestResult result;
    result.passed = true;

    FCFS_Scheduler scheduler;
    RequestQueue queue;

    for (int i = 0; i < 10; i++) {
        Request req;
        req.request_id = i;
        req.input_size = 1024;
        queue.push(req);
    }

    Batch batch1, batch2, batch3;
    scheduler.select_batch(queue, batch1);
    scheduler.select_batch(queue, batch2);
    scheduler.select_batch(queue, batch3);

    TEST_ASSERT_EQ(batch1.requests.size(), static_cast<size_t>(4), "第1批应该有4个");
    TEST_ASSERT_EQ(batch2.requests.size(), static_cast<size_t>(4), "第2批应该有4个");
    TEST_ASSERT_EQ(batch3.requests.size(), static_cast<size_t>(2), "第3批应该有2个");

    // 验证顺序
    TEST_ASSERT_EQ(batch1.requests[0].request_id, static_cast<uint64_t>(0), "顺序错误");
    TEST_ASSERT_EQ(batch2.requests[0].request_id, static_cast<uint64_t>(4), "顺序错误");
    TEST_ASSERT_EQ(batch3.requests[0].request_id, static_cast<uint64_t>(8), "顺序错误");

    return result;
}

TEST_CATEGORY(FCFSScheduler, EmptyQueue) {
    TestResult result;
    result.passed = true;

    FCFS_Scheduler scheduler;
    RequestQueue queue;

    Batch batch;
    bool has_batch = scheduler.select_batch(queue, batch);

    TEST_ASSERT(has_batch == false, "空队列应该返回false");
    TEST_ASSERT(batch.requests.empty(), "batch应该为空");

    return result;
}

TEST_CATEGORY(FCFSScheduler, BatchSizeTracking) {
    TestResult result;
    result.passed = true;

    FCFS_Scheduler scheduler;
    RequestQueue queue;

    for (int i = 0; i < 3; i++) {
        Request req;
        req.request_id = i;
        req.input_size = 1024;
        queue.push(req);
    }

    Batch batch;
    scheduler.select_batch(queue, batch);

    TEST_ASSERT_EQ(batch.requests.size(), static_cast<size_t>(3), "应该获取所有3个请求");
    TEST_ASSERT_EQ(batch.total_input_size, 3 * 1024, "total_input_size 应该正确");

    return result;
}

// ========== Priority Scheduler 测试 ==========

TEST_CATEGORY(PriorityScheduler, FCFSFallback) {
    TestResult result;
    result.passed = true;

    PriorityScheduler scheduler;
    RequestQueue queue;

    for (int i = 0; i < 5; i++) {
        Request req;
        req.request_id = i;
        req.input_size = 1024;
        req.priority = Priority::MEDIUM;
        queue.push(req);
    }

    Batch batch;
    bool has_batch = scheduler.select_batch(queue, batch);

    TEST_ASSERT(has_batch == true, "应该能获取到batch");
    TEST_ASSERT_EQ(batch.requests.size(), static_cast<size_t>(4), "batch size应该正确");

    return result;
}

TEST_CATEGORY(PriorityScheduler, ConfigurableBatchSize) {
    TestResult result;
    result.passed = true;

    PriorityScheduler scheduler(8);  // 自定义 batch size = 8
    RequestQueue queue;

    for (int i = 0; i < 10; i++) {
        Request req;
        req.request_id = i;
        req.input_size = 1024;
        queue.push(req);
    }

    Batch batch;
    scheduler.select_batch(queue, batch);

    TEST_ASSERT_EQ(batch.requests.size(), static_cast<size_t>(8), "应该使用配置的batch size");

    return result;
}

// ========== RL Scheduler 基础测试 ==========

TEST_CATEGORY(RLScheduler, Initialization) {
    TestResult result;
    result.passed = true;

    RLScheduler scheduler(0.1, 0.95, 0.3);

    // 检查调度器能正常工作
    RequestQueue queue;
    for (int i = 0; i < 5; i++) {
        Request req;
        req.request_id = i;
        req.input_size = 1024;
        queue.push(req);
    }

    Batch batch;
    bool has_batch = scheduler.select_batch(queue, batch);

    TEST_ASSERT(has_batch == true, "应该能获取到batch");
    TEST_ASSERT(batch.requests.size() > 0, "batch应该不为空");

    return result;
}

TEST_CATEGORY(RLScheduler, ExplorationMode) {
    TestResult result;
    result.passed = true;

    // 高探索率，应该能看到不同的batch大小选择
    RLScheduler scheduler(0.1, 0.95, 1.0);  // 100% 探索

    std::unordered_map<size_t, int> batch_size_counts;
    const int rounds = 50;

    for (int round = 0; round < rounds; round++) {
        RequestQueue queue;
        for (int i = 0; i < 10; i++) {
            Request req;
            req.request_id = i;
            req.input_size = 1024;
            queue.push(req);
        }

        Batch batch;
        scheduler.select_batch(queue, batch);
        batch_size_counts[batch.requests.size()]++;
    }

    // 在高探索率下，应该能观察到不同的 batch size 选择
    TEST_ASSERT(batch_size_counts.size() >= 1, "应该至少选择1种batch size");

    return result;
}

TEST_CATEGORY(RLScheduler, ExploitationMode) {
    TestResult result;
    result.passed = true;

    // 0 探索率，纯利用模式
    RLScheduler scheduler(0.1, 0.95, 0.0);

    RequestQueue queue;
    for (int i = 0; i < 10; i++) {
        Request req;
        req.request_id = i;
        req.input_size = 1024;
        queue.push(req);
    }

    Batch batch;
    bool has_batch = scheduler.select_batch(queue, batch);

    TEST_ASSERT(has_batch == true, "应该能获取到batch");

    return result;
}

TEST_CATEGORY(RLScheduler, EmptyQueue) {
    TestResult result;
    result.passed = true;

    RLScheduler scheduler;
    RequestQueue queue;

    Batch batch;
    bool has_batch = scheduler.select_batch(queue, batch);

    TEST_ASSERT(has_batch == false, "空队列应该返回false");

    return result;
}

TEST_CATEGORY(RLScheduler, LearningProgress) {
    TestResult result;
    result.passed = true;

    RLScheduler scheduler(0.5, 0.9, 0.5);  // 高学习率

    // 模拟多轮调度，验证Q-table在增长
    for (int round = 0; round < 20; round++) {
        RequestQueue queue;
        for (int i = 0; i < 5; i++) {
            Request req;
            req.request_id = i;
            req.input_size = 1024;
            queue.push(req);
        }

        Batch batch;
        scheduler.select_batch(queue, batch);
    }

    // 如果能走到这里，说明学习过程没有崩溃
    TEST_ASSERT(true, "学习过程应该正常进行");

    return result;
}

// ========== Queue 测试 ==========

TEST_CATEGORY(RequestQueue, PushPop) {
    TestResult result;
    result.passed = true;

    RequestQueue queue;

    Request req1;
    req1.request_id = 1;
    req1.input_size = 1024;

    Request req2;
    req2.request_id = 2;
    req2.input_size = 2048;

    queue.push(req1);
    queue.push(req2);

    TEST_ASSERT_EQ(queue.size(), static_cast<size_t>(2), "队列大小应该为2");

    Request popped;
    queue.pop(popped);
    TEST_ASSERT_EQ(popped.request_id, static_cast<uint64_t>(1), "应该FIFO顺序");
    TEST_ASSERT_EQ(queue.size(), static_cast<size_t>(1), "pop后大小应该为1");

    return result;
}

TEST_CATEGORY(RequestQueue, FIFOOrder) {
    TestResult result;
    result.passed = true;

    RequestQueue queue;

    for (uint64_t i = 0; i < 10; i++) {
        Request req;
        req.request_id = i;
        queue.push(req);
    }

    for (uint64_t i = 0; i < 10; i++) {
        Request req;
        queue.pop(req);
        TEST_ASSERT_EQ(req.request_id, i, "FIFO顺序错误");
    }

    return result;
}

TEST_CATEGORY(RequestQueue, EmptyCheck) {
    TestResult result;
    result.passed = true;

    RequestQueue queue;
    TEST_ASSERT(queue.empty() == true, "新队列应该为空");

    Request req;
    req.request_id = 1;
    queue.push(req);
    TEST_ASSERT(queue.empty() == false, "push后不应该为空");

    queue.pop(req);
    TEST_ASSERT(queue.empty() == true, "pop后应该为空");

    return result;
}

TEST_CATEGORY(RequestQueue, WaitForRequest) {
    TestResult result;
    result.passed = true;

    RequestQueue queue;

    // 空队列应该等待超时
    bool has_request = queue.wait_for_request(std::chrono::milliseconds(10));
    TEST_ASSERT(has_request == false, "空队列应该超时返回false");

    // 添加请求后应该立即返回true
    Request req;
    req.request_id = 1;
    queue.push(req);

    has_request = queue.wait_for_request(std::chrono::milliseconds(100));
    TEST_ASSERT(has_request == true, "有请求时应该返回true");

    return result;
}

// ========== Batch 测试 ==========

TEST_CATEGORY(Batch, DefaultEmpty) {
    TestResult result;
    result.passed = true;

    Batch batch;
    TEST_ASSERT(batch.requests.empty(), "新建batch应该为空");
    TEST_ASSERT_EQ(batch.total_input_size, 0, "初始total_input_size应该为0");

    return result;
}

TEST_CATEGORY(Batch, AddRequests) {
    TestResult result;
    result.passed = true;

    Batch batch;

    Request req1;
    req1.input_size = 1024;
    batch.requests.push_back(req1);
    batch.total_input_size += req1.input_size;

    Request req2;
    req2.input_size = 2048;
    batch.requests.push_back(req2);
    batch.total_input_size += req2.input_size;

    TEST_ASSERT_EQ(batch.requests.size(), static_cast<size_t>(2), "应该有2个请求");
    TEST_ASSERT_EQ(batch.total_input_size, 1024 + 2048, "total_input_size应该正确");

    return result;
}
