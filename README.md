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
- LRU eviction
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

## Design

- `StorageEngine`: abstract interface for cache operations
- `InMemoryStorage`: thread-safe sharded in-memory implementation
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
- some older experimental RESP files may still exist in the repository but are not part of the active build path
- the current default `CACHE_SIZE` in `src/constants/KacheConstants.h` directly affects how many keys remain in memory and therefore what gets written into snapshots

## Author

Shreyas R
