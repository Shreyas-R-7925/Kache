#pragma once
#include "EvictionPolicy.h"
#include <list>
#include <unordered_map>
#include <stdexcept>

class LRUPolicy : public EvictionPolicy {
private:
    std::list<std::string> lruList;
    std::unordered_map<std::string, std::list<std::string>::iterator> pos;

public:
    void onGet(const std::string& key) override;
    void onSet(const std::string& key) override;
    void onDelete(const std::string& key) override;
    std::string evict() override;
};