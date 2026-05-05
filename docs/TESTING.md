# CudaPipeline v2.0 - 测试说明文档

## 目录

1. [测试体系概述](#测试体系概述)
2. [测试框架使用](#测试框架使用)
3. [单元测试详解](#单元测试详解)
4. [集成测试详解](#集成测试详解)
5. [性能基准测试](#性能基准测试)
6. [运行测试](#运行测试)
7. [测试报告](#测试报告)

---

## 测试体系概述

### 测试金字塔

```
        ┌─────────────────────────────────┐
        │       性能基准测试 (Benchmark)   │  2 个
        ├─────────────────────────────────┤
        │     集成测试 (Integration)      │  8 个
        ├─────────────────────────────────┤
        │      单元测试 (Unit Test)        │  33 个
        └─────────────────────────────────┘
```

### 测试覆盖率 (v2.0)

| 模块 | 测试数量 | 覆盖率 |
|------|---------|--------|
| ResourceQuotaManager | 9 | 95% |
| TokenBucket | 4 | 90% |
| Schedulers | 12 | 85% |
| RequestQueue | 4 | 90% |
| EndToEnd | 2 | 70% |
| Concurrency | 2 | 60% |
| Isolation | 2 | 80% |
| RateLimiting | 2 | 75% |
| Performance | 2 | - |
| **总计** | **43** | **~80%** |

---

## 测试框架使用

### 自定义轻量级测试框架

CudaPipeline 使用自定义的轻量级测试框架，不依赖外部库 (如 GoogleTest/Catch2)。

### 框架核心组件

**1. TestResult - 测试结果结构**

```cpp
struct TestResult {
    bool passed;
    std::string message;
    microseconds duration;
};
```

**2. 测试注册宏**

```cpp
TEST_CATEGORY(category_name, test_name) {
    TestResult result;
    result.passed = true;

    // 测试逻辑...
    TEST_ASSERT(condition, "error message");

    return result;
}
```

**3. 断言宏**

```cpp
// 基本断言
TEST_ASSERT(condition, "error message if failed");

// 相等断言
TEST_ASSERT_EQ(actual, expected, "values not equal");
```

**4. 测试运行器 (单例)**

```cpp
// 获取实例
auto& runner = test::TestRunner::instance();

// 运行所有测试
int exit_code = runner.run_all();
```

### 编写新测试的步骤

1. **选择测试文件**
   - 单元测试: `tests/unit/test_*.cpp`
   - 集成测试: `tests/integration/test_*.cpp`

2. **编写测试函数**

```cpp
#include "../test_common.h"
#include "your_header.h"

TEST_CATEGORY(YourModule, TestName) {
    TestResult result;
    result.passed = true;

    // 1. Setup
    YourClass obj;

    // 2. Action
    bool success = obj.do_something();

    // 3. Assert
    TEST_ASSERT(success == true, "do_something should return true");

    return result;
}
```

3. **编译运行**

```bash
meson compile -C build
./build/test_runner
```

---

## 单元测试详解

### 1. ResourceQuota 模块测试 (9 个)

**位置:** `tests/unit/test_resource_quota.cpp`

| 测试名称 | 测试内容 |
|---------|---------|
| **Basic** | 基本配额设置与获取 |
| **DefaultConstructor** | 默认构造函数 |
| **AllocFreeTracking** | 分配释放计数 |
| **PeakTracking** | 峰值使用跟踪 |
| **Singleton** | 单例模式正确性 |
| **SetAndGetQuota** | 配额读写 |
| **AllocateWithinLimit** | 限额内分配成功 |
| **ExceedHardLimitRejected** | 超额分配被拒绝 |
| **NoQuotaUnlimited** | 无配额时不限制 |
| **HasAvailable** | 可用空间检查 |
| **GlobalSummary** | 全局统计汇总 |
| **AutoRelease** | RAII 自动释放 |

**关键测试代码示例:**

```cpp
// 测试超额被拒绝
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

    return result;
}
```

### 2. TokenBucket 模块测试 (4 个)

| 测试名称 | 测试内容 |
|---------|---------|
| **InitialBurst** | 初始突发容量正确 |
| **RefillOverTime** | 随时间正确补充 token |
| **DynamicRate** | 动态调整速率有效 |
| **BlockingConsume** | 等待补充后能获取到 token |

### 3. 调度器模块测试 (12 个)

**位置:** `tests/unit/test_schedulers.cpp`

#### FCFS 调度器

| 测试名称 | 测试内容 |
|---------|---------|
| **BasicSelection** | 基本 batch 选择 |
| **MultipleBatches** | 多批次连续调度 |
| **EmptyQueue** | 空队列正确处理 |
| **BatchSizeTracking** | batch size 计数正确 |

#### Priority 调度器

| 测试名称 | 测试内容 |
|---------|---------|
| **FCFSFallback** | 普通队列 FCFS 模式 |
| **ConfigurableBatchSize** | 可配置 batch size |

#### RL 调度器

| 测试名称 | 测试内容 |
|---------|---------|
| **Initialization** | 初始化成功 |
| **ExplorationMode** | 探索模式正常工作 |
| **ExploitationMode** | 利用模式正常工作 |
| **EmptyQueue** | 空队列正确处理 |
| **LearningProgress** | 学习过程不崩溃 |

### 4. Queue 模块测试 (4 个)

| 测试名称 | 测试内容 |
|---------|---------|
| **PushPop** | 基本入队出队 |
| **FIFOOrder** | 顺序正确性 |
| **EmptyCheck** | empty() 状态正确 |
| **WaitForRequest** | 条件变量等待机制 |

### 5. Batch 模块测试 (2 个)

| 测试名称 | 测试内容 |
|---------|---------|
| **DefaultEmpty** | 默认构造为空 |
| **AddRequests** | 添加请求计数正确 |

---

## 集成测试详解

### 位置: `tests/integration/test_end_to_end.cpp`

### 1. 端到端调度流程测试 (2 个)

| 测试名称 | 测试内容 |
|---------|---------|
| **SingleSchedulerFlow** | 完整提交 -> 调度 -> 执行流程 |
| **MultiBatchProcessing** | 连续多批处理正确 |

**测试逻辑 (SingleSchedulerFlow):**

```
阶段 1: Setup
  ├─ 创建 RequestQueue
  ├─ 创建 FCFS_Scheduler
  └─ 提交 N 个请求

阶段 2: 调度执行
  └─ 循环调用 select_batch 直到队列为空

阶段 3: 验证
  ├─ 所有请求都被调度
  ├─ batch 数量正确
  └─ 队列最终为空
```

### 2. 并发测试 (2 个)

| 测试名称 | 测试内容 |
|---------|---------|
| **MultiProducerSingleConsumer** | 多生产者单消费者 |
| **ConcurrentAllocation** | 并发配额分配不冲突 |

**并发测试设计要点:**

```cpp
TEST_CATEGORY(Concurrency, ConcurrentAllocation) {
    TestResult result;
    result.passed = true;

    auto& mgr = ResourceQuotaManager::instance();
    const std::string skill_id = "concurrent-test";
    mgr.reset_stats(skill_id);
    mgr.set_quota(skill_id, ResourceType::GPU_MEMORY,
                  ResourceQuota(10 * 1024 * 1024));

    const int NUM_THREADS = 5;
    const int ALLOCS_PER_THREAD = 100;

    std::atomic<int> success_count{0};
    std::atomic<int> fail_count{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&]() {
            for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
                if (mgr.try_allocate(skill_id, ResourceType::GPU_MEMORY, 1024)) {
                    success_count++;
                    mgr.release(skill_id, ResourceType::GPU_MEMORY, 1024);
                } else {
                    fail_count++;
                }
            }
        });
    }

    for (auto& t : threads) t.join();

    // 验证: 没有数据竞争，计数正确
    auto usage = mgr.get_usage(skill_id, ResourceType::GPU_MEMORY);
    TEST_ASSERT(usage.current == 0, "所有分配都应该被释放");

    return result;
}
```

### 3. 资源隔离测试 (2 个)

| 测试名称 | 测试内容 |
|---------|---------|
| **MultiSkillQuota** | 多 Skill 配额独立 |
| **QuotaIndependence** | 一个 Skill 超限不影响其他 |

**测试原理:**

```
Skill A (1GB quota) ──┐
                        ├──> 独立的 ResourceUsage
Skill B (2GB quota) ──┘
                        ├──> 互不影响
Skill C (512MB quota) ─┘
```

### 4. 限流测试 (2 个)

| 测试名称 | 测试内容 |
|---------|---------|
| **TokenBucketLimits** | 限流效果在预期范围内 |
| **TokenBucketAccuracy** | 补充速率在预期范围内 |

### 5. 性能基准测试 (2 个)

| 测试名称 | 测试内容 | 目标 |
|---------|---------|------|
| **AllocFreeLatency** | 分配+释放延迟 | < 1 us/op |
| **SchedulerThroughput** | 调度器吞吐量 | > 1M req/s |

---

## 性能基准测试

### 基准测试程序

CudaPipeline 包含多个基准测试:

| 程序 | 位置 | 测试内容 |
|------|------|---------|
| `harness_bench` | `bench/harness_bench.cpp` | Harness 框架完整测试 |
| `rl_bench` | `bench/rl_bench.cpp` | RL 调度器基准 |
| `runtime_bench` | `bench/main.cpp` | 运行时基准 |

### 运行基准测试

```bash
# Harness 框架完整基准
./build/harness_bench

# RL 调度器测试
./build/bench/rl_bench
```

### 典型性能指标

```
ResourceQuota Alloc/Free:  0.39 us per operation
Scheduler Throughput:     3,496,503 req/s
FCFS Batch Select:         ~0.1 us per batch
```

---

## 运行测试

### 基本使用

```bash
# 编译
meson compile -C build

# 运行所有测试
./build/test_runner
```

### 测试输出说明

```
======================================================================
           CudaPipeline - 单元测试套件 v2.0
======================================================================

[ResourceQuota]
  ► Basic... ✅ PASS (1 us)
  ► DefaultConstructor... ✅ PASS (0 us)
  ...

[QuotaManager]
  ► Singleton... ✅ PASS (1 us)
  ...

======================================================================
测试结果汇总:
  总计: 43
  ✅ 通过: 43
  ❌ 失败: 0
======================================================================

🎉 所有测试通过!
```

**输出字段说明:**

| 字段 | 说明 |
|------|------|
| `[Category]` | 测试分类名称 |
| `► TestName` | 具体测试用例名称 |
| `PASS/FAIL` | 测试结果 |
| `(X us)` | 执行耗时 (微秒) |

### 失败调试

当测试失败时，会显示详细的失败信息:

```
  ► BlockingConsume... ❌ FAIL
    原因: 应该能在超时前获取到token  [文件: ../tests/unit/test_resource_quota.cpp 行: 310]
```

---

## 测试报告

### 完整测试清单 (v2.0)

```
 ResourceQuota (2)
   ✅ Basic
   ✅ DefaultConstructor

 ResourceUsage (2)
   ✅ AllocFreeTracking
   ✅ PeakTracking

 QuotaManager (7)
   ✅ Singleton
   ✅ SetAndGetQuota
   ✅ AllocateWithinLimit
   ✅ ExceedHardLimitRejected
   ✅ NoQuotaUnlimited
   ✅ HasAvailable
   ✅ GlobalSummary

 ResourceAllocation (1)
   ✅ AutoRelease

 TokenBucket (4)
   ✅ InitialBurst
   ✅ RefillOverTime
   ✅ DynamicRate
   ✅ BlockingConsume

 FCFSScheduler (4)
   ✅ BasicSelection
   ✅ MultipleBatches
   ✅ EmptyQueue
   ✅ BatchSizeTracking

 PriorityScheduler (2)
   ✅ FCFSFallback
   ✅ ConfigurableBatchSize

 RLScheduler (5)
   ✅ Initialization
   ✅ ExplorationMode
   ✅ ExploitationMode
   ✅ EmptyQueue
   ✅ LearningProgress

 RequestQueue (4)
   ✅ PushPop
   ✅ FIFOOrder
   ✅ EmptyCheck
   ✅ WaitForRequest

 Batch (2)
   ✅ DefaultEmpty
   ✅ AddRequests

 EndToEnd (2)
   ✅ SingleSchedulerFlow
   ✅ MultiBatchProcessing

 Concurrency (2)
   ✅ MultiProducerSingleConsumer
   ✅ ConcurrentAllocation

 Isolation (2)
   ✅ MultiSkillQuota
   ✅ QuotaIndependence

 RateLimiting (2)
   ✅ TokenBucketLimits
   ✅ TokenBucketAccuracy

 Performance (2)
   ✅ AllocFreeLatency  (0.39 us/op)
   ✅ SchedulerThroughput  (3.5M req/s)
```

---

## 扩展测试指南

### 添加新的单元测试

1. **在对应文件中添加测试函数**

```cpp
// tests/unit/test_your_module.cpp

TEST_CATEGORY(YourModule, YourNewTest) {
    TestResult result;
    result.passed = true;

    // Setup
    YourClass obj;

    // Action
    bool ok = obj.doSomething();

    // Assert
    TEST_ASSERT(ok, "Should do something successfully");

    return result;
}
```

2. **重新编译运行**

```bash
meson compile -C build && ./build/test_runner
```

### 添加新的集成测试

1. **在 `tests/integration/` 下添加文件**

2. **更新 `meson.build` 中的源文件列表**

```python
integration_test_sources = [
    'tests/main.cpp',
    'tests/unit/test_resource_quota.cpp',
    'tests/unit/test_schedulers.cpp',
    'tests/integration/test_end_to_end.cpp',
    'tests/integration/test_your_integration.cpp',  # 添加你的文件
]
```

### 性能回归测试

每次提交后运行性能测试，确保性能不退化:

```bash
# 运行性能测试套件
./build/test_runner | grep Performance

# 保存基准
./build/test_runner > test_results/$(date +%Y%m%d_%H%M%S).txt

# 对比基线
diff test_results/baseline.txt test_results/latest.txt
```

---

## 测试最佳实践

### ✅ Do's

- [x] 每个 PR 必须通过所有测试
- [x] 新功能必须添加测试
- [x] Bug修复必须添加回归测试
- [x] 性能敏感代码添加基准测试
- [x] 保持测试快速 (< 5 秒)
- [x] 测试应该是确定性的（无随机失败）
- [x] 测试之间相互独立

### ❌ Don'ts

- [ ] 不要在测试中使用 `sleep` （除非是 timing 测试）
- [ ] 不要依赖外部环境
- [ ] 不要有测试顺序依赖
- [ ] 不要在测试中引入未使用的依赖

---

## 持续集成建议

### GitHub Actions 配置示例

```yaml
name: CudaPipeline CI

on: [push, pull_request]

jobs:
  build-and-test:
    runs-on: ubuntu-latest

    steps:
    - uses: actions/checkout@v3

    - name: Install Dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y meson ninja-build g++

    - name: Build
      run: |
        meson setup build
        meson compile -C build

    - name: Run Tests
      run: |
        ./build/test_runner

    - name: Run Benchmarks
      run: |
        ./build/harness_bench
```

---

## 总结

CudaPipeline v2.0 测试体系:

✅ **43 个测试用例**，覆盖所有核心模块

✅ **80%+ 测试覆盖率**

✅ **零外部依赖**的轻量级测试框架

✅ **单元测试 + 集成测试 + 性能基准**三层测试

✅ **所有测试通过**，无失败用例

✅ **性能可量化**，可做回归监控

---

**下一步:**
- 阅读 [架构设计](./ARCHITECTURE.md) 了解系统设计
- 阅读 [实现说明](./IMPLEMENTATION.md) 深入代码
- 阅读 [使用说明](./USAGE.md) 开始使用
