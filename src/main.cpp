#include "../include/parser/RespParser.h"
#include "../include/Buffer.h"
#include <iostream>

int main() {
    std::string input = "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n";

    Buffer buffer(input);
    RespParser parser;

    auto result = parser.parse(buffer);

    for (auto& val : result->arrayValue) {
        std::cout << val->strValue << std::endl;
    }

    return 0;
}