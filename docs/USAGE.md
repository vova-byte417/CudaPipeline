# CudaPipeline v2.0 - 使用说明文档

## 快速开始

### 1. 编译项目

```bash
# 配置构建
meson setup build

# 编译
meson compile -C build
```

### 2. 运行测试套件

```bash
# 运行所有测试
./build/test_runner

# 查看测试帮助
./build/test_runner --help
```

### 3. 运行基准测试

```bash
# Harness 框架完整测试
./build/harness_bench

# RL 调度器测试
./build/bench/rl_bench

# 所有 benchmark 目录
ls build/*bench*
```

---

## 资源配额管理使用指南

### 基础用法

```cpp
#include "harness/resource_quota.h"

using namespace harness;

int main() {
    // 获取管理器单例
    auto& mgr = ResourceQuotaManager::instance();

    // 为 Skill 设置配额
    mgr.set_quota("my-ai-skill", ResourceType::GPU_MEMORY,
                  ResourceQuota(1024 * 1024 * 1024));  // 1GB

    // 尝试分配资源
    bool success = mgr.try_allocate("my-ai-skill",
                                     ResourceType::GPU_MEMORY,
                                     256 * 1024 * 1024);  // 256MB

    if (success) {
        std::cout << "分配成功!" << std::endl;

        // 使用资源...

        // 释放资源
        mgr.release("my-ai-skill",
                    ResourceType::GPU_MEMORY,
                    256 * 1024 * 1024);
    } else {
        std::cout << "超出配额!" << std::endl;
    }

    return 0;
}
```

### RAII 自动管理（推荐）

```cpp
#include "harness/resource_quota.h"

using namespace harness;

void process_request(const std::string& skill_id, size_t memory_needed) {
    // 构造时分配，析构时自动释放
    ResourceAllocation alloc(skill_id, ResourceType::GPU_MEMORY, memory_needed);

    if (!alloc) {
        throw std::runtime_error("配额不足!");
    }

    // 使用资源...
    do_compute();

}  // 离开作用域，自动释放资源
```

### 软硬配额

```cpp
// 设置配额
ResourceQuota quota;
quota.hard_limit = 1024 * 1024 * 1024;  // 1GB 硬限制
quota.soft_limit = 768 * 1024 * 1024;   // 768MB 软限制
quota.enabled = true;

mgr.set_quota("skill-id", ResourceType::GPU_MEMORY, quota);

// 设置超限回调
mgr.set_soft_limit_callback([](const std::string& skill_id, ResourceType type,
                                size_t requested, size_t limit) {
    std::cout << "WARN: " << skill_id << " 超过软限制!" << std::endl;
});

mgr.set_hard_limit_callback([](const std::string& skill_id, ResourceType type,
                                 size_t requested, size_t limit) {
    std::cout << "ERROR: " << skill_id << " 超过硬限制，已拒绝!" << std::endl;
});
```

### 查询使用情况

```cpp
// 获取快照
ResourceUsageSnapshot usage = mgr.get_usage("skill-id", ResourceType::GPU_MEMORY);

std::cout << "当前使用: " << usage.current << " bytes" << std::endl;
std::cout << "峰值使用: " << usage.peak << " bytes" << std::endl;
std::cout << "总分配次数: " << usage.total_allocated << std::endl;
std::cout << "超限次数: " << usage.hard_limit_violations << std::endl;

// 检查是否有足够可用空间
bool has_space = mgr.has_available("skill-id", ResourceType::GPU_MEMORY, 1024 * 1024);
```

### 多 Skill 隔离

```cpp
// Skill A - 1GB 配额
mgr.set_quota("skill-a", ResourceType::GPU_MEMORY, ResourceQuota(1024 * 1024 * 1024));

// Skill B - 2GB 配额
mgr.set_quota("skill-b", ResourceType::GPU_MEMORY, ResourceQuota(2048 * 1024 * 1024));

// Skill C - 512MB 配额
mgr.set_quota("skill-c", ResourceType::GPU_MEMORY, ResourceQuota(512 * 1024 * 1024));

// 每个 Skill 独立记账，互不影响
```

---

## 令牌桶限流使用指南

### 基础限流

```cpp
#include "harness/resource_quota.h"

using namespace harness;

int main() {
    // 创建令牌桶: 100 token/s，最大突发 200
    TokenBucket bucket(100, 200);

    // 处理请求
    while (running) {
        if (bucket.try_consume()) {
            // 获得 token，处理请求
            process_request();
        } else {
            // 被限流，可以 sleep 或返回错误
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    return 0;
}
```

