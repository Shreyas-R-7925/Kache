#pragma once
#include <unordered_map>
#include <memory>
#include "IRespTypeParser.h"

class RespParser {
private:
    std::unordered_map<char, std::unique_ptr<IRespTypeParser>> parsers;

public:
    RespParser();

    std::shared_ptr<RespValue> parse(Buffer& buffer);
};