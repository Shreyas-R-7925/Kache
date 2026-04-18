#pragma once

#include <string>

#include "parser/RespReply.h"

class RespSerializer {
public:
    static std::string serialize(const RespReply& reply);
};