### 阻塞模式

```cpp
TokenBucket bucket(100, 200);

// 阻塞等待，最多 100ms
if (bucket.consume(1, std::chrono::milliseconds(100))) {
    // 获得 token
} else {
    // 超时
}
```

### 动态调整速率

```cpp
TokenBucket bucket(100, 200);

// 根据负载动态调整
if (load > 0.8) {
    bucket.set_rate(50);  // 降低速率
} else {
    bucket.set_rate(200); // 提高速率
}
```

### 获取当前可用量

```cpp
size_t available = bucket.available();
std::cout << "当前可用 token: " << available << std::endl;
```

---

## 调度器使用指南

### FCFS 调度器（先来先服务）

```cpp
#include "scheduler/fcfs_scheduler.h"
#include "queue.h"
#include "runtime/batch.h"

FCFS_Scheduler scheduler;
RequestQueue queue;

// 提交请求
for (int i = 0; i < 100; i++) {
    Request req;
    req.request_id = i;
    req.input_size = 1024;
    queue.push(req);
}

// 调度 batch
Batch batch;
while (scheduler.select_batch(queue, batch)) {
    std::cout << "处理 batch: " << batch.requests.size() << " 个请求" << std::endl;

    // 执行计算...
    execute_batch(batch);
}
```

### RL 调度器（强化学习自适应）

```cpp
#include "scheduler/rl_scheduler.h"

// 创建 RL 调度器
RLScheduler scheduler(
    0.1,    // 学习率
    0.95,   // 折扣因子
    0.3     // 初始探索率
);

// 使用方式与 FCFS 相同
RequestQueue queue;
Batch batch;

// 调度器会自动学习最优的 batch size
while (scheduler.select_batch(queue, batch)) {
    // 执行...
    execute_batch(batch);
}
```

### 优先级调度器

```cpp
#include "scheduler/priority_scheduler.h"
#include "priority_queue.h"

PriorityScheduler scheduler;
PriorityRequestQueue prio_queue;

// 提交不同优先级的请求
Request high_prio_req;
high_prio_req.priority = Priority::HIGH;
prio_queue.push(high_prio_req);

Request low_prio_req;
low_prio_req.priority = Priority::LOW;
prio_queue.push(low_prio_req);

// 调度会按加权优先级选择
Batch batch;
scheduler.select_batch(prio_queue, batch);
```

---

## 多线程并发编程指南

### 线程安全的组件

以下组件可以安全地在多线程中使用：

| 组件 | 线程安全方式 |
|------|-------------|
| ResourceQuotaManager | Mutex 保护 |
| TokenBucket | Mutex 保护 |
| RequestQueue | Mutex + CV |
| PriorityRequestQueue | Mutex + CV |
| DeadlineQueue | Mutex + CV |

### 生产者-消费者模式

```cpp
#include "queue.h"
#include <thread>
#include <vector>

RequestQueue queue;
std::atomic<bool> running{true};

// 生产者线程
void producer(int id) {
    for (int i = 0; i < 100; i++) {
        Request req;
        req.request_id = id * 1000 + i;
        queue.push(req);
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
}

// 消费者线程
void consumer(int id) {
    FCFS_Scheduler scheduler;
    Batch batch;

    while (running) {
        if (scheduler.select_batch(queue, batch)) {
            // 处理 batch...
            std::cout << "Worker " << id << ": "
                      << batch.requests.size() << " requests" << std::endl;
        }
    }
}

int main() {
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    // 3 个生产者
    for (int i = 0; i < 3; i++) {
        producers.emplace_back(producer, i);
    }

    // 2 个消费者
    for (int i = 0; i < 2; i++) {
        consumers.emplace_back(consumer, i);
    }

    // 等待完成
    for (auto& t : producers) t.join();

    std::this_thread::sleep_for(std::chrono::seconds(1));
    running = false;

    for (auto& t : consumers) t.join();

    return 0;
}
```

### 并发配额管理

```cpp
auto& mgr = ResourceQuotaManager::instance();
mgr.set_quota("test-skill", ResourceType::GPU_MEMORY, ResourceQuota(10 * 1024 * 1024));

std::vector<std::thread> threads;
std::atomic<int> success_count{0};
std::atomic<int> fail_count{0};

for (int i = 0; i < 10; i++) {
    threads.emplace_back([&]() {
        for (int j = 0; j < 100; j++) {
            if (mgr.try_allocate("test-skill", ResourceType::GPU_MEMORY, 1024)) {
                success_count++;
                std::this_thread::sleep_for(std::chrono::microseconds(1));
                mgr.release("test-skill", ResourceType::GPU_MEMORY, 1024);
            } else {
                fail_count++;
            }
        }
    });
}

for (auto& t : threads) t.join();

std::cout << "Success: " << success_count << std::endl;
std::cout << "Fail: " << fail_count << std::endl;
```

