#pragma once

#include <string>
#include <vector>

class RespParser {
public:
    std::vector<std::string> parse(const std::string& rawBytes) const;

private:
    static std::vector<std::string> parseInline(const std::string& rawBytes);
    static std::vector<std::string> parseArray(const std::string& rawBytes);
};
