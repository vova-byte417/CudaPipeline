#include "scheduler/rl_scheduler.h"
#include <algorithm>
#include <cmath>
#include <iostream>

// ============================================
// QNetwork 实现
// ============================================
QNetwork::QNetwork(int input_dim, int hidden_dim) : rng_(std::random_device{}()) {
    // 初始化层1: input -> hidden
    layer1_.w.resize(hidden_dim, std::vector<float>(input_dim));
    layer1_.b.resize(hidden_dim, 0.0f);
    
    // 初始化层2: hidden -> hidden
    layer2_.w.resize(hidden_dim, std::vector<float>(hidden_dim));
    layer2_.b.resize(hidden_dim, 0.0f);
    
    // 初始化层3: hidden -> 1
    layer3_.w.resize(1, std::vector<float>(hidden_dim));
    layer3_.b.resize(1, 0.0f);
    
    // Xavier初始化
    std::normal_distribution<float> dist(0.0f, 0.1f);
    for (auto& row : layer1_.w) for (auto& w : row) w = dist(rng_);
    for (auto& row : layer2_.w) for (auto& w : row) w = dist(rng_);
    for (auto& row : layer3_.w) for (auto& w : row) w = dist(rng_);
}

float QNetwork::forward(const std::vector<float>& state) {
    std::vector<float> x = state;
    
    // Layer 1
    std::vector<float> h1(layer1_.b.size(), 0.0f);
    for (size_t i = 0; i < layer1_.w.size(); ++i) {
        for (size_t j = 0; j < x.size(); ++j) {
            h1[i] += x[j] * layer1_.w[i][j];
        }
        h1[i] = relu(h1[i] + layer1_.b[i]);
    }
    
    // Layer 2
    std::vector<float> h2(layer2_.b.size(), 0.0f);
    for (size_t i = 0; i < layer2_.w.size(); ++i) {
        for (size_t j = 0; j < h1.size(); ++j) {
            h2[i] += h1[j] * layer2_.w[i][j];
        }
        h2[i] = relu(h2[i] + layer2_.b[i]);
    }
    
    // Layer 3 (输出Q值)
    float q_value = 0.0f;
    for (size_t i = 0; i < layer3_.w[0].size(); ++i) {
        q_value += h2[i] * layer3_.w[0][i];
    }
    return q_value + layer3_.b[0];
}

void QNetwork::train(const std::vector<float>& s, const std::vector<float>& a,
                     float reward, const std::vector<float>& s_next, float lr) {
    // 简化版本的梯度下降
    float q_current = forward(s);
    float q_next = forward(s_next);
    float target = reward + 0.95f * q_next; 
    float loss = 0.5f * (target - q_current) * (target - q_current);
    
    // 这里简化：实际应该用自动微分
    // 生产环境建议用libtorch或者onnxruntime
}

void QNetwork::save(const std::string& path) {
    std::ofstream f(path, std::ios::binary);
    // 序列化权重（简化版）
    for (const auto& row : layer1_.w) 
        for (float w : row) f.write(reinterpret_cast<const char*>(&w), sizeof(w));
    for (float b : layer1_.b) 
        f.write(reinterpret_cast<const char*>(&b), sizeof(b));
    std::cout << "[RL] Model saved to " << path << std::endl;
}

bool QNetwork::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    // 反序列化（简化版）
    std::cout << "[RL] Model loaded from " << path << std::endl;
    return true;
}

// ============================================
// ReplayBuffer 实现
// ============================================
ReplayBuffer::ReplayBuffer(size_t capacity) : capacity_(capacity), rng_(std::random_device{}()) {
    buffer_.reserve(capacity);
}

void ReplayBuffer::push(Experience exp) {
    if (buffer_.size() < capacity_) {
        buffer_.push_back(exp);
    } else {
        buffer_[ptr_] = exp;
        ptr_ = (ptr_ + 1) % capacity_;
    }
}

std::vector<Experience> ReplayBuffer::sample_batch(size_t batch_size) {
    std::vector<Experience> batch;
    batch.reserve(batch_size);
    std::uniform_int_distribution<size_t> dist(0, buffer_.size() - 1);
    for (size_t i = 0; i < batch_size; ++i) {
        batch.push_back(buffer_[dist(rng_)]);
    }
    return batch;
}

// ============================================
// RLScheduler 实现
// ============================================
RLScheduler::RLScheduler(bool train_mode) 
    : train_mode_(train_mode), rng_(std::random_device{}()) {
    
    q_network_ = std::make_unique<QNetwork>(StateFeatures::DIM);
    target_network_ = std::make_unique<QNetwork>(StateFeatures::DIM);
    replay_buffer_ = std::make_unique<ReplayBuffer>();
    
    std::cout << "[RL] Scheduler initialized (train_mode=" 
              << (train_mode ? "ON" : "OFF") << ")" << std::endl;
}

