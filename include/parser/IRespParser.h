#pragma once
#include "Buffer.h"
#include "RespValue.h"

class IRespTypeParser {
public:
    virtual ~IRespTypeParser() = default;

    virtual std::shared_ptr<RespValue> parse(Buffer& buffer) = 0;
};