# Kache

Kache is a Redis-compatible in-memory key-value store written in C++. It exposes a RESP-speaking TCP server on `127.0.0.1:6380`, supports pluggable eviction policies (`LRU` and `LFU`), handles TTL expiry on access, and persists writes through a write-ahead log plus snapshot recovery.

## Features

- Redis-compatible TCP server on port `6380`
- RESP parsing for inline and array-style requests
- Supported commands: `SET`, `GET`, `DEL`, `EXISTS`, `EXPIRE`, `TTL`, `KEYS *`, `FLUSHALL`, `BGSAVE`
- Thread-safe sharded in-memory storage using `16` shards
- Pluggable eviction policies through a strategy interface
- TTL support with lazy expiry checks on reads and metadata commands
- Write-ahead log persistence in `wal.log`
- Snapshot persistence in `snapshot.rdb`
- Restart recovery by loading snapshot first and replaying WAL second

## Architecture

The request path is:

`TCP server -> RESP parser -> CommandHandler -> StorageEngine -> RESP serializer -> TCP response`

Main components:

- `TCPServer`: accepts client connections and runs the socket request/response loop
- `RespParser`: converts raw bytes into command tokens
- `CommandHandler`: validates command shape and dispatches to storage
- `InMemoryStorage`: sharded, thread-safe storage implementation
- `EvictionPolicy`: strategy interface used by `LRU` and `LFU`
- `PersistenceManager`: WAL append, snapshot write, snapshot load, WAL replay

## Supported Commands

| Command | Behavior |
| --- | --- |
| `SET key value` | Store or overwrite a value |
| `GET key` | Return the value or null bulk string |
| `DEL key [key ...]` | Delete one or more keys |
| `EXISTS key [key ...]` | Count keys that exist |
| `EXPIRE key seconds` | Attach a TTL in seconds |
| `TTL key` | Return remaining TTL, `-1`, or `-2` |
| `KEYS *` | Return all live keys |
| `FLUSHALL` | Clear the in-memory dataset |
| `BGSAVE` | Write a snapshot and truncate the WAL |

## Concurrency Model

- The TCP server handles each client connection in its own worker thread.
- Storage is split into `16` shards.
- Each shard owns its own `std::shared_mutex`.
- A small metadata mutex protects global size accounting and eviction bookkeeping.

This keeps read/write contention lower than a single global storage lock while preserving consistent eviction state.

## Eviction Policies

Kache supports two startup-selectable eviction policies:

- `LRU`: evict the least recently used key
- `LFU`: evict the least frequently used key, with recency as a tie-breaker

Start the server with either policy:

```bash
./build/kache_server --eviction-policy lru
./build/kache_server --eviction-policy lfu
```

If no flag is passed, the server defaults to `LRU`.

## TTL Behavior

TTL is supported through `EXPIRE` and `TTL`.

- Expiry timestamps are stored as absolute time points
- Expired keys are purged when touched by operations such as `GET`, `EXISTS`, and `TTL`
- Snapshot and WAL recovery preserve TTL state across restarts

Important: expiry is currently lazy. There is no background active expiry sweeper in the current implementation.

## Persistence Model

Kache keeps the live dataset in memory and uses persistence for restart recovery.

### WAL

- Write commands are appended to `wal.log`
- TTL writes are persisted as `SET` followed by `EXPIREAT`
- `FLUSHALL` is also written to the WAL

Example WAL contents:

```text
SET foo bar
SET temp value
EXPIREAT temp 1776771002045
DEL foo
```

### Snapshot

- `BGSAVE` writes the current live dataset to `snapshot.rdb`
- Snapshot format is a simple line-based format owned by this project
- The first line is a format header: `KACHE_SNAPSHOT_V1`
- Each remaining line stores `key value expiry_epoch_ms_or_-1`

Example snapshot:

```text
KACHE_SNAPSHOT_V1
alpha 1 -1
temp value 1776771002045
```

### Recovery Order

On startup Kache restores state in this order:

1. Load `snapshot.rdb`
2. Replay `wal.log`

### Current Persistence Notes

- `BGSAVE` is currently synchronous despite the Redis-like command name
- Eviction metadata is not persisted
- Restart recovery restores keys and TTLs, but not exact pre-restart LRU/LFU history

## Project Layout

```text
Kache/
├── include/
│   ├── parser/
│   ├── server/
│   ├── CommandHandler.h
│   ├── EvictionPolicy.h
│   ├── InMemoryStorage.h
│   ├── LFUPolicy.h
│   ├── LRUPolicy.h
│   ├── PersistenceManager.h
│   └── StorageEngine.h
├── src/
│   ├── constants/
│   ├── server/
│   └── service/
├── tests/
├── bench/
├── CMakeLists.txt
├── DESIGN.md
└── README.md
```

## Build

### Prerequisites

- C++17 or newer
- CMake 3.10+
- `redis-cli` for manual verification
- `redis-benchmark` for network benchmarks

### Build Commands

```bash
cmake -S . -B build
cmake --build build
```

## Run

Start the server:

