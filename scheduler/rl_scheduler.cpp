bool RLScheduler::select_batch(RequestQueue& queue, Batch& batch)
{
    // TODO: 提取状态 → 模型 forward → 选择动作
    // 当前用 SJF 作为 baseline + 随机探索
    static SJFScheduler fallback;
    return fallback.select_batch(queue, batch);
}

void RLScheduler::train_step(const Batch& executed_batch, double reward)
{
    std::cout << "[RL] Reward: " << reward << " | Batch size: " 
              << executed_batch.requests.size() << std::endl;
    // 这里可记录经验回放、更新策略
}