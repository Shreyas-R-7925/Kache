#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

class StorageEngine {
public:
    virtual ~StorageEngine() = default;

    virtual std::optional<std::string> get(const std::string& key) = 0;
    virtual void set(const std::string& key, const std::string& value) = 0;
    virtual void set(const std::string& key, const std::string& value, std::chrono::seconds ttl) = 0;
    virtual bool del(const std::string& key) = 0;
    virtual bool exists(const std::string& key) = 0;
    virtual bool expire(const std::string& key, std::chrono::seconds ttl) = 0;
    virtual long ttl(const std::string& key) = 0;
    virtual std::vector<std::string> keys(const std::string& pattern) = 0;
    virtual void flushAll() = 0;
};
