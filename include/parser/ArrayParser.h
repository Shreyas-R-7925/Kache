#pragma once
#include "IRespTypeParser.h"

class RespParser;

class ArrayParser : public IRespTypeParser {
private:
    RespParser& dispatcher;

public:
    ArrayParser(RespParser& d);
    std::shared_ptr<RespValue> parse(Buffer& buffer) override;
};