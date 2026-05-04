#pragma once
#include "scheduler/scheduler.h"

class RLScheduler : public Scheduler {
public:
    bool select_batch(RequestQueue& queue, Batch& batch) override;

    // RL 接口（面试亮点）
    void train_step(const Batch& executed_batch, double reward);
    void load_model(const std::string& path);
    void save_model(const std::string& path);

private:
    // 状态特征：queue length, avg exec time, utilization 等
    // 动作：batch size, 选择哪些任务
    // 这里先用规则模拟，后面可接 PyTorch C++ / ONNX
    int current_policy = 0;   // 0=exploration, 1=exploitation
};