#include "parser/RespSerializer.h"

#include <string>

std::string RespSerializer::serialize(const RespReply& reply) {
    switch (reply.type) {
        case RespReplyType::SimpleString:
            return "+" + reply.stringValue + "\r\n";
        case RespReplyType::BulkString:
            return "$" + std::to_string(reply.stringValue.size()) + "\r\n" + reply.stringValue + "\r\n";
        case RespReplyType::Error:
            return "-" + reply.stringValue + "\r\n";
        case RespReplyType::Integer:
            return ":" + std::to_string(reply.integerValue) + "\r\n";
        case RespReplyType::Array: {
            std::string encoded = "*" + std::to_string(reply.arrayValues.size()) + "\r\n";
            for (const auto& value : reply.arrayValues) {
                encoded += "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n";
            }
            return encoded;
        }
        case RespReplyType::NullBulkString:
            return "$-1\r\n";
    }

    return "-ERR internal serializer error\r\n";
}
