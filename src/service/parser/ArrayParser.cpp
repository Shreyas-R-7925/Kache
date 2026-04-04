#include "../include/parser/ArrayParser.h"
#include "../include/parser/RespParser.h"

ArrayParser::ArrayParser(RespParser& d) : dispatcher(d) {}

std::shared_ptr<RespValue> ArrayParser::parse(Buffer& buffer) {
    buffer.get(); 

    int count = std::stoi(buffer.readLine());

    std::vector<std::shared_ptr<RespValue>> elements;

    for (int i = 0; i < count; i++) {
        elements.push_back(dispatcher.parse(buffer));
    }

    return RespValue::createArray(elements);
}