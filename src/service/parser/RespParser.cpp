#include "parser/RespParser.h"

#include <cctype>
#include <sstream>
#include <stdexcept>

namespace {

void expectCrlf(const std::string& input, size_t& offset) {
    if (offset + 1 >= input.size() || input[offset] != '\r' || input[offset + 1] != '\n') {
        throw std::runtime_error("Invalid RESP: expected CRLF");
    }

    offset += 2;
}

std::string readUntilCrlf(const std::string& input, size_t& offset) {
    const size_t crlf = input.find("\r\n", offset);
    if (crlf == std::string::npos) {
        throw std::runtime_error("Invalid RESP: missing CRLF");
    }

    const std::string token = input.substr(offset, crlf - offset);
    offset = crlf + 2;
    return token;
}

}  // namespace

std::vector<std::string> RespParser::parse(const std::string& rawBytes) const {
    if (rawBytes.empty()) {
        throw std::runtime_error("Empty request");
    }

    if (rawBytes.front() == '*') {
        return parseArray(rawBytes);
    }

    return parseInline(rawBytes);
}

std::vector<std::string> RespParser::parseInline(const std::string& rawBytes) {
    std::string normalized = rawBytes;
    while (!normalized.empty() && (normalized.back() == '\n' || normalized.back() == '\r')) {
        normalized.pop_back();
    }

    std::istringstream input(normalized);
    std::vector<std::string> tokens;
    std::string token;
    while (input >> token) {
        tokens.push_back(token);
    }

    if (tokens.empty()) {
        throw std::runtime_error("Invalid inline command");
    }

    return tokens;
}

std::vector<std::string> RespParser::parseArray(const std::string& rawBytes) {
    size_t offset = 1;
    const int elementCount = std::stoi(readUntilCrlf(rawBytes, offset));
    if (elementCount < 0) {
        throw std::runtime_error("Invalid RESP array length");
    }

    std::vector<std::string> tokens;
    tokens.reserve(static_cast<size_t>(elementCount));

    for (int index = 0; index < elementCount; ++index) {
        if (offset >= rawBytes.size() || rawBytes[offset] != '$') {
            throw std::runtime_error("Invalid RESP: expected bulk string");
        }

        ++offset;
        const int bulkLength = std::stoi(readUntilCrlf(rawBytes, offset));
        if (bulkLength < 0) {
            throw std::runtime_error("Null bulk strings are not supported in commands");
        }

        if (offset + static_cast<size_t>(bulkLength) > rawBytes.size()) {
            throw std::runtime_error("Invalid RESP: bulk string truncated");
        }

        tokens.push_back(rawBytes.substr(offset, static_cast<size_t>(bulkLength)));
        offset += static_cast<size_t>(bulkLength);
        expectCrlf(rawBytes, offset);
    }

    return tokens;
}
