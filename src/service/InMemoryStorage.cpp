#include "InMemoryStorage.h"

#include <algorithm>
#include <stdexcept>

#include "../constants/KacheConstants.h"
#include "LRUPolicy.h"

InMemoryStorage::InMemoryStorage(size_t cap, const std::string& policyType) : capacity_(cap) {
    if (policyType == LRU_EVICTION_POLICY) {
        policy_ = std::make_unique<LRUPolicy>();
    } else {
        throw std::invalid_argument("Unsupported eviction policy");
    }

    cleanupThread_ = std::thread(&InMemoryStorage::cleanupExpiredKeys, this);
}

InMemoryStorage::~InMemoryStorage() {
    stopFlag_ = true;
    if (cleanupThread_.joinable()) {
        cleanupThread_.join();
    }
}

std::optional<std::string> InMemoryStorage::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = std::chrono::steady_clock::now();

    if (purgeIfExpiredLocked(key, now)) {
        return std::nullopt;
    }

    auto it = store_.find(key);
    if (it == store_.end()) {
        return std::nullopt;
    }

    policy_->onGet(key);
    return it->second.value;
}

void InMemoryStorage::set(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    setInternalLocked(key, value, std::nullopt);
}

void InMemoryStorage::set(const std::string& key, const std::string& value, std::chrono::seconds ttl) {
    std::lock_guard<std::mutex> lock(mutex_);
    setInternalLocked(key, value, std::chrono::steady_clock::now() + ttl);
}

bool InMemoryStorage::del(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (store_.erase(key) == 0) {
        return false;
    }

    policy_->onDelete(key);
    return true;
}

bool InMemoryStorage::exists(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = std::chrono::steady_clock::now();

    if (purgeIfExpiredLocked(key, now)) {
        return false;
    }

    return store_.find(key) != store_.end();
}

bool InMemoryStorage::expire(const std::string& key, std::chrono::seconds ttl) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = std::chrono::steady_clock::now();

    if (purgeIfExpiredLocked(key, now)) {
        return false;
    }

    auto it = store_.find(key);
    if (it == store_.end()) {
        return false;
    }

    it->second.expiry = now + ttl;
    return true;
}

long InMemoryStorage::ttl(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = std::chrono::steady_clock::now();

    if (purgeIfExpiredLocked(key, now)) {
        return -2;
    }

    auto it = store_.find(key);
    if (it == store_.end()) {
        return -2;
    }

    if (!it->second.expiry.has_value()) {
        return -1;
    }

    const auto remaining = std::chrono::duration_cast<std::chrono::seconds>(it->second.expiry.value() - now).count();
    return remaining > 0 ? remaining : 0;
}

std::vector<std::string> InMemoryStorage::keys(const std::string& pattern) {
    if (pattern != "*") {
        throw std::invalid_argument("Only KEYS * is supported");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = std::chrono::steady_clock::now();
    std::vector<std::string> result;

    for (auto it = store_.begin(); it != store_.end();) {
        if (isExpired(it->second, now)) {
            policy_->onDelete(it->first);
            it = store_.erase(it);
            continue;
        }

        result.push_back(it->first);
        ++it;
    }

    std::sort(result.begin(), result.end());
    return result;
}

void InMemoryStorage::flushAll() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& [key, _] : store_) {
        policy_->onDelete(key);
    }

    store_.clear();
}

bool InMemoryStorage::isExpired(const Entry& entry, TimePoint now) const {
    return entry.expiry.has_value() && now >= entry.expiry.value();
}

bool InMemoryStorage::purgeIfExpiredLocked(const std::string& key, TimePoint now) {
    auto it = store_.find(key);
    if (it == store_.end() || !isExpired(it->second, now)) {
        return false;
    }

    policy_->onDelete(key);
    store_.erase(it);
    return true;
}

void InMemoryStorage::setInternalLocked(const std::string& key, const std::string& value, std::optional<TimePoint> expiry) {
    if (store_.find(key) == store_.end() && store_.size() >= capacity_) {
        const std::string evictedKey = policy_->evict();
        store_.erase(evictedKey);
    }

    store_[key] = Entry{value, expiry};
    policy_->onSet(key);
}

void InMemoryStorage::cleanupExpiredKeys() {
    while (!stopFlag_) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            const auto now = std::chrono::steady_clock::now();

            for (auto it = store_.begin(); it != store_.end();) {
                if (isExpired(it->second, now)) {
                    policy_->onDelete(it->first);
                    it = store_.erase(it);
                } else {
                    ++it;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
