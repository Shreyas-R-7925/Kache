#include "../include/parser/RespValue.h"

std::shared_ptr<RespValue> RespValue::createBulk(const std::string& val) {
    auto r = std::make_shared<RespValue>();
    r->type = RespType::BULK_STRING;
    r->strValue = val;
    return r;
}

std::shared_ptr<RespValue> RespValue::createArray(const std::vector<std::shared_ptr<RespValue>>& val) {
    auto r = std::make_shared<RespValue>();
    r->type = RespType::ARRAY;
    r->arrayValue = val;
    return r;
}

std::shared_ptr<RespValue> RespValue::createNull() {
    auto r = std::make_shared<RespValue>();
    r->type = RespType::NULL_BULK;
    return r;
} 