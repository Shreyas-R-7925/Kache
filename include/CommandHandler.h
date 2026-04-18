#pragma once

#include <memory>
#include <string>
#include <vector>

#include "StorageEngine.h"
#include "parser/RespReply.h"

class CommandHandler {
public:
    explicit CommandHandler(std::shared_ptr<StorageEngine> storage);

    RespReply handle(const std::vector<std::string>& command);

private:
    std::shared_ptr<StorageEngine> storage_;

    static std::string uppercase(std::string value);
    static bool parsePositiveSeconds(const std::string& rawValue, long& seconds);
};
