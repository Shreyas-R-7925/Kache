#include "../include/Buffer.h"
#include <stdexcept>

Buffer::Buffer(const std::string& input) : data(input) {}

char Buffer::peek() const {
    if (pos >= data.size()) throw std::runtime_error("Buffer underflow");
    return data[pos];
}

char Buffer::get() {
    if (pos >= data.size()) throw std::runtime_error("Buffer underflow");
    return data[pos++];
}

std::string Buffer::read(size_t len) {
    if (pos + len > data.size()) throw std::runtime_error("Buffer underflow");
    std::string res = data.substr(pos, len);
    pos += len;
    return res;
}

std::string Buffer::readLine() {
    size_t start = pos;

    while (pos + 1 < data.size()) {
        if (data[pos] == '\r' && data[pos + 1] == '\n') {
            std::string line = data.substr(start, pos - start);
            pos += 2;
            return line;
        }
        pos++;
    }

    throw std::runtime_error("Invalid RESP: missing CRLF");
}

bool Buffer::hasRemaining() const {
    return pos < data.size();
}