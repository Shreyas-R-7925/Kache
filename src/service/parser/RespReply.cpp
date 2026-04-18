#include "parser/RespReply.h"

RespReply RespReply::simple(std::string value) {
    return RespReply{RespReplyType::SimpleString, std::move(value)};
}

RespReply RespReply::bulk(std::string value) {
    return RespReply{RespReplyType::BulkString, std::move(value)};
}

RespReply RespReply::error(std::string value) {
    return RespReply{RespReplyType::Error, std::move(value)};
}

RespReply RespReply::integer(long value) {
    RespReply reply{RespReplyType::Integer, "", value};
    return reply;
}

RespReply RespReply::array(std::vector<std::string> values) {
    RespReply reply{RespReplyType::Array};
    reply.arrayValues = std::move(values);
    return reply;
}

RespReply RespReply::nullBulk() {
    return RespReply{RespReplyType::NullBulkString};
}
