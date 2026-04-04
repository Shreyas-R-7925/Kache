#pragma once
#include "IRespTypeParser.h"

class BulkStringParser : public IRespTypeParser {
public:
    std::shared_ptr<RespValue> parse(Buffer& buffer) override;
};