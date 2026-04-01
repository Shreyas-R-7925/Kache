#include "../include/InMemoryStorage.h"   
#include <stdexcept>
#include "constants/KacheConstants.h"
#include "../include/LRUPolicy.h"

InMemoryStorage::InMemoryStorage(size_t cap, const std::string& policyType) : capacity(cap) {

    if (policyType == LRU_EVICTION_POLICY) {
        policy = std::make_unique<LRUPolicy>();
    } else {
        throw std::invalid_argument("Unsupported eviction policy");
    }

    cleanupThread = std::thread(&InMemoryStorage::cleanupExpiredKeys, this);
}

InMemoryStorage::~InMemoryStorage() {
    stopFlag = true;

    if (cleanupThread.joinable()) {
        cleanupThread.join();
    }
}

std::optional<std::string> InMemoryStorage::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mtx);

    auto it = store.find(key);
    if (it == store.end()) return std::nullopt;

    auto now = std::chrono::steady_clock::now();

    if (it->second.expiry && now > it->second.expiry.value()) {
        policy->onDelete(key);
        store.erase(it);
        return std::nullopt;
    }

    policy->onGet(key); 
    return it->second.value;
}

void InMemoryStorage::set(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mtx);

    if (store.find(key) != store.end()) {
        store[key].value = value;
        policy->onSet(key);
        return;
    }

    if (store.size() >= capacity) {
        std::string evictKey = policy->evict();
        store.erase(evictKey);
    }

    store[key] = {value, std::nullopt};
    policy->onSet(key);
}

void InMemoryStorage::set(const std::string& key, const std::string& value, std::chrono::seconds ttl) {
    std::lock_guard<std::mutex> lock(mtx);

    TimePoint expiry = std::chrono::steady_clock::now() + ttl;

    if (store.find(key) != store.end()) {
        store[key] = {value, expiry};
        policy->onSet(key);
        return;
    }

    if (store.size() >= capacity) {
        std::string evictKey = policy->evict();
        store.erase(evictKey);
    }

    store[key] = {value, expiry};
    policy->onSet(key);
}

void InMemoryStorage::del(const std::string& key) {
    std::lock_guard<std::mutex> lock(mtx);

    store.erase(key);
    policy->onDelete(key);  
}

bool InMemoryStorage::exists(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mtx);

    auto it = store.find(key);
    if (it == store.end()) return false;

    auto now = std::chrono::steady_clock::now();

    if (it->second.expiry && now > it->second.expiry.value()) {
        return false;
    }

    return true;
}

void InMemoryStorage::cleanupExpiredKeys() {
    while (!stopFlag) {
        {
            std::lock_guard<std::mutex> lock(mtx);

            for (auto it = store.begin(); it != store.end();) {
                if (it->second.expiry &&
                    std::chrono::steady_clock::now() > it->second.expiry.value()) {

                    policy->onDelete(it->first); 
                    it = store.erase(it);

                } else {
                    ++it;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}