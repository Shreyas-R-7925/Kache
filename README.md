# Kache

Kache is a lightweight in-memory key-value store inspired by Redis. It now exposes a Redis-compatible TCP interface on port `6380`, parses RESP requests, dispatches commands through a storage engine, and returns RESP responses that work with `redis-cli`.

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
- LRU-backed in-memory storage
- TTL and expiry support

## Supported Commands

- `SET key value`
- `GET key`
- `DEL key [key ...]`
- `EXISTS key [key ...]`
- `EXPIRE key seconds`
- `TTL key`
- `KEYS *`
- `FLUSHALL`

## Project Structure

```text
Kache/
├── include/
│   ├── CommandHandler.h
│   ├── InMemoryStorage.h
│   ├── StorageEngine.h
│   ├── parser/
│   └── server/
├── src/
│   ├── constants/
│   ├── server/
│   └── service/
├── tests/
├── CMakeLists.txt
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

### Unit Tests

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

### Manual Testing with redis-cli

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

### Inline Command Testing

You can also test raw inline commands:

```bash
printf "SET a 1\r\n" | nc 127.0.0.1 6380
printf "GET a\r\n" | nc 127.0.0.1 6380
```

## Design

- `StorageEngine`: abstract interface for cache operations
- `InMemoryStorage`: in-memory implementation using `unordered_map`
- `CommandHandler`: validates and dispatches parsed commands
- `RespParser`: converts raw client bytes into command tokens
- `RespSerializer`: converts command results into RESP replies
- `TCPServer`: owns the socket lifecycle and request/response loop

This keeps the Redis protocol handling separate from storage concerns and makes each layer easier to test independently.

## Current Limitations

- `KEYS` currently supports only `*`
- persistence is not implemented
- the server handles clients in a simple sequential loop
- some older experimental RESP files may still exist in the repository but are not part of the active build path

## Author

Shreyas R