```bash
./build/kache_server
```

The server listens on:

```text
127.0.0.1:6380
```

Start with a specific eviction policy:

```bash
./build/kache_server --eviction-policy lru
./build/kache_server --eviction-policy lfu
```

## Testing

Run the automated test binary:

```bash
ctest --test-dir build --output-on-failure
./build/kache_tests
```

Covered areas include:

- inline RESP parsing
- array RESP parsing
- RESP serialization
- command dispatch behavior
- expiry and TTL behavior
- `KEYS *` and `FLUSHALL`
- concurrent `SET` and `GET` stress
- LRU read-refresh behavior
- LRU vs LFU eviction differences
- WAL replay restore
- snapshot restore through `BGSAVE`

## Manual Verification

Start the server in one terminal:

```bash
./build/kache_server
```

Then run these commands in another terminal:

```bash
redis-cli -p 6380 SET foo bar
redis-cli -p 6380 --raw GET foo
redis-cli -p 6380 EXISTS foo
redis-cli -p 6380 EXPIRE foo 10
redis-cli -p 6380 TTL foo
redis-cli -p 6380 KEYS '*'
redis-cli -p 6380 DEL foo
redis-cli -p 6380 FLUSHALL
```

Expected results:

- `SET foo bar` returns `OK`
- `GET foo` returns `bar`
- `EXISTS foo` returns `1`
- `EXPIRE foo 10` returns `1`
- `TTL foo` returns a value near `10`
- `KEYS '*'` includes `foo`
- `DEL foo` returns `1`
- `FLUSHALL` returns `OK`

## Persistence Checks

### WAL Check

Remove any old persistence files:

```bash
rm -f wal.log snapshot.rdb
```

Start the server, then:

```bash
redis-cli -p 6380 SET foo bar
redis-cli -p 6380 SET baz qux
redis-cli -p 6380 DEL baz
cat wal.log
```

Expected WAL contents:

```text
SET foo bar
SET baz qux
DEL baz
```

Restart the server and verify:

```bash
redis-cli -p 6380 --raw GET foo
redis-cli -p 6380 EXISTS baz
```

Expected:

- `GET foo` returns `bar`
- `EXISTS baz` returns `0`

### Snapshot Check

Start the server, then:

```bash
redis-cli -p 6380 SET alpha 1
redis-cli -p 6380 SET beta 2
redis-cli -p 6380 BGSAVE
cat snapshot.rdb
```

Restart the server and verify:

```bash
redis-cli -p 6380 --raw GET alpha
redis-cli -p 6380 --raw GET beta
```

Expected:

- `GET alpha` returns `1`
- `GET beta` returns `2`

### TTL Persistence Check

```bash
rm -f wal.log snapshot.rdb
./build/kache_server
```

Then:

```bash
redis-cli -p 6380 SET temp value
redis-cli -p 6380 EXPIRE temp 30
redis-cli -p 6380 BGSAVE
```

Restart the server and verify:

```bash
redis-cli -p 6380 TTL temp
redis-cli -p 6380 --raw GET temp
```

Expected:

- `TTL temp` returns a positive value
- `GET temp` returns `value` until the key expires

## Benchmarking

The exact numbers you report depend on machine, compiler flags, and dataset size.

Important: the current source default cache capacity is `3` in `src/constants/KacheConstants.h`. That is useful for eviction testing, but it is not a realistic benchmark capacity. For meaningful network benchmarks, increase the default capacity or expose capacity as a runtime flag before benchmarking large key ranges.

Baseline benchmark flow:

1. Start the server
2. Clear existing data
3. Preload keys for `GET`
4. Run `GET` and `SET` with `4` concurrent clients

Commands:

```bash
./build/kache_server --eviction-policy lru
redis-cli -p 6380 FLUSHALL
redis-benchmark -p 6380 -t set -n 100000 -r 100000 -c 8 -q
redis-benchmark -p 6380 -t get -n 100000 -c 4 -r 100000 --csv
redis-benchmark -p 6380 -t set -n 100000 -c 4 -r 100000 --csv
```

For steadier numbers, repeat each benchmark three times and average throughput:

```bash
redis-benchmark -p 6380 -t get -n 100000 -c 4 -r 100000 --csv
redis-benchmark -p 6380 -t get -n 100000 -c 4 -r 100000 --csv
redis-benchmark -p 6380 -t get -n 100000 -c 4 -r 100000 --csv

redis-benchmark -p 6380 -t set -n 100000 -c 4 -r 100000 --csv
redis-benchmark -p 6380 -t set -n 100000 -c 4 -r 100000 --csv
redis-benchmark -p 6380 -t set -n 100000 -c 4 -r 100000 --csv
```

## Limitations

- `KEYS` currently supports only `*`
- `BGSAVE` is synchronous
- Snapshot format is project-specific, not Redis RDB-compatible
- Eviction metadata is not persisted across restarts
- Expiry is lazy; there is no background active expiry loop
- Default cache capacity is currently tuned for eviction tests, not production-like workloads

## Author

Shreyas R