---

## 性能调优指南

### 1. Batch Size 调优

```
小 Batch (1-2):
  ✅ 延迟低
  ❌ 吞吐量低
  ❌ GPU 利用率低

中等 Batch (4-8):
  ✅ 平衡延迟和吞吐量
  ✅ 推荐大多数场景

大 Batch (16+):
  ✅ 吞吐量最高
  ❌ 延迟高
  ❌ 可能造成饥饿
```

**推荐:** 使用 RL 调度器自动学习最优 batch size

### 2. 配额设置建议

```
GPU 显存配额:
  推理服务: 512MB - 2GB per skill
  训练服务: 8GB - 32GB per skill

并发限制:
  GPU Kernels: 8 - 32 per skill
  CUDA Streams: 4 - 16 per skill
```

### 3. 限流配置

```
在线推理服务:
  Rate: 100 - 1000 req/s
  Burst: 2x - 5x rate

批量处理服务:
  Rate: 10 - 100 req/s
  Burst: 10x rate
```

---

## 监控与可观测性

### 资源使用指标

```cpp
// 获取 Skill 资源使用
auto usage = mgr.get_usage("skill-id", ResourceType::GPU_MEMORY);

// 计算使用率
double utilization = static_cast<double>(usage.current) / quota.hard_limit;

if (utilization > 0.9) {
    std::cout << "WARNING: Skill approaching quota limit!" << std::endl;
}
```

### 超限告警

```cpp
mgr.set_hard_limit_callback([](const std::string& skill_id, ...) {
    // 发送告警
    alerting_system.send_alert("QUOTA_EXCEEDED", skill_id);

    // 记录日志
    logger.error("Skill {} exceeded GPU memory quota!", skill_id);
});
```

---

## 常见问题 FAQ

### Q1: ResourceQuotaManager 是线程安全的吗？

是的，内部使用 `std::mutex` 保护所有共享数据结构。多线程可以安全地并发调用。

### Q2: TokenBucket 的时间精度是多少？

毫秒级精度，使用 `std::chrono::steady_clock`。

### Q3: RL 调度器需要多久才能收敛？

通常在几百到几千个请求后开始收敛。可以通过调整学习率 `alpha` 来控制收敛速度。

### Q4: 如何选择适合的调度器？

| 场景 | 推荐调度器 |
|------|-----------|
| 简单流量 | FCFS |
| 多优先级 | Priority |
| 实时任务 | EDF |
| 动态/未知流量 | RL |

**推荐:** 始终从 RL 调度器开始，根据需要 fallback 到 FCFS

### Q5: 性能开销有多大？

```
配额检查: ~0.39 us
调度器开销: ~0.3 us
TokenBucket: ~0.5 us
总体框架开销: < 1% (可忽略)
```

---

## 最佳实践清单

### ✅ 资源管理
- [ ] 始终使用 RAII 的 `ResourceAllocation` 管理资源
- [ ] 设置合理的软限制，提前发现问题
- [ ] 注册硬限制回调，及时处理超限
- [ ] 定期检查资源使用情况

### ✅ 调度策略
- [ ] 使用 RL 调度器作为默认
- [ ] 设置合适的 max_batch_size
- [ ] 监控 batch size 分布
- [ ] 评估不同调度器的性能差异

### ✅ 并发编程
- [ ] 使用 RequestQueue 进行线程间通信
- [ ] 避免在持有锁时执行耗时操作
- [ ] 正确设置 wait_for 超时
- [ ] 使用 atomic 进行轻量级同步

### ✅ 限流
- [ ] 为每个 Skill 设置独立的 TokenBucket
- [ ] 配置合理的突发容量
- [ ] 监控限流发生频率
- [ ] 动态调整速率应对流量波动

### ✅ 监控
- [ ] 定期输出资源使用统计
- [ ] 设置超限告警阈值
- [ ] 监控调度器性能指标
- [ ] 建立性能基线

---

## 下一步

阅读更多文档:
- [架构设计](./ARCHITECTURE.md) - 了解整体架构
- [实现说明](./IMPLEMENTATION.md) - 深入代码实现
- [测试说明](./TESTING.md) - 测试体系详解
