#pragma once
#include <vector>
#include <string>
#include "parser/RespValue.h"

class CommandConverter {
public:
    static std::vector<std::string> toCommand(const std::shared_ptr<RespValue>& value);
};