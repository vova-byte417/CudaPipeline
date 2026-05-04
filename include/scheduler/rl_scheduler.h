#pragma once

#include "scheduler/scheduler.h"
#include "request.h"
#include "queue.h"
#include "runtime/batch.h"

#include <vector>
#include <string>
#include <unordered_map>
#include <random>
#include <memory>
#include <fstream>

// ============================================
// 1. 特征提取器
// ============================================
struct StateFeatures {
    // 任务特征
    float task_size_norm;      // 归一化任务大小 [0, 1]
    float op_type_onehot[3];   // 算子类型 one-hot: vec_add/matmul/conv
    float priority_norm;         // 优先级 [0, 1]
    float wait_time_norm;     // 等待时间 [0, 1]
    
    // 队列状态特征
    float queue_depth_norm;   // 队列深度
    float avg_batch_size;      // 平均batch大小
    float gpu_util_estimate;   // GPU利用率估计
    
    static constexpr int DIM =  9;  // 总特征维度
    
    std::vector<float> to_vector() const {
        std::vector<float> f;
        f.reserve(DIM);
        f.push_back(task_size_norm);
        f.push_back(op_type_onehot[0]);
        f.push_back(op_type_onehot[1]);
        f.push_back(op_type_onehot[2]);
        f.push_back(priority_norm);
        f.push_back(wait_time_norm);
        f.push_back(queue_depth_norm);
        f.push_back(avg_batch_size);
        f.push_back(gpu_util_estimate);
        return f;
    }
};

// ============================================
// 2. 简单的Q网络实现 (3层MLP)
// ============================================
class QNetwork {
public:
    QNetwork(int input_dim, int hidden_dim = 64);
    
    // 前向传播：输入状态特征，输出Q值
    float forward(const std::vector<float>& state);
    
    // 训练一步：(s, a, r, s')
    void train(const std::vector<float>& s, 
               const std::vector<float>& a,
               float reward,
               const std::vector<float>& s_next,
               float lr = 0.001f);
    
    // 保存/加载模型
    void save(const std::string& path);
    bool load(const std::string& path);
    
private:
    // 简化：用一个简单的线性层 + ReLU
    struct Layer {
        std::vector<std::vector<float>> w;
        std::vector<float> b;
    };
    
    Layer layer1_;  // input -> hidden
    Layer layer2_;  // hidden -> hidden
    Layer layer3_;  // hidden -> 1 (Q值
    
    std::mt19937 rng_;
    
    float relu(float x) { return x > 0 ? x : 0; }
};

// ============================================
// 3. 经验回放缓存
// ============================================
struct Experience {
    std::vector<float> state;
    int action;           // 选了哪个任务
    float reward;         // 奖励（负的延迟
    std::vector<float> next_state;
    bool done;
};

class ReplayBuffer {
public:
    ReplayBuffer(size_t capacity = 10000);
    
    void push(Experience exp);
    std::vector<Experience> sample_batch(size_t batch_size);
    size_t size() const { return buffer_.size(); }
    
private:
    std::vector<Experience> buffer_;
    size_t capacity_;
    size_t ptr_ = 0;
    std::mt19937 rng_;
};

// ============================================
// 4. RL调度器主类
// ============================================
class RLScheduler : public Scheduler {
public:
    RLScheduler(bool train_mode = true);
    
    // 核心接口：选择batch
    bool select_batch(RequestQueue& queue, Batch& batch) override;
    
    // 训练相关
    void record_reward(float latency_ms);  // 记录这次决策的奖励
    void train_step();
    void update_target_network();
    
    // 超参数
    void set_epsilon(float eps) { epsilon_ = eps; }
    float get_epsilon() const { return epsilon_; }
    
    // 保存/加载模型
    void save_model(const std::string& path);
    bool load_model(const std::string& path);
    
private:
    // 提取单个任务的特征
    StateFeatures extract_features(const Request& req, 
                                  size_t queue_depth,
                                  size_t avg_batch_size);
    
    // 对所有候选任务打分
    std::vector<float> score_candidates(const std::vector<Request>& candidates,
                                        size_t queue_depth);
    
    // 选择Top-K
    std::vector<size_t> select_topk(const std::vector<float>& scores, 
                                      size_t k);
    
    // ε-greedy探索
    std::vector<size_t> epsilon_greedy(const std::vector<float>& scores,
                                          size_t k);
    
    std::unique_ptr<QNetwork> q_network_;
    std::unique_ptr<QNetwork> target_network_;
    std::unique_ptr<ReplayBuffer> replay_buffer_;
    
    float epsilon_ = 0.1f;      // 探索率
    float gamma_ = 0.95f;         // 折扣因子
    
    bool train_mode_;
    size_t train_step_ = 0;
    
    // 最近一次决策的状态
    std::vector<float> last_state_;
    int last_action_ = 0;
    
    std::mt19937 rng_;
    
    static constexpr int MAX_BATCH_SIZE = 8;
    static constexpr int CANDIDATE_COUNT = 24;  // MAX_BATCH_SIZE * 3
};
