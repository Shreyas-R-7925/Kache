#pragma once

#include <array>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "EvictionPolicy.h"
#include "StorageEngine.h"

using TimePoint = std::chrono::steady_clock::time_point;

class InMemoryStorage : public StorageEngine {
public:
    InMemoryStorage(size_t cap, const std::string& policyType);
    ~InMemoryStorage() override = default;

    std::optional<std::string> get(const std::string& key) override;
    void set(const std::string& key, const std::string& value) override;
    void set(const std::string& key, const std::string& value, std::chrono::seconds ttl) override;
    bool del(const std::string& key) override;
    bool exists(const std::string& key) override;
    bool expire(const std::string& key, std::chrono::seconds ttl) override;
    long ttl(const std::string& key) override;
    std::vector<std::string> keys(const std::string& pattern) override;
    void flushAll() override;

private:
    static constexpr size_t kShardCount = 16;

    struct Entry {
        std::string value;
        std::optional<TimePoint> expiry;
    };

    struct Shard {
        mutable std::shared_mutex mutex;
        std::unordered_map<std::string, Entry> store;
    };

    std::array<Shard, kShardCount> shards_;
    mutable std::mutex metadataMutex_;
    std::unique_ptr<EvictionPolicy> policy_;
    size_t capacity_;
    size_t size_ = 0;

    bool isExpired(const Entry& entry, TimePoint now) const;
    size_t shardIndexFor(const std::string& key) const;
    Shard& shardFor(const std::string& key);
    const Shard& shardFor(const std::string& key) const;
    bool purgeIfExpiredLocked(Shard& shard, const std::string& key, TimePoint now);
    bool eraseLocked(Shard& shard, const std::string& key);
    void setInternal(const std::string& key, const std::string& value, std::optional<TimePoint> expiry);
};
