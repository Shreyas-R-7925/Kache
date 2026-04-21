#pragma once

#include <cstddef>

namespace kache::constants {

inline constexpr int kDefaultCacheSize = 3;
inline constexpr int kServerPort = 6380;
inline constexpr int kServerBacklog = 16;
inline constexpr std::size_t kServerBufferSize = 4096;
inline constexpr std::size_t kShardCount = 16;

inline constexpr const char* kLruEvictionPolicy = "LRU";
inline constexpr const char* kLfuEvictionPolicy = "LFU";
inline constexpr const char* kEvictionPolicyFlag = "--eviction-policy";

inline constexpr const char* kDefaultWalFileName = "wal.log";
inline constexpr const char* kDefaultSnapshotFileName = "snapshot.rdb";
inline constexpr const char* kSnapshotHeader = "KACHE_SNAPSHOT_V1";

inline constexpr const char* kTempWalPrefix = "kache_wal";
inline constexpr const char* kTempSnapshotPrefix = "kache_snapshot";

}  // namespace kache::constants