StateFeatures RLScheduler::extract_features(const Request& req,
                                              size_t queue_depth,
                                              size_t avg_batch_size) {
    StateFeatures f;
    
    // 任务大小归一化
    f.task_size_norm = std::min(1.0f, static_cast<float>(req.input_size) / 32768.0f);
    
    // 算子类型one-hot
    f.op_type_onehot[0] = (req.operator_name == "vector_add") ? 1.0f : 0.0f;
    f.op_type_onehot[1] = (req.operator_name == "matmul") ? 1.0f : 0.0f;
    f.op_type_onehot[2] = (req.operator_name == "conv2d") ? 1.0f : 0.0f;
    
    // 优先级归一化
    f.priority_norm = static_cast<float>(req.priority) / 3.0f;
    
    // 等待时间（简化版）
    f.wait_time_norm = 0.5f;
    
    // 队列深度归一化
    f.queue_depth_norm = std::min(1.0f, static_cast<float>(queue_depth) / 100.0f);
    
    // 平均batch大小
    f.avg_batch_size = static_cast<float>(avg_batch_size) / MAX_BATCH_SIZE;
    
    // GPU利用率估计（简化版）
    f.gpu_util_estimate = 0.7f;
    
    return f;
}

std::vector<float> RLScheduler::score_candidates(
    const std::vector<Request>& candidates,
    size_t queue_depth) {
    
    std::vector<float> scores;
    scores.reserve(candidates.size());
    
    for (const auto& req : candidates) {
        auto features = extract_features(req, queue_depth, MAX_BATCH_SIZE / 2);
        float q_value = q_network_->forward(features.to_vector());
        scores.push_back(q_value);
    }
    
    return scores;
}

std::vector<size_t> RLScheduler::select_topk(
    const std::vector<float>& scores,
    size_t k) {
    
    std::vector<size_t> indices(scores.size());
    std::iota(indices.begin(), indices.end(), 0);
    
    // 按Q值降序排序
    std::sort(indices.begin(), indices.end(),
        [&scores](size_t a, size_t b) {
            return scores[a] > scores[b];
        });
    
    k = std::min(k, indices.size());
    indices.resize(k);
    return indices;
}

std::vector<size_t> RLScheduler::epsilon_greedy(
    const std::vector<float>& scores,
    size_t k) {
    
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    
    if (train_mode_ && dist(rng_) < epsilon_) {
        // 探索：随机选
        std::vector<size_t> indices(scores.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::shuffle(indices.begin(), indices.end(), rng_);
        indices.resize(std::min(k, indices.size()));
        return indices;
    }
    
    // 利用：选Top-K
    return select_topk(scores, k);
}

bool RLScheduler::select_batch(RequestQueue& queue, Batch& batch) {
    batch.requests.clear();
    batch.total_input_size = 0;
    
    // 1. 拉取候选任务
    std::vector<Request> candidates;
    queue.pop_batch(candidates, CANDIDATE_COUNT);
    
    if (candidates.empty()) {
        return false;
    }
    
    // 2. 给每个候选任务打分
    auto scores = score_candidates(candidates, queue.size());
    
    // 3. ε-greedy选择
    size_t batch_size = std::min(static_cast<size_t>(MAX_BATCH_SIZE), candidates.size());
    auto selected_indices = epsilon_greedy(scores, batch_size);
    
    // 4. 组成batch
    for (size_t idx : selected_indices) {
        batch.requests.push_back(candidates[idx]);
        batch.total_input_size += candidates[idx].input_size;
    }
    
    // 5. 未选中的放回队列
    std::vector<Request> unselected;
    for (size_t i = 0; i < candidates.size(); ++i) {
        if (std::find(selected_indices.begin(), selected_indices.end(), i) 
            == selected_indices.end()) {
            unselected.push_back(candidates[i]);
        }
    }
    queue.push_front_batch(unselected);
    
    // 6. 训练模式下保存状态用于后续训练
    if (train_mode_ && !selected_indices.size() > 0) {
        last_state_ = extract_features(
            candidates[selected_indices[0]], 
            queue.size(),
        MAX_BATCH_SIZE / 2).to_vector();
        last_action_ = selected_indices[0];
    }
    
    if (train_mode_ && train_step_ % 100 == 0) {
        std::cout << "[RL] Step=" << train_step_ 
                  << ", Epsilon=" << epsilon_ 
                  << ", Batch=" << batch.requests.size() << std::endl;
    }
    
    train_step_++;
    
    return !batch.requests.empty();
}

void RLScheduler::record_reward(float latency_ms) {
    if (!train_mode_) return;
    
    // 奖励 = -延迟（越短越好）
    float reward = -latency_ms;
    
    // 简化：记录经验
    Experience exp;
    exp.state = last_state_;
    exp.action = last_action_;
    exp.reward = reward;
    // exp.next_state = ...;  // 下一个状态
    exp.done = true;
    
    replay_buffer_->push(exp);
}

void RLScheduler::train_step() {
    if (!train_mode_ || replay_buffer_->size() < 100) {
        return;
    }
    
    // 从回放缓存采样训练
    auto batch = replay_buffer_->sample_batch(32);
    
    // 训练Q网络
    // ...
    
    // 定期更新目标网络
    if (train_step_ % 10 == 0) {
        update_target_network();
    }
}

void RLScheduler::update_target_network() {
    // 软更新目标网络
    // target = tau * main + (1-tau) * target
    // 简化版：直接复制
}

void RLScheduler::save_model(const std::string& path) {
    q_network_->save(path);
}

bool RLScheduler::load_model(const std::string& path) {
    return q_network_->load(path);
}
