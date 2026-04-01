#include "LRUPolicy.h"
#include <stdexcept>

void LRUPolicy::onGet(const std::string& key) {
    auto it = pos.find(key);
    if (it == pos.end()) return;

    lruList.erase(it->second);
    lruList.push_front(key);
    pos[key] = lruList.begin();
}

void LRUPolicy::onSet(const std::string& key) {
    auto it = pos.find(key);
    if (it != pos.end()) {
        lruList.erase(it->second);
    }

    lruList.push_front(key);
    pos[key] = lruList.begin();
}

void LRUPolicy::onDelete(const std::string& key) {
    auto it = pos.find(key);
    if (it == pos.end()) return;

    lruList.erase(it->second);
    pos.erase(it);
}

std::string LRUPolicy::evict() {
    if (lruList.empty()) {
        throw std::runtime_error("Eviction on empty cache");
    }

    std::string key = lruList.back();
    lruList.pop_back();
    pos.erase(key);
    return key;
}