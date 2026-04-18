#include "../include/converter/CommandConverter.h"
#include <stdexcept>

std::vector<std::string> CommandConverter::toCommand(const std::shared_ptr<RespValue>& value) {
    if (value->type != RespType::ARRAY) {
        throw std::runtime_error("Expected array for command");
    }

    std::vector<std::string> result;

    for (auto& v : value->arrayValue) {
        if (v->type != RespType::BULK_STRING) {
            throw std::runtime_error("Expected bulk string");
        }
        result.push_back(v->strValue);
    }

    return result;
}