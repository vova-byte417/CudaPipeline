#pragma once

#include "scheduler/scheduler.h"
#include "queue.h"
#include "runtime/batch.h"
#include <vector>
#include <unordered_map>
#include <random>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <chrono>

// Q-learning 状态特征
struct State {
    size_t queue_size;           // 队列长度
    double avg_wait_time;         // 平均等待时间
    size_t high_prio_ratio;     // 高优先级请求比例
    
    bool operator==(const State& other) const {
        return queue_size == other.queue_size &&
               std::abs(avg_wait_time - other.avg_wait_time) < 1e-6 &&
               high_prio_ratio == other.high_prio_ratio;
    }
};

// State 的哈希函数
namespace std {
    template<> struct hash<State> {
        size_t operator()(const State& s) const {
            size_t h1 = hash<size_t>()(s.queue_size);
            size_t h2 = hash<double>()(std::round(s.avg_wait_time * 100) / 100.0);
            size_t h3 = hash<size_t>()(s.high_prio_ratio);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}

// RL 动作：选择 batch 大小
enum class RLAction {
    BATCH_SIZE_1 = 0,   // 小 batch = 1
    BATCH_SIZE_2 = 1,   // 中 batch = 2
    BATCH_SIZE_4 = 2,   // 大 batch = 4
    BATCH_SIZE_8 = 3,   // 超大 batch = 8
    COUNT = 4
};

// RL Scheduler - 基于 Q-Learning 的自适应调度器
class RLScheduler : public Scheduler {
public:
    RLScheduler(double learning_rate = 0.1,
                 double discount_factor = 0.95,
                 double exploration_rate = 0.3)
        : alpha_(learning_rate),
          gamma_(discount_factor),
          epsilon_(exploration_rate),
          total_episodes_(0),
          rng_(std::random_device{}()) {
        
        // 初始化 Q-table
        init_q_table();
    }

    // 从队列选择 batch
    bool select_batch(RequestQueue& queue, Batch& batch) override {
        if (queue.empty()) {
            return false;
        }

        // 1. 观察当前状态
        State current_state = observe_state(queue);
        
        // 2. 选择动作 (ε-greedy)
        RLAction action = select_action(current_state);
        
        // 3. 执行动作
        size_t batch_size = get_batch_size(action);
        size_t selected = execute_batch(queue, batch, batch_size);
        
        // 4. 计算奖励
        double reward = calculate_reward(batch, selected);
        
        // 5. 观察新状态
        State next_state = observe_state(queue);
        
        // 6. 更新 Q-table
        update_q_table(current_state, action, reward, next_state);
        
        // 7. 衰减探索率
        decay_exploration();
        
        total_episodes_++;
        
        return selected > 0;
    }

    // 获取 batch 大小（用于测试）
    size_t get_batch_size(RLAction action) const {
        switch (static_cast<int>(action)) {
            case 0: return 1;
            case 1: return 2;
            case 2: return 4;
            case 3: return 8;
            default: return 2;
        }
    }

    // 打印 Q-table 统计
    void print_stats() const {
        std::cout << "\n" << std::string(50, '=') << "\n";
        std::cout << "           RL Scheduler Statistics           \n";
        std::cout << std::string(50, '=') << "\n";
        std::cout << "Total episodes: " << total_episodes_ << "\n";
        std::cout << "Current epsilon: " << std::fixed << std::setprecision(3) << epsilon_ << "\n";
        std::cout << "Q-table size: " << q_table_.size() << " states\n";
        
        // 统计各动作被选择的次数
        std::unordered_map<int, size_t> action_counts;
        for (const auto& [state, q_values] : q_table_) {
            int best_action = 0;
            double max_q = q_values[0];
            for (size_t i = 1; i < q_values.size(); i++) {
                if (q_values[i] > max_q) {
                    max_q = q_values[i];
                    best_action = static_cast<int>(i);
                }
            }
            action_counts[best_action]++;
        }
        
        std::cout << "\nPreferred action distribution:\n";
        std::cout << "  BATCH_SIZE_1: " << action_counts[0] << " states\n";
        std::cout << "  BATCH_SIZE_2: " << action_counts[1] << " states\n";
        std::cout << "  BATCH_SIZE_4: " << action_counts[2] << " states\n";
        std::cout << "  BATCH_SIZE_8: " << action_counts[3] << " states\n";
        std::cout << std::string(50, '=') << "\n\n";
    }

    // 重置学习状态
    void reset() {
        q_table_.clear();
        total_episodes_ = 0;
        epsilon_ = 0.3;
        init_q_table();
    }

    virtual ~RLScheduler() = default;

private:
    // Q-table: State -> [Q-value for each action]
    std::unordered_map<State, std::vector<double>> q_table_;
    
    // 超参数
    double alpha_;      // 学习率
    double gamma_;     // 折扣因子
    double epsilon_;  // 探索率
    
    // 统计
    size_t total_episodes_;
    
    // 随机数生成器
    std::mt19937 rng_;

    // 初始化 Q-table
    void init_q_table() {
        // 预填充一些常见状态
        for (size_t q_size : {0, 1, 2, 4, 8, 16, 32}) {
            for (double wait : {0.0, 1.0, 5.0, 10.0}) {
                for (size_t ratio : {0, 30, 60, 100}) {
                    State s;
                    s.queue_size = q_size;
                    s.avg_wait_time = wait;
                    s.high_prio_ratio = ratio;
                    q_table_[s] = {0.0, 0.0, 0.0, 0.0};
                }
            }
        }
    }

    // 观察当前状态
    State observe_state(RequestQueue& queue) {
        State s;
        s.queue_size = std::min(queue.size(), static_cast<size_t>(32));
        s.avg_wait_time = 0.0;  // 简化：实际可以记录等待时间
        s.high_prio_ratio = 50;  // 简化
        return s;
    }

    // ε-greedy 动作选择
    RLAction select_action(const State& state) {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        
        // 确保状态在 Q-table 中
        if (q_table_.find(state) == q_table_.end()) {
            q_table_[state] = {0.0, 0.0, 0.0, 0.0};
        }
        
        if (dist(rng_) < epsilon_) {
            // 探索：随机选择动作
            std::uniform_int_distribution<int> action_dist(0, 3);
            return static_cast<RLAction>(action_dist(rng_));
        } else {
            // 利用：选择 Q-value 最大的动作
            const auto& q_values = q_table_[state];
            int best_action = 0;
            double max_q = q_values[0];
            for (size_t i = 1; i < q_values.size(); i++) {
                if (q_values[i] > max_q) {
                    max_q = q_values[i];
                    best_action = static_cast<int>(i);
                }
            }
            return static_cast<RLAction>(best_action);
        }
    }

    // 执行动作：从队列选择指定数量的请求
    size_t execute_batch(RequestQueue& queue, Batch& batch, size_t batch_size) {
        batch.requests.clear();
        batch.total_input_size = 0;

        Request req;
        size_t count = 0;
        while (count < batch_size && queue.pop(req)) {
            batch.requests.push_back(req);
            batch.total_input_size += req.input_size;
            count++;
        }
        
        return count;
    }

    // 计算奖励
    double calculate_reward(const Batch& batch, size_t selected) {
        if (selected == 0) {
            return -1.0;  // 空 batch 惩罚
        }
        
        // 奖励 = 吞吐量 - 延迟
        double throughput_reward = static_cast<double>(selected) * 10.0;
        
        // batch 利用率奖励：batch 越大，单个请求开销越小
        double efficiency_reward = selected > 1 ? 5.0 : 0.0;
        
        return throughput_reward + efficiency_reward;
    }

    // Q-learning 更新
    void update_q_table(const State& s, RLAction action, double reward, const State& s_next) {
        // 确保 s_next 在 Q-table 中
        if (q_table_.find(s_next) == q_table_.end()) {
            q_table_[s_next] = {0.0, 0.0, 0.0, 0.0};
        }
        
        // 找到 s_next 的最大 Q-value
        const auto& q_next = q_table_[s_next];
        double max_q_next = *std::max_element(q_next.begin(), q_next.end());
        
        // Q-learning 公式
        int action_idx = static_cast<int>(action);
        double& q_current = q_table_[s][action_idx];
        q_table_[s][action_idx] = q_current + alpha_ * (reward + gamma_ * max_q_next - q_current);
    }

    // 衰减探索率
    void decay_exploration() {
        // 指数衰减
        epsilon_ = std::max(0.01, epsilon_ * 0.999);
    }
};
