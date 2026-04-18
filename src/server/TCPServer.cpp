#include "server/TCPServer.h"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

#include "parser/RespSerializer.h"

namespace {
constexpr int kBacklog = 16;
constexpr size_t kBufferSize = 4096;
}

TCPServer::TCPServer(int port, std::shared_ptr<CommandHandler> commandHandler)
    : port_(port), commandHandler_(std::move(commandHandler)) {}

void TCPServer::run() const {
    const int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    int reuseAddress = 1;
    if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &reuseAddress, sizeof(reuseAddress)) < 0) {
        close(serverFd);
        throw std::runtime_error("Failed to set socket options");
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(static_cast<uint16_t>(port_));

    if (bind(serverFd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        close(serverFd);
        throw std::runtime_error("Failed to bind socket");
    }

    if (listen(serverFd, kBacklog) < 0) {
        close(serverFd);
        throw std::runtime_error("Failed to listen on socket");
    }

    std::cout << "Kache listening on port " << port_ << '\n';

    while (true) {
        socklen_t addressLength = sizeof(address);
        const int clientFd = accept(serverFd, reinterpret_cast<sockaddr*>(&address), &addressLength);
        if (clientFd < 0) {
            std::cerr << "accept failed\n";
            continue;
        }

        while (true) {
            char buffer[kBufferSize] = {0};
            const ssize_t bytesRead = read(clientFd, buffer, sizeof(buffer));
            if (bytesRead <= 0) {
                break;
            }

            const std::string rawRequest(buffer, static_cast<size_t>(bytesRead));
            const std::string response = handleRequest(rawRequest);
            send(clientFd, response.c_str(), response.size(), 0);
        }

        close(clientFd);
    }
}

std::string TCPServer::handleRequest(const std::string& rawRequest) const {
    try {
        const std::vector<std::string> command = parser_.parse(rawRequest);
        const RespReply reply = commandHandler_->handle(command);
        return RespSerializer::serialize(reply);
    } catch (const std::exception& exception) {
        return RespSerializer::serialize(RespReply::error(std::string("ERR ") + exception.what()));
    }
}
