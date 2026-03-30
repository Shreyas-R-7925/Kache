#pragma once
#include "StorageEngine.h"
#include <string>
#include <unordered_map>
#include <optional>
#include <chrono>
#include <thread>

using TimePoint = std::chrono::steady_clock::time_point;

class InMemoryStorage : public StorageEngine {
public:
    InMemoryStorage(); 
    ~InMemoryStorage();

    std::optional<std::string> get(const std::string& key) const override;
    void set(const std::string& key, const std::string& value) override;
    void set(const std::string& key, const std::string& value, std::chrono::seconds ttl) override;
    void del(const std::string& key) override;
    bool exists(const std::string& key) const override;

private:

    mutable std::mutex mtx;              
    std::thread cleanupThread;          
    std::atomic<bool> stopFlag{false};  

    void cleanupExpiredKeys(); 

    struct Entry {
        std::string value;
        std::optional<TimePoint> expiry;
    };

    std::unordered_map<std::string, Entry> store;
    
};