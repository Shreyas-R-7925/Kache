#pragma once
#include <memory>
#include "RespValue.h"
#include "Buffer.h"

class IRespTypeParser {
public:
    virtual ~IRespTypeParser() = default;
    virtual std::shared_ptr<RespValue> parse(Buffer& buffer) = 0;
};