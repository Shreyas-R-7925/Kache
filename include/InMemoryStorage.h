#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "EvictionPolicy.h"
#include "StorageEngine.h"

using TimePoint = std::chrono::steady_clock::time_point;

class InMemoryStorage : public StorageEngine {
public:
    InMemoryStorage(size_t cap, const std::string& policyType);
    ~InMemoryStorage() override;

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
    struct Entry {
        std::string value;
        std::optional<TimePoint> expiry;
    };

    mutable std::mutex mutex_;
    std::thread cleanupThread_;
    std::atomic<bool> stopFlag_{false};
    std::unordered_map<std::string, Entry> store_;
    std::unique_ptr<EvictionPolicy> policy_;
    size_t capacity_;

    bool isExpired(const Entry& entry, TimePoint now) const;
    bool purgeIfExpiredLocked(const std::string& key, TimePoint now);
    void setInternalLocked(const std::string& key, const std::string& value, std::optional<TimePoint> expiry);
    void cleanupExpiredKeys();
};
