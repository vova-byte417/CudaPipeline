#include "harness/resource_quota.h"
#include <algorithm>

namespace harness {

// ========== ResourceQuotaManager 实现 ==========

ResourceQuotaManager& ResourceQuotaManager::instance() {
    static ResourceQuotaManager instance;
    return instance;
}

void ResourceQuotaManager::set_quota(const std::string& skill_id, ResourceType type, const ResourceQuota& quota) {
    std::lock_guard<std::mutex> lock(mutex_);
    quotas_[skill_id][type] = quota;
}

ResourceQuota ResourceQuotaManager::get_quota(const std::string& skill_id, ResourceType type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto skill_it = quotas_.find(skill_id);
    if (skill_it != quotas_.end()) {
        auto it = skill_it->second.find(type);
        if (it != skill_it->second.end()) {
            return it->second;
        }
    }
    return ResourceQuota();
}

void ResourceQuotaManager::ensure_skill_exists(const std::string& skill_id) {
    // 这里不需要做什么，operator[] 会自动创建
    // 我们在 try_allocate 时检查
}

bool ResourceQuotaManager::try_allocate(const std::string& skill_id, ResourceType type, size_t amount) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto& usage = usage_[skill_id][type];
    auto& quota = quotas_[skill_id][type];

    if (!quota.enabled) {
        // 没有配额限制，直接允许
        usage.record_alloc(amount);
        return true;
    }

    // 检查硬限制
    if (usage.would_exceed_hard(amount, quota.hard_limit)) {
        usage.hard_limit_violations++;
        if (hard_limit_callback_) {
            hard_limit_callback_(skill_id, type, amount, quota.hard_limit);
        }
        return false;
    }

    // 检查软限制
    if (usage.would_exceed_soft(amount, quota.soft_limit)) {
        usage.soft_limit_violations++;
        if (soft_limit_callback_) {
            soft_limit_callback_(skill_id, type, amount, quota.soft_limit);
        }
        // 软限制只警告，允许通过
    }

    usage.record_alloc(amount);
    return true;
}

void ResourceQuotaManager::release(const std::string& skill_id, ResourceType type, size_t amount) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& usage = usage_[skill_id][type];
    usage.record_free(amount);
}

ResourceUsageSnapshot ResourceQuotaManager::get_usage(const std::string& skill_id, ResourceType type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto skill_it = usage_.find(skill_id);
    if (skill_it != usage_.end()) {
        auto it = skill_it->second.find(type);
        if (it != skill_it->second.end()) {
            return it->second.snapshot();
        }
    }
    return ResourceUsageSnapshot();
}

bool ResourceQuotaManager::has_available(const std::string& skill_id, ResourceType type, size_t amount) const {
    auto usage = get_usage(skill_id, type);
    auto quota = get_quota(skill_id, type);
    if (!quota.enabled) return true;
    return usage.current + amount <= quota.hard_limit;
}

ResourceQuotaManager::GlobalSummary ResourceQuotaManager::get_global_summary(ResourceType type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    GlobalSummary summary;

    for (const auto& [skill_id, type_map] : usage_) {
        auto it = type_map.find(type);
        if (it != type_map.end()) {
            summary.total_allocated += it->second.current.load();
            summary.active_skills++;
            summary.limit_violations += it->second.hard_limit_violations.load();
        }
    }

    return summary;
}

void ResourceQuotaManager::set_soft_limit_callback(ViolationCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    soft_limit_callback_ = cb;
}

void ResourceQuotaManager::set_hard_limit_callback(ViolationCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    hard_limit_callback_ = cb;
}

void ResourceQuotaManager::reset_stats(const std::string& skill_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    usage_[skill_id].clear();
}

void ResourceQuotaManager::reset_all_stats() {
    std::lock_guard<std::mutex> lock(mutex_);
    usage_.clear();
}

void ResourceQuotaManager::remove_skill(const std::string& skill_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    usage_.erase(skill_id);
    quotas_.erase(skill_id);
}

} // namespace harness
