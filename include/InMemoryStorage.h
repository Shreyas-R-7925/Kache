#pragma once
#include "StorageEngine.h"
#include <string>
#include <unordered_map>

class InMemoryStorage : public StorageEngine {
public:
    ~InMemoryStorage() = default;

    std::string get(const std::string& key) const override;
    void set(const std::string& key, const std::string& value) override;
    void del(const std::string& key) override;
    bool exists(const std::string& key) const override;

private:
    std::unordered_map<std::string, std::string> store;
};
