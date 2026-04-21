#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

class PersistenceManager {
public:
    struct SnapshotRecord {
        std::string key;
        std::string value;
        std::optional<std::chrono::system_clock::time_point> expiry;
    };

    PersistenceManager(std::string walPath, std::string snapshotPath);

    void appendCommand(const std::vector<std::string>& tokens);
    std::vector<std::vector<std::string>> loadWal() const;
    std::vector<SnapshotRecord> loadSnapshot() const;
    void writeSnapshot(const std::vector<SnapshotRecord>& records) const;
    void truncateWal() const;

private:
    std::string walPath_;
    std::string snapshotPath_;
    bool enabled_ = true;

    static std::string escapeToken(const std::string& token);
    static std::string unescapeToken(const std::string& token);
    static std::string joinTokens(const std::vector<std::string>& tokens);
    static std::vector<std::string> splitTokens(const std::string& line);
    static long long toEpochMillis(std::chrono::system_clock::time_point timePoint);
    static std::chrono::system_clock::time_point fromEpochMillis(long long epochMillis);
};
