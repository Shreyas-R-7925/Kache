# Kache

Kache is a lightweight in-memory key-value store inspired by Redis. It exposes a Redis-compatible TCP interface on port `6380`, parses RESP requests, dispatches commands through a storage engine, and returns RESP responses that work with `redis-cli`.

## Current Capabilities

- In-memory key-value storage
- Redis-compatible TCP server on port `6380`
- RESP parsing for:
  - inline commands like `SET foo bar`
  - array commands like `*3\r\n$3\r\nSET...`
- RESP serialization for:
  - simple strings
  - bulk strings
  - errors
  - integers
  - arrays
  - null bulk strings
- Thread-safe sharded in-memory storage
- Configurable `LRU` or `LFU` eviction
- Registry-based eviction policy selection
- TTL and expiry support
- WAL persistence through `wal.log`
- Snapshot persistence through `snapshot.rdb`
- Snapshot restore + WAL replay on startup

## Supported Commands

- `SET key value`
- `GET key`
- `DEL key [key ...]`
- `EXISTS key [key ...]`
- `EXPIRE key seconds`
- `TTL key`
- `KEYS *`
- `FLUSHALL`
- `BGSAVE`

## Project Structure

```text
Kache/
├── include/
│   ├── CommandHandler.h
│   ├── InMemoryStorage.h
│   ├── PersistenceManager.h
│   ├── StorageEngine.h
│   ├── parser/
│   └── server/
├── src/
│   ├── constants/
│   ├── server/
│   └── service/
├── tests/
├── CMakeLists.txt
├── DESIGN.md
└── README.md
```

## Request Flow

When a Redis client sends a command, the code path is:

`TCP server -> RESP parser -> CommandHandler -> StorageEngine -> RESP serializer -> TCP response`

Example:

```text
redis-cli -p 6380 EXISTS foo
```

This becomes a RESP request, gets parsed into tokens like `["EXISTS", "foo"]`, is dispatched through `CommandHandler`, checked in `InMemoryStorage`, serialized into an integer RESP reply, and sent back to the client.

## Concurrency Model

- The TCP server accepts clients and handles each connection in its own worker thread.
- `InMemoryStorage` uses `16` shards.
- Each shard has its own `std::shared_mutex`.
- A small metadata mutex protects shared eviction bookkeeping and size accounting.

This reduces contention compared with one global lock while keeping the LRU path correct.

## Persistence Model

Kache keeps the live dataset in memory. Persistence is used for restart recovery.

### WAL

- Every write command is appended to `wal.log`.
- Example entries:

```text
SET foo bar
DEL baz
EXPIREAT temp 1776771002045
FLUSHALL
```

- On startup, Kache replays the WAL to rebuild the latest state.
- WAL stores write history, not a final materialized cache image.
- Runtime eviction metadata such as LRU recency and LFU counters is not persisted.
- Because of that, restart recovery rebuilds keys and then applies eviction again from replayed writes.

### Snapshot

- `BGSAVE` writes the current in-memory dataset to `snapshot.rdb`.
- The snapshot format is a simple line-based format:

```text
KACHE_SNAPSHOT_V1
alpha 1 -1
temp value 1776771002045
```

- `-1` means the key has no expiry.
- Any other value is an absolute expiry timestamp in epoch milliseconds.

### Startup Recovery

On startup, Kache restores state in this order:

1. load `snapshot.rdb`
2. replay `wal.log`

This matches the usual snapshot + append-log recovery model.

## Build

### Prerequisites

- C++17 or newer
- CMake 3.10+
- `redis-cli` for end-to-end verification

### Commands

```bash
cmake -S . -B build
cmake --build build
```

## Run

Start the server:

```bash
./build/kache_server
```

Choose an eviction policy explicitly:

```bash
./build/kache_server --eviction-policy lru
./build/kache_server --eviction-policy lfu
```

Default behavior:

- if no flag is passed, Kache starts with `LRU`

The server listens on:

```text
127.0.0.1:6380
```

## Testing

### Automated Tests

Run the test suite:

```bash
ctest --test-dir build --output-on-failure
./build/kache_tests
```

The tests cover:

