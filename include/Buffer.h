#pragma once
#include <string>

class Buffer {
private:
    std::string data;
    size_t pos = 0;

public:
    Buffer(const std::string& input);

    char peek() const;
    char get();

    std::string read(size_t len);
    std::string readLine(); 

    bool hasRemaining() const;
};