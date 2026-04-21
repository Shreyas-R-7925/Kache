#include "InMemoryStorage.h"

#include <algorithm>
#include <functional>
#include <stdexcept>

#include "../constants/KacheConstants.h"
#include "LRUPolicy.h"

InMemoryStorage::InMemoryStorage(size_t cap, const std::string& policyType) : capacity_(cap) {
    if (policyType == LRU_EVICTION_POLICY) {
        policy_ = std::make_unique<LRUPolicy>();
    } else {
        throw std::invalid_argument("Unsupported eviction policy");
    }
}

std::optional<std::string> InMemoryStorage::get(const std::string& key) {
    const auto now = std::chrono::steady_clock::now();
    Shard& shard = shardFor(key);
    std::lock_guard<std::mutex> metadataLock(metadataMutex_);
    std::unique_lock<std::shared_mutex> shardLock(shard.mutex);
    auto it = shard.store.find(key);
    if (it == shard.store.end()) {
        return std::nullopt;
    }

    if (isExpired(it->second, now)) {
        purgeIfExpiredLocked(shard, key, now);
        return std::nullopt;
    }

    policy_->onGet(key);
    return it->second.value;
}

void InMemoryStorage::set(const std::string& key, const std::string& value) {
    setInternal(key, value, std::nullopt);
}

void InMemoryStorage::set(const std::string& key, const std::string& value, std::chrono::seconds ttl) {
    setInternal(key, value, std::chrono::steady_clock::now() + ttl);
}

bool InMemoryStorage::del(const std::string& key) {
    std::lock_guard<std::mutex> metadataLock(metadataMutex_);
    Shard& shard = shardFor(key);
    std::unique_lock<std::shared_mutex> shardLock(shard.mutex);
    return eraseLocked(shard, key);
}

bool InMemoryStorage::exists(const std::string& key) {
    const auto now = std::chrono::steady_clock::now();
    Shard& shard = shardFor(key);

    {
        std::shared_lock<std::shared_mutex> shardLock(shard.mutex);
        auto it = shard.store.find(key);
        if (it == shard.store.end()) {
            return false;
        }

        if (!isExpired(it->second, now)) {
            return true;
        }
    }

    std::lock_guard<std::mutex> metadataLock(metadataMutex_);
    std::unique_lock<std::shared_mutex> shardLock(shard.mutex);
    purgeIfExpiredLocked(shard, key, now);
    return shard.store.find(key) != shard.store.end();
}

bool InMemoryStorage::expire(const std::string& key, std::chrono::seconds ttl) {
    const auto now = std::chrono::steady_clock::now();
    Shard& shard = shardFor(key);
    std::lock_guard<std::mutex> metadataLock(metadataMutex_);
    std::unique_lock<std::shared_mutex> shardLock(shard.mutex);

    if (purgeIfExpiredLocked(shard, key, now)) {
        return false;
    }

    auto it = shard.store.find(key);
    if (it == shard.store.end()) {
        return false;
    }

    it->second.expiry = now + ttl;
    return true;
}

long InMemoryStorage::ttl(const std::string& key) {
    const auto now = std::chrono::steady_clock::now();
    Shard& shard = shardFor(key);

    {
        std::shared_lock<std::shared_mutex> shardLock(shard.mutex);
        auto it = shard.store.find(key);
        if (it == shard.store.end()) {
            return -2;
        }

        if (!isExpired(it->second, now)) {
            if (!it->second.expiry.has_value()) {
                return -1;
            }

            const auto remaining = std::chrono::duration_cast<std::chrono::seconds>(it->second.expiry.value() - now).count();
            return remaining > 0 ? remaining : 0;
        }
    }

    std::lock_guard<std::mutex> metadataLock(metadataMutex_);
    std::unique_lock<std::shared_mutex> shardLock(shard.mutex);
    purgeIfExpiredLocked(shard, key, now);
    auto it = shard.store.find(key);
    if (it == shard.store.end()) {
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

    std::vector<std::string> result;
    for (const auto& shard : shards_) {
        std::shared_lock<std::shared_mutex> shardLock(shard.mutex);
        for (const auto& [key, entry] : shard.store) {
            if (!isExpired(entry, std::chrono::steady_clock::now())) {
                result.push_back(key);
            }
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

void InMemoryStorage::flushAll() {
    std::scoped_lock metadataLock(metadataMutex_);

    for (auto& shard : shards_) {
        std::unique_lock<std::shared_mutex> shardLock(shard.mutex);
        for (const auto& [key, _] : shard.store) {
            policy_->onDelete(key);
        }
        shard.store.clear();
    }

    size_ = 0;
}

bool InMemoryStorage::isExpired(const Entry& entry, TimePoint now) const {
    return entry.expiry.has_value() && now >= entry.expiry.value();
}

size_t InMemoryStorage::shardIndexFor(const std::string& key) const {
    return std::hash<std::string>{}(key) % kShardCount;
}

InMemoryStorage::Shard& InMemoryStorage::shardFor(const std::string& key) {
    return shards_[shardIndexFor(key)];
}

const InMemoryStorage::Shard& InMemoryStorage::shardFor(const std::string& key) const {
    return shards_[shardIndexFor(key)];
}

bool InMemoryStorage::purgeIfExpiredLocked(Shard& shard, const std::string& key, TimePoint now) {
    auto it = shard.store.find(key);
    if (it == shard.store.end() || !isExpired(it->second, now)) {
        return false;
    }

    policy_->onDelete(key);
    shard.store.erase(it);
    --size_;
    return true;
}

bool InMemoryStorage::eraseLocked(Shard& shard, const std::string& key) {
    auto it = shard.store.find(key);
    if (it == shard.store.end()) {
        return false;
    }

    policy_->onDelete(key);
    shard.store.erase(it);
    --size_;
    return true;
}

void InMemoryStorage::setInternal(const std::string& key, const std::string& value, std::optional<TimePoint> expiry) {
    std::lock_guard<std::mutex> metadataLock(metadataMutex_);
    Shard& shard = shardFor(key);
    std::unique_lock<std::shared_mutex> shardLock(shard.mutex);

    const auto now = std::chrono::steady_clock::now();
    purgeIfExpiredLocked(shard, key, now);

    auto it = shard.store.find(key);
    const bool isInsert = it == shard.store.end();

    if (isInsert && size_ >= capacity_) {
        const std::string evictedKey = policy_->evict();
        Shard& evictedShard = shardFor(evictedKey);
        if (&evictedShard == &shard) {
            eraseLocked(shard, evictedKey);
        } else {
            shardLock.unlock();
            std::unique_lock<std::shared_mutex> evictedShardLock(evictedShard.mutex);
            eraseLocked(evictedShard, evictedKey);
            shardLock.lock();
        }
    }

    if (isInsert) {
        ++size_;
    }

    shard.store[key] = Entry{value, expiry};
    policy_->onSet(key);
}
