#include "../include/parser/RespParser.h"
#include "../include/parser/BulkStringParser.h"
#include "../include/parser/ArrayParser.h"
#include <stdexcept>

RespParser::RespParser() {
    parsers['$'] = std::make_unique<BulkStringParser>();
    parsers['*'] = std::make_unique<ArrayParser>(*this);
}

std::shared_ptr<RespValue> RespParser::parse(Buffer& buffer) {
    char type = buffer.peek();

    if (parsers.find(type) == parsers.end()) {
        throw std::runtime_error("Unsupported RESP type");
    }

    return parsers[type]->parse(buffer);
}