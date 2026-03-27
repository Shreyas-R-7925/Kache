#include "../include/InMemoryStorage.h"

std::string InMemoryStorage::get(const std::string& key) const {
    return exists(key) ? store.at(key) : "";
}

void InMemoryStorage::set(const std::string& key, const std::string& value) {
    store[key] = value;
}

void InMemoryStorage::del(const std::string& key) {
    store.erase(key);
}

bool InMemoryStorage::exists(const std::string& key) const {
    return store.find(key) != store.end();
}