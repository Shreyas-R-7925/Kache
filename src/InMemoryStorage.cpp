#include "../include/InMemoryStorage.h"

InMemoryStorage::InMemoryStorage() {
    cleanupThread = std::thread(&InMemoryStorage::cleanupExpiredKeys, this);
}

InMemoryStorage::~InMemoryStorage() {
    stopFlag = true;

    if (cleanupThread.joinable()) {
        cleanupThread.join();
    }
}

std::optional<std::string> InMemoryStorage::get(const std::string& key) const {

    std::lock_guard<std::mutex> lock(mtx); 

    auto it = store.find(key);
    if (it == store.end()) {
        return std::nullopt;
    }

    if (it->second.expiry && std::chrono::steady_clock::now() > it->second.expiry.value()) {
        return std::nullopt;
    }

    return it->second.value;
}

void InMemoryStorage::set(const std::string& key, const std::string& value) {

    std::lock_guard<std::mutex> lock(mtx);

    store[key] = {value, std::nullopt};
}

void InMemoryStorage::set(const std::string& key, const std::string& value, std::chrono::seconds ttl) {

    std::lock_guard<std::mutex> lock(mtx); 

    TimePoint expiry = std::chrono::steady_clock::now() + ttl;
    store[key] = {value, expiry};
}

void InMemoryStorage::del(const std::string& key) {

    std::lock_guard<std::mutex> lock(mtx); 

    store.erase(key);
}

bool InMemoryStorage::exists(const std::string& key) const {

    std::lock_guard<std::mutex> lock(mtx); 

    auto it = store.find(key);
    if (it == store.end()) {
        return false;
    }

    if (it->second.expiry && std::chrono::steady_clock::now() > it->second.expiry.value()) {
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
                    it = store.erase(it);
                } else {
                    ++it;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}