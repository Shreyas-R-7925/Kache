#pragma once
#include "StorageEngine.h"
#include "EvictionPolicy.h"
#include <string>
#include <unordered_map>
#include <optional>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>

using TimePoint = std::chrono::steady_clock::time_point;

class InMemoryStorage : public StorageEngine {
public:
    InMemoryStorage(size_t cap, const std::string& policyType);
    ~InMemoryStorage();

    std::optional<std::string> get(const std::string& key) override;
    void set(const std::string& key, const std::string& value) override;
    void set(const std::string& key, const std::string& value, std::chrono::seconds ttl) override;
    void del(const std::string& key) override;
    bool exists(const std::string& key) const override;

private:
    struct Entry {
        std::string value;
        std::optional<TimePoint> expiry;
    };

    mutable std::mutex mtx;
    std::thread cleanupThread;
    std::atomic<bool> stopFlag{false};

    std::unordered_map<std::string, Entry> store;

    std::unique_ptr<EvictionPolicy> policy;  
    size_t capacity;                         

    void cleanupExpiredKeys();
};