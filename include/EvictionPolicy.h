#pragma once
#include <string>

class EvictionPolicy {
public:
    virtual ~EvictionPolicy() = default;

    virtual void onGet(const std::string& key) = 0;
    virtual void onSet(const std::string& key) = 0;
    virtual void onDelete(const std::string& key) = 0;

    virtual std::string evict() = 0;
};