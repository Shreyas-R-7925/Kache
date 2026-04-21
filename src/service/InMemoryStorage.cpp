#include "InMemoryStorage.h"

#include <algorithm>
#include <functional>
#include <sstream>
#include <stdexcept>

#include "../constants/KacheConstants.h"
#include "LRUPolicy.h"

InMemoryStorage::InMemoryStorage(
    size_t cap,
    const std::string& policyType,
    std::string walPath,
    std::string snapshotPath)
    : persistenceManager_(std::move(walPath), std::move(snapshotPath)), capacity_(cap) {
    if (policyType == LRU_EVICTION_POLICY) {
        policy_ = std::make_unique<LRUPolicy>();
    } else {
        throw std::invalid_argument("Unsupported eviction policy");
    }

    restoreFromPersistence();
}

std::optional<std::string> InMemoryStorage::get(const std::string& key) {
    const auto now = std::chrono::system_clock::now();
    Shard& shard = shardFor(key);
    std::lock_guard<std::mutex> metadataLock(metadataMutex_);
    std::unique_lock<std::shared_mutex> shardLock(shard.mutex);
    auto it = shard.store.find(key);
    if (it == shard.store.end()) {
        return std::nullopt;
    }

    if (isExpired(it->second, now)) {
        purgeIfExpiredLocked(shard, key, now);
        return std::nullopt;
    }

    policy_->onGet(key);
    return it->second.value;
}

void InMemoryStorage::set(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> metadataLock(metadataMutex_);
    appendWalCommand({"SET", key, value});
    setInternalLocked(key, value, std::nullopt);
}

void InMemoryStorage::set(const std::string& key, const std::string& value, std::chrono::seconds ttl) {
    const auto expiry = std::chrono::system_clock::now() + ttl;
    std::lock_guard<std::mutex> metadataLock(metadataMutex_);
    appendWalCommand({"SET", key, value});
    appendWalCommand({
        "EXPIREAT",
        key,
        std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(expiry.time_since_epoch()).count())});
    setInternalLocked(key, value, expiry);
}

bool InMemoryStorage::del(const std::string& key) {
    std::lock_guard<std::mutex> metadataLock(metadataMutex_);
    appendWalCommand({"DEL", key});
    Shard& shard = shardFor(key);
    std::unique_lock<std::shared_mutex> shardLock(shard.mutex);
    return eraseLocked(shard, key);
}

bool InMemoryStorage::exists(const std::string& key) {
    const auto now = std::chrono::system_clock::now();
    Shard& shard = shardFor(key);

    {
        std::shared_lock<std::shared_mutex> shardLock(shard.mutex);
        auto it = shard.store.find(key);
        if (it == shard.store.end()) {
            return false;
        }

        if (!isExpired(it->second, now)) {
            return true;
        }
    }

    std::lock_guard<std::mutex> metadataLock(metadataMutex_);
    std::unique_lock<std::shared_mutex> shardLock(shard.mutex);
    purgeIfExpiredLocked(shard, key, now);
    return shard.store.find(key) != shard.store.end();
}

bool InMemoryStorage::expire(const std::string& key, std::chrono::seconds ttl) {
    const auto now = std::chrono::system_clock::now();
    const auto expiry = now + ttl;
    Shard& shard = shardFor(key);
    std::lock_guard<std::mutex> metadataLock(metadataMutex_);
    appendWalCommand({
        "EXPIREAT",
        key,
        std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(expiry.time_since_epoch()).count())});
    std::unique_lock<std::shared_mutex> shardLock(shard.mutex);

    if (purgeIfExpiredLocked(shard, key, now)) {
        return false;
    }

    auto it = shard.store.find(key);
    if (it == shard.store.end()) {
        return false;
    }

    it->second.expiry = expiry;
    return true;
}

long InMemoryStorage::ttl(const std::string& key) {
    const auto now = std::chrono::system_clock::now();
    Shard& shard = shardFor(key);

    {
        std::shared_lock<std::shared_mutex> shardLock(shard.mutex);
        auto it = shard.store.find(key);
        if (it == shard.store.end()) {
            return -2;
        }

        if (!isExpired(it->second, now)) {
            if (!it->second.expiry.has_value()) {
                return -1;
            }

            const auto remaining = std::chrono::duration_cast<std::chrono::seconds>(it->second.expiry.value() - now).count();
            return remaining > 0 ? remaining : 0;
        }
    }

    std::lock_guard<std::mutex> metadataLock(metadataMutex_);
    std::unique_lock<std::shared_mutex> shardLock(shard.mutex);
    purgeIfExpiredLocked(shard, key, now);
    auto it = shard.store.find(key);
    if (it == shard.store.end()) {
        return -2;
    }

    if (!it->second.expiry.has_value()) {
        return -1;
    }

    const auto remaining = std::chrono::duration_cast<std::chrono::seconds>(it->second.expiry.value() - now).count();
    return remaining > 0 ? remaining : 0;
}

std::vector<std::string> InMemoryStorage::keys(const std::string& pattern) {
    if (pattern != "*") {
        throw std::invalid_argument("Only KEYS * is supported");
    }

    std::vector<std::string> result;
    for (const auto& shard : shards_) {
        std::shared_lock<std::shared_mutex> shardLock(shard.mutex);
        for (const auto& [key, entry] : shard.store) {
            if (!isExpired(entry, std::chrono::system_clock::now())) {
                result.push_back(key);
            }
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

void InMemoryStorage::flushAll() {
    std::scoped_lock metadataLock(metadataMutex_);
    appendWalCommand({"FLUSHALL"});

    for (auto& shard : shards_) {
        std::unique_lock<std::shared_mutex> shardLock(shard.mutex);
        for (const auto& [key, _] : shard.store) {
            policy_->onDelete(key);
        }
        shard.store.clear();
    }

    size_ = 0;
}

void InMemoryStorage::bgsave() {
    std::lock_guard<std::mutex> metadataLock(metadataMutex_);
    const auto now = std::chrono::system_clock::now();
    persistenceManager_.writeSnapshot(snapshotRecordsLocked(now));
    persistenceManager_.truncateWal();
}

bool InMemoryStorage::isExpired(const Entry& entry, TimePoint now) const {
    return entry.expiry.has_value() && now >= entry.expiry.value();
}

size_t InMemoryStorage::shardIndexFor(const std::string& key) const {
    return std::hash<std::string>{}(key) % kShardCount;
}

InMemoryStorage::Shard& InMemoryStorage::shardFor(const std::string& key) {
    return shards_[shardIndexFor(key)];
}

const InMemoryStorage::Shard& InMemoryStorage::shardFor(const std::string& key) const {
    return shards_[shardIndexFor(key)];
}

bool InMemoryStorage::purgeIfExpiredLocked(Shard& shard, const std::string& key, TimePoint now) {
    auto it = shard.store.find(key);
    if (it == shard.store.end() || !isExpired(it->second, now)) {
        return false;
    }

    policy_->onDelete(key);
    shard.store.erase(it);
    --size_;
    return true;
}

bool InMemoryStorage::eraseLocked(Shard& shard, const std::string& key) {
    auto it = shard.store.find(key);
    if (it == shard.store.end()) {
        return false;
    }

    policy_->onDelete(key);
    shard.store.erase(it);
    --size_;
    return true;
}

void InMemoryStorage::setInternal(const std::string& key, const std::string& value, std::optional<TimePoint> expiry) {
    std::lock_guard<std::mutex> metadataLock(metadataMutex_);
    setInternalLocked(key, value, expiry);
}

void InMemoryStorage::setInternalLocked(const std::string& key, const std::string& value, std::optional<TimePoint> expiry) {
    Shard& shard = shardFor(key);
    std::unique_lock<std::shared_mutex> shardLock(shard.mutex);

    const auto now = std::chrono::system_clock::now();
    purgeIfExpiredLocked(shard, key, now);

    auto it = shard.store.find(key);
    const bool isInsert = it == shard.store.end();

    if (isInsert && size_ >= capacity_) {
        const std::string evictedKey = policy_->evict();
        Shard& evictedShard = shardFor(evictedKey);
        if (&evictedShard == &shard) {
            eraseLocked(shard, evictedKey);
        } else {
            shardLock.unlock();
            std::unique_lock<std::shared_mutex> evictedShardLock(evictedShard.mutex);
            eraseLocked(evictedShard, evictedKey);
            shardLock.lock();
        }
    }

    if (isInsert) {
        ++size_;
    }

    shard.store[key] = Entry{value, expiry};
    policy_->onSet(key);
}

void InMemoryStorage::restoreFromPersistence() {
    replaying_ = true;

    for (const auto& record : persistenceManager_.loadSnapshot()) {
        restoreSet(record.key, record.value, record.expiry);
    }

    for (const auto& command : persistenceManager_.loadWal()) {
        if (command.empty()) {
            continue;
        }

        if (command[0] == "SET" && command.size() == 3) {
            restoreSet(command[1], command[2], std::nullopt);
            continue;
        }

        if (command[0] == "DEL" && command.size() == 2) {
            del(command[1]);
            continue;
        }

        if (command[0] == "EXPIREAT" && command.size() == 3) {
            expireAt(command[1], TimePoint(std::chrono::milliseconds(std::stoll(command[2]))));
            continue;
        }

        if (command[0] == "FLUSHALL" && command.size() == 1) {
            flushAll();
        }
    }

    replaying_ = false;
}

void InMemoryStorage::appendWalCommand(const std::vector<std::string>& tokens) {
    if (!replaying_) {
        persistenceManager_.appendCommand(tokens);
    }
}

void InMemoryStorage::restoreSet(const std::string& key, const std::string& value, std::optional<TimePoint> expiry) {
    setInternal(key, value, expiry);
}

void InMemoryStorage::expireAt(const std::string& key, TimePoint expiry) {
    std::lock_guard<std::mutex> metadataLock(metadataMutex_);
    Shard& shard = shardFor(key);
    std::unique_lock<std::shared_mutex> shardLock(shard.mutex);

    const auto now = std::chrono::system_clock::now();
    if (purgeIfExpiredLocked(shard, key, now)) {
        return;
    }

    auto it = shard.store.find(key);
    if (it == shard.store.end()) {
        return;
    }

    it->second.expiry = expiry;
}

std::vector<PersistenceManager::SnapshotRecord> InMemoryStorage::snapshotRecordsLocked(TimePoint now) const {
    std::vector<PersistenceManager::SnapshotRecord> records;

    for (const auto& shard : shards_) {
        std::shared_lock<std::shared_mutex> shardLock(shard.mutex);
        for (const auto& [key, entry] : shard.store) {
            if (isExpired(entry, now)) {
                continue;
            }

            records.push_back(PersistenceManager::SnapshotRecord{key, entry.value, entry.expiry});
        }
    }

    std::sort(records.begin(), records.end(), [](const auto& left, const auto& right) {
        return left.key < right.key;
    });
    return records;
}
