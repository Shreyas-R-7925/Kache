#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "CommandHandler.h"
#include "InMemoryStorage.h"
#include "parser/RespParser.h"
#include "parser/RespSerializer.h"
#include "constants/KacheConstants.h"

namespace {

struct TempPersistencePaths {
    std::filesystem::path directory;
    std::filesystem::path walPath;
    std::filesystem::path snapshotPath;
};

void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

TempPersistencePaths makeTempPersistencePaths(const std::string& prefix) {
    const auto uniqueSuffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / (prefix + "_" + uniqueSuffix);
    std::filesystem::create_directories(directory);

    return TempPersistencePaths{
        directory,
        directory / "wal.log",
        directory / "snapshot.rdb",
    };
}

std::shared_ptr<InMemoryStorage> makeIsolatedStorage(size_t capacity = CACHE_SIZE) {
    return std::make_shared<InMemoryStorage>(
        capacity,
        LRU_EVICTION_POLICY,
        "",
        "");
}

void expectTokens(const std::vector<std::string>& actual, const std::vector<std::string>& expected, const std::string& testName) {
    expect(actual == expected, testName + " failed");
}

void testInlineParsing() {
    RespParser parser;
    expectTokens(parser.parse("SET foo bar\r\n"), {"SET", "foo", "bar"}, "inline parsing");
}

void testArrayParsing() {
    RespParser parser;
    expectTokens(
        parser.parse("*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n"),
        {"SET", "foo", "bar"},
        "array parsing");
}

void testSerializer() {
    expect(RespSerializer::serialize(RespReply::simple("OK")) == "+OK\r\n", "simple string serialization failed");
    expect(RespSerializer::serialize(RespReply::bulk("foobar")) == "$6\r\nfoobar\r\n", "bulk serialization failed");
    expect(RespSerializer::serialize(RespReply::error("ERR boom")) == "-ERR boom\r\n", "error serialization failed");
    expect(RespSerializer::serialize(RespReply::integer(42)) == ":42\r\n", "integer serialization failed");
}

void testCommandFlow() {
    auto storage = makeIsolatedStorage();
    CommandHandler handler(storage);

    expect(RespSerializer::serialize(handler.handle({"SET", "foo", "bar"})) == "+OK\r\n", "SET failed");
    expect(RespSerializer::serialize(handler.handle({"GET", "foo"})) == "$3\r\nbar\r\n", "GET failed");
    expect(RespSerializer::serialize(handler.handle({"EXISTS", "foo"})) == ":1\r\n", "EXISTS failed");
    expect(RespSerializer::serialize(handler.handle({"DEL", "foo"})) == ":1\r\n", "DEL failed");
    expect(RespSerializer::serialize(handler.handle({"GET", "foo"})) == "$-1\r\n", "nil GET failed");
}

void testExpiryFlow() {
    auto storage = makeIsolatedStorage();
    CommandHandler handler(storage);

    handler.handle({"SET", "temp", "value"});
    expect(RespSerializer::serialize(handler.handle({"EXPIRE", "temp", "1"})) == ":1\r\n", "EXPIRE failed");

    const RespReply ttlReply = handler.handle({"TTL", "temp"});
    expect(ttlReply.type == RespReplyType::Integer, "TTL type mismatch");
    expect(
        ttlReply.integerValue >= 0 && ttlReply.integerValue <= 1,
        "TTL range mismatch: " + std::to_string(ttlReply.integerValue));

    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    expect(RespSerializer::serialize(handler.handle({"TTL", "temp"})) == ":-2\r\n", "expired TTL failed");
}

void testKeysAndFlushAll() {
    auto storage = makeIsolatedStorage();
    CommandHandler handler(storage);

    handler.handle({"SET", "alpha", "1"});
    handler.handle({"SET", "beta", "2"});

    expect(
        RespSerializer::serialize(handler.handle({"KEYS", "*"})) == "*2\r\n$5\r\nalpha\r\n$4\r\nbeta\r\n",
        "KEYS failed");

    expect(RespSerializer::serialize(handler.handle({"FLUSHALL"})) == "+OK\r\n", "FLUSHALL failed");
    expect(RespSerializer::serialize(handler.handle({"KEYS", "*"})) == "*0\r\n", "empty KEYS failed");
}

void testConcurrentSetAndGet() {
    auto storage = makeIsolatedStorage(512);

    constexpr int threadCount = 10;
    constexpr int operationsPerThread = 500;

    std::atomic<bool> failed{false};
    std::atomic<int> readyCount{0};
    std::atomic<bool> start{false};
    std::vector<std::thread> workers;
    workers.reserve(threadCount);

    for (int threadIndex = 0; threadIndex < threadCount; ++threadIndex) {
        workers.emplace_back([&, threadIndex]() {
            readyCount.fetch_add(1);
            while (!start.load()) {
            }

            for (int opIndex = 0; opIndex < operationsPerThread; ++opIndex) {
                const std::string key = "thread:" + std::to_string(threadIndex) + ":key:" + std::to_string(opIndex % 25);
                const std::string value = "value:" + std::to_string(threadIndex) + ":" + std::to_string(opIndex);

                storage->set(key, value);
                const auto stored = storage->get(key);

                if (!stored.has_value() || stored.value() != value) {
                    failed = true;
                    return;
                }
            }
        });
    }

    while (readyCount.load() != threadCount) {
    }
    start = true;

    for (auto& worker : workers) {
        worker.join();
    }

    expect(!failed.load(), "concurrent SET/GET stress test failed");

    for (int threadIndex = 0; threadIndex < threadCount; ++threadIndex) {
        for (int keyIndex = 0; keyIndex < 25; ++keyIndex) {
            const std::string key = "thread:" + std::to_string(threadIndex) + ":key:" + std::to_string(keyIndex);
            expect(storage->exists(key), "concurrent stress test lost key: " + key);
        }
    }
}

void testLruReadRefreshesRecency() {
    auto storage = makeIsolatedStorage(2);

    storage->set("a", "1");
    storage->set("b", "2");

    const auto value = storage->get("a");
    expect(value.has_value() && value.value() == "1", "LRU read refresh setup failed");

    storage->set("c", "3");

    expect(storage->exists("a"), "LRU should retain recently read key");
    expect(!storage->exists("b"), "LRU should evict least recently used key");
    expect(storage->exists("c"), "LRU should keep new key");
}

void testWalReplayRestoresState() {
    const TempPersistencePaths paths = makeTempPersistencePaths("kache_wal");

    {
        auto storage = std::make_shared<InMemoryStorage>(
            CACHE_SIZE,
            LRU_EVICTION_POLICY,
            paths.walPath.string(),
            paths.snapshotPath.string());
        CommandHandler handler(storage);

        expect(RespSerializer::serialize(handler.handle({"SET", "foo", "bar"})) == "+OK\r\n", "WAL SET foo failed");
        expect(RespSerializer::serialize(handler.handle({"SET", "baz", "qux"})) == "+OK\r\n", "WAL SET baz failed");
        expect(RespSerializer::serialize(handler.handle({"DEL", "baz"})) == ":1\r\n", "WAL DEL failed");
    }

    {
        auto storage = std::make_shared<InMemoryStorage>(
            CACHE_SIZE,
            LRU_EVICTION_POLICY,
            paths.walPath.string(),
            paths.snapshotPath.string());
        CommandHandler handler(storage);

        expect(RespSerializer::serialize(handler.handle({"GET", "foo"})) == "$3\r\nbar\r\n", "WAL replay missing foo");
        expect(RespSerializer::serialize(handler.handle({"GET", "baz"})) == "$-1\r\n", "WAL replay restored deleted key");
    }

    std::filesystem::remove_all(paths.directory);
}

void testBgsaveRestoresState() {
    const TempPersistencePaths paths = makeTempPersistencePaths("kache_snapshot");

    {
        auto storage = std::make_shared<InMemoryStorage>(
            CACHE_SIZE,
            LRU_EVICTION_POLICY,
            paths.walPath.string(),
            paths.snapshotPath.string());
        CommandHandler handler(storage);

        expect(RespSerializer::serialize(handler.handle({"SET", "alpha", "1"})) == "+OK\r\n", "snapshot SET alpha failed");
        expect(RespSerializer::serialize(handler.handle({"SET", "beta", "2"})) == "+OK\r\n", "snapshot SET beta failed");
        expect(RespSerializer::serialize(handler.handle({"EXPIRE", "beta", "30"})) == ":1\r\n", "snapshot EXPIRE beta failed");
        expect(RespSerializer::serialize(handler.handle({"BGSAVE"})) == "+OK\r\n", "BGSAVE failed");
    }

    {
        auto storage = std::make_shared<InMemoryStorage>(
            CACHE_SIZE,
            LRU_EVICTION_POLICY,
            paths.walPath.string(),
            paths.snapshotPath.string());
        CommandHandler handler(storage);

        expect(RespSerializer::serialize(handler.handle({"GET", "alpha"})) == "$1\r\n1\r\n", "snapshot restore missing alpha");
        expect(RespSerializer::serialize(handler.handle({"GET", "beta"})) == "$1\r\n2\r\n", "snapshot restore missing beta");

        const RespReply ttlReply = handler.handle({"TTL", "beta"});
        expect(ttlReply.type == RespReplyType::Integer, "snapshot TTL type mismatch");
        expect(ttlReply.integerValue > 0 && ttlReply.integerValue <= 30, "snapshot TTL replay mismatch");
    }

    std::filesystem::remove_all(paths.directory);
}

}  // namespace

int main() {
    try {
        testInlineParsing();
        testArrayParsing();
        testSerializer();
        testCommandFlow();
        testExpiryFlow();
        testKeysAndFlushAll();
        testConcurrentSetAndGet();
        testLruReadRefreshesRecency();
        testWalReplayRestoresState();
        testBgsaveRestoresState();
        std::cout << "All tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return EXIT_FAILURE;
    }
}
