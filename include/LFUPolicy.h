#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "EvictionPolicy.h"

class LFUPolicy : public EvictionPolicy {
private:
    struct Entry {
        std::uint64_t frequency = 0;
        std::uint64_t recency = 0;
    };

    std::unordered_map<std::string, Entry> entries_;
    std::uint64_t clock_ = 0;

public:
    void onGet(const std::string& key) override;
    void onSet(const std::string& key) override;
    void onDelete(const std::string& key) override;
    std::string evict() override;
};
