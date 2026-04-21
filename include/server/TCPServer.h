#pragma once

#include <memory>
#include <string>

#include "CommandHandler.h"
#include "parser/RespParser.h"

class TCPServer {
public:
    TCPServer(int port, std::shared_ptr<CommandHandler> commandHandler);
    void run() const;

private:
    int port_;
    std::shared_ptr<CommandHandler> commandHandler_;
    RespParser parser_;

    void handleClient(int clientFd) const;
    std::string handleRequest(const std::string& rawRequest) const;
};
