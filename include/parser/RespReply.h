#pragma once

#include <string>
#include <vector>

enum class RespReplyType {
    SimpleString,
    BulkString,
    Error,
    Integer,
    Array,
    NullBulkString
};

struct RespReply {
    RespReplyType type;
    std::string stringValue;
    long integerValue = 0;
    std::vector<std::string> arrayValues;

    static RespReply simple(std::string value);
    static RespReply bulk(std::string value);
    static RespReply error(std::string value);
    static RespReply integer(long value);
    static RespReply array(std::vector<std::string> values);
    static RespReply nullBulk();
};
