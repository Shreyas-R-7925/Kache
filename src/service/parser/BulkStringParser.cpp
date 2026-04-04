#include "../include/parser/BulkStringParser.h"

std::shared_ptr<RespValue> BulkStringParser::parse(Buffer& buffer) {
    buffer.get(); 

    int len = std::stoi(buffer.readLine());

    if (len == -1) {
        return RespValue::createNull();
    }

    std::string value = buffer.read(len);
    buffer.read(2);

    return RespValue::createBulk(value);
}