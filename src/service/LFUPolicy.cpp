#include "LFUPolicy.h"

#include <limits>
#include <stdexcept>

void LFUPolicy::onGet(const std::string& key) {
    auto it = entries_.find(key);
    if (it == entries_.end()) {
        return;
    }

    ++it->second.frequency;
    it->second.recency = ++clock_;
}

void LFUPolicy::onSet(const std::string& key) {
    auto [it, inserted] = entries_.try_emplace(key);
    if (inserted) {
        it->second.frequency = 1;
    } else {
        ++it->second.frequency;
    }

    it->second.recency = ++clock_;
}

void LFUPolicy::onDelete(const std::string& key) {
    entries_.erase(key);
}

std::string LFUPolicy::evict() {
    if (entries_.empty()) {
        throw std::runtime_error("Eviction on empty cache");
    }

    auto victim = entries_.begin();
    std::uint64_t lowestFrequency = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t oldestRecency = std::numeric_limits<std::uint64_t>::max();

    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
        const auto frequency = it->second.frequency;
        const auto recency = it->second.recency;

        if (frequency < lowestFrequency || (frequency == lowestFrequency && recency < oldestRecency)) {
            victim = it;
            lowestFrequency = frequency;
            oldestRecency = recency;
        }
    }

    const std::string key = victim->first;
    entries_.erase(victim);
    return key;
}
