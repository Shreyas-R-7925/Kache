#include "CommandHandler.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

CommandHandler::CommandHandler(std::shared_ptr<StorageEngine> storage) : storage_(std::move(storage)) {}

RespReply CommandHandler::handle(const std::vector<std::string>& command) {
    if (command.empty()) {
        return RespReply::error("ERR empty command");
    }

    const std::string verb = uppercase(command.front());

    if (verb == "SET") {
        if (command.size() != 3) {
            return RespReply::error("ERR wrong number of arguments for 'SET' command");
        }

        storage_->set(command[1], command[2]);
        return RespReply::simple("OK");
    }

    if (verb == "GET") {
        if (command.size() != 2) {
            return RespReply::error("ERR wrong number of arguments for 'GET' command");
        }

        const auto value = storage_->get(command[1]);
        return value.has_value() ? RespReply::bulk(*value) : RespReply::nullBulk();
    }

    if (verb == "DEL") {
        if (command.size() < 2) {
            return RespReply::error("ERR wrong number of arguments for 'DEL' command");
        }

        long deleted = 0;
        for (size_t index = 1; index < command.size(); ++index) {
            deleted += storage_->del(command[index]) ? 1 : 0;
        }

        return RespReply::integer(deleted);
    }

    if (verb == "EXISTS") {
        if (command.size() < 2) {
            return RespReply::error("ERR wrong number of arguments for 'EXISTS' command");
        }

        long matches = 0;
        for (size_t index = 1; index < command.size(); ++index) {
            matches += storage_->exists(command[index]) ? 1 : 0;
        }

        return RespReply::integer(matches);
    }

    if (verb == "EXPIRE") {
        if (command.size() != 3) {
            return RespReply::error("ERR wrong number of arguments for 'EXPIRE' command");
        }

        long seconds = 0;
        if (!parsePositiveSeconds(command[2], seconds)) {
            return RespReply::error("ERR value is not an integer or out of range");
        }

        return RespReply::integer(storage_->expire(command[1], std::chrono::seconds(seconds)) ? 1 : 0);
    }

    if (verb == "TTL") {
        if (command.size() != 2) {
            return RespReply::error("ERR wrong number of arguments for 'TTL' command");
        }

        return RespReply::integer(storage_->ttl(command[1]));
    }

    if (verb == "KEYS") {
        if (command.size() != 2) {
            return RespReply::error("ERR wrong number of arguments for 'KEYS' command");
        }

        if (command[1] != "*") {
            return RespReply::error("ERR only KEYS * is supported");
        }

        return RespReply::array(storage_->keys(command[1]));
    }

    if (verb == "FLUSHALL") {
        if (command.size() != 1) {
            return RespReply::error("ERR wrong number of arguments for 'FLUSHALL' command");
        }

        storage_->flushAll();
        return RespReply::simple("OK");
    }

    if (verb == "BGSAVE") {
        if (command.size() != 1) {
            return RespReply::error("ERR wrong number of arguments for 'BGSAVE' command");
        }

        storage_->bgsave();
        return RespReply::simple("OK");
    }

    return RespReply::error("ERR unknown command");
}

std::string CommandHandler::uppercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return value;
}

bool CommandHandler::parsePositiveSeconds(const std::string& rawValue, long& seconds) {
    try {
        size_t processed = 0;
        const long parsed = std::stol(rawValue, &processed);
        if (processed != rawValue.size() || parsed < 0) {
            return false;
        }

        seconds = parsed;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}
