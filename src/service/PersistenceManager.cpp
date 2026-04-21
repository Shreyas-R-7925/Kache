#include "PersistenceManager.h"
#include "constants/KacheConstants.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include "constants/KacheConstants.h"

PersistenceManager::PersistenceManager(std::string walPath, std::string snapshotPath)
    : walPath_(std::move(walPath)), snapshotPath_(std::move(snapshotPath)) {
    enabled_ = !walPath_.empty() && !snapshotPath_.empty();
}

void PersistenceManager::appendCommand(const std::vector<std::string>& tokens) {
    if (!enabled_) {
        return;
    }

    std::ofstream walStream(walPath_, std::ios::app);
    if (!walStream.is_open()) {
        throw std::runtime_error("Failed to open WAL for append");
    }

    walStream << joinTokens(tokens) << '\n';
}

std::vector<std::vector<std::string>> PersistenceManager::loadWal() const {
    if (!enabled_) {
        return {};
    }

    std::ifstream walStream(walPath_);
    if (!walStream.is_open()) {
        return {};
    }

    std::vector<std::vector<std::string>> commands;
    std::string line;
    while (std::getline(walStream, line)) {
        if (!line.empty()) {
            commands.push_back(splitTokens(line));
        }
    }

    return commands;
}

std::vector<PersistenceManager::SnapshotRecord> PersistenceManager::loadSnapshot() const {
    if (!enabled_) {
        return {};
    }

    std::ifstream snapshotStream(snapshotPath_);
    if (!snapshotStream.is_open()) {
        return {};
    }

    std::string header;
    if (!std::getline(snapshotStream, header)) {
        return {};
    }

    if (header != kache::constants::kSnapshotHeader) {
        throw std::runtime_error("Invalid snapshot header");
    }

    std::vector<SnapshotRecord> records;
    std::string line;
    while (std::getline(snapshotStream, line)) {
        if (line.empty()) {
            continue;
        }

        const std::vector<std::string> tokens = splitTokens(line);
        if (tokens.size() != 3) {
            throw std::runtime_error("Invalid snapshot record");
        }

        SnapshotRecord record;
        record.key = tokens[0];
        record.value = tokens[1];
        if (tokens[2] != "-1") {
            record.expiry = fromEpochMillis(std::stoll(tokens[2]));
        }
        records.push_back(std::move(record));
    }

    return records;
}

void PersistenceManager::writeSnapshot(const std::vector<SnapshotRecord>& records) const {
    if (!enabled_) {
        return;
    }

    std::ofstream snapshotStream(snapshotPath_, std::ios::trunc);
    if (!snapshotStream.is_open()) {
        throw std::runtime_error("Failed to open snapshot for write");
    }

    snapshotStream << kache::constants::kSnapshotHeader << '\n';
    for (const auto& record : records) {
        const std::string expiryToken = record.expiry.has_value()
            ? std::to_string(toEpochMillis(record.expiry.value()))
            : "-1";
        snapshotStream << joinTokens({record.key, record.value, expiryToken}) << '\n';
    }
}

void PersistenceManager::truncateWal() const {
    if (!enabled_) {
        return;
    }

    std::ofstream walStream(walPath_, std::ios::trunc);
    if (!walStream.is_open()) {
        throw std::runtime_error("Failed to truncate WAL");
    }
}

std::string PersistenceManager::escapeToken(const std::string& token) {
    std::string escaped;
    escaped.reserve(token.size());

    for (const char character : token) {
        switch (character) {
            case '\\':
                escaped += "\\\\";
                break;
            case ' ':
                escaped += "\\s";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped += character;
                break;
        }
    }

    return escaped;
}

std::string PersistenceManager::unescapeToken(const std::string& token) {
    std::string unescaped;
    unescaped.reserve(token.size());

    for (size_t index = 0; index < token.size(); ++index) {
        if (token[index] != '\\' || index + 1 >= token.size()) {
            unescaped += token[index];
            continue;
        }

        ++index;
        switch (token[index]) {
            case '\\':
                unescaped += '\\';
                break;
            case 's':
                unescaped += ' ';
                break;
            case 'n':
                unescaped += '\n';
                break;
            case 't':
                unescaped += '\t';
                break;
            default:
                unescaped += token[index];
                break;
        }
    }

    return unescaped;
}

std::string PersistenceManager::joinTokens(const std::vector<std::string>& tokens) {
    std::ostringstream encoded;
    for (size_t index = 0; index < tokens.size(); ++index) {
        if (index > 0) {
            encoded << ' ';
        }
        encoded << escapeToken(tokens[index]);
    }
    return encoded.str();
}

std::vector<std::string> PersistenceManager::splitTokens(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;

    for (size_t index = 0; index < line.size(); ++index) {
        const char character = line[index];
        if (character == ' ') {
            tokens.push_back(unescapeToken(current));
            current.clear();
            continue;
        }

        current += character;
    }

    tokens.push_back(unescapeToken(current));
    return tokens;
}

long long PersistenceManager::toEpochMillis(std::chrono::system_clock::time_point timePoint) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(timePoint.time_since_epoch()).count();
}

std::chrono::system_clock::time_point PersistenceManager::fromEpochMillis(long long epochMillis) {
    return std::chrono::system_clock::time_point(std::chrono::milliseconds(epochMillis));
}