- inline RESP parsing
- array RESP parsing
- RESP serialization
- command dispatch behavior
- expiry and TTL behavior
- `KEYS *` and `FLUSHALL`
- concurrent `SET` + `GET` stress with 10 threads
- LRU read-refresh behavior
- LRU vs LFU comparison under the same access pattern
- WAL replay restore
- snapshot restore through `BGSAVE`

### Basic Manual Testing

Start the server in one terminal:

```bash
./build/kache_server
```

In another terminal:

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

Expected behavior:

- `SET foo bar` returns `OK`
- `GET foo` returns `bar`
- `EXISTS foo` returns `1`
- `EXPIRE foo 10` returns `1`
- `TTL foo` returns a number near `10`
- `KEYS '*'` includes `foo`
- `DEL foo` returns `1`
- `FLUSHALL` returns `OK`

### WAL Test

Remove old persistence files first:

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

- `GET foo` -> `bar`
- `EXISTS baz` -> `0`

Important note:

- if eviction is enabled and capacity is exceeded during replay, some keys written in `wal.log` may not survive after restart
- this is expected because WAL records the original write commands, not the final post-eviction in-memory state

### Snapshot Test

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

- `GET alpha` -> `1`
- `GET beta` -> `2`

### TTL Persistence Test

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

Restart the server, then:

```bash
redis-cli -p 6380 TTL temp
redis-cli -p 6380 --raw GET temp
```

Expected:

- `TTL temp` returns a positive number
- `GET temp` returns `value` until the key expires

### Inline Command Testing

You can also test raw inline commands:

```bash
printf "SET a 1\r\n" | nc 127.0.0.1 6380
printf "GET a\r\n" | nc 127.0.0.1 6380
```

## LRU vs LFU

Kache supports two startup-selectable eviction policies:

- `lru`: evict the least recently used key
- `lfu`: evict the lowest-frequency key, with recency as the tie-breaker

Implementation note:

- `EvictionPolicy` is the strategy interface
- concrete policies are registered through a small factory registry
- `InMemoryStorage` resolves the selected policy by name at startup

Startup examples:

```bash
./build/kache_server --eviction-policy lru
./build/kache_server --eviction-policy lfu
```

Comparison pattern used in tests with capacity `3`:

1. `SET a 1`
2. `SET b 2`
3. `SET c 3`
4. `GET a`
5. `GET a`
6. `GET b`
7. `GET c`
8. `SET d 4`

Survivors under the same pattern:

| Policy | Evicted Key | Surviving Keys |
| --- | --- | --- |
| LRU | `a` | `b`, `c`, `d` |
| LFU | `b` | `a`, `c`, `d` |

Why they differ:

- `LRU` cares only about the most recent touch order, so `a` becomes the oldest key by the time `d` is inserted.
- `LFU` keeps `a` because it was accessed more times than `b` or `c`; between `b` and `c`, the older one loses on the recency tie-break.

## Design

- `StorageEngine`: abstract interface for cache operations
- `InMemoryStorage`: thread-safe sharded in-memory implementation
- `LRUPolicy` and `LFUPolicy`: pluggable eviction strategies
- eviction policy construction: registry-backed factory selection
- `PersistenceManager`: WAL and snapshot read/write logic
- `CommandHandler`: validates and dispatches parsed commands
- `RespParser`: converts raw client bytes into command tokens
- `RespSerializer`: converts command results into RESP replies
- `TCPServer`: owns the socket lifecycle and request/response loop

This keeps protocol handling, storage, concurrency, and persistence responsibilities separated enough to test each part independently.

## Current Limitations

- `KEYS` currently supports only `*`
- `BGSAVE` is manual, not scheduled automatically
- snapshot format is intentionally simple, not Redis RDB-compatible
- LFU metadata is maintained in memory only; restart recovery rebuilds keys but not historical frequencies
- LRU recency metadata is also not persisted; WAL replay restores writes, not exact pre-restart recency order
- some older experimental RESP files may still exist in the repository but are not part of the active build path
- the current defaults are centralized in `src/constants/KacheConstants.h`
- the default cache size directly affects how many keys remain in memory and therefore what gets written into snapshots

## Author

Shreyas R
