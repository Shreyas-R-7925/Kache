#pragma once
#include <string>
#include <vector>
#include <memory>

enum class RespType {
    BULK_STRING,
    ARRAY,
    NULL_BULK
};

class RespValue {
public:
    RespType type;

    std::string strValue;
    std::vector<std::shared_ptr<RespValue>> arrayValue;

    static std::shared_ptr<RespValue> createBulk(const std::string& val);
    static std::shared_ptr<RespValue> createArray(const std::vector<std::shared_ptr<RespValue>>& val);
    static std::shared_ptr<RespValue> createNull();
};