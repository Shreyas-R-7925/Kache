#include <iostream>
#include <memory>

#include "CommandHandler.h"
#include "InMemoryStorage.h"
#include "server/TCPServer.h"
#include "constants/KacheConstants.h"

int main() {
    try {
        auto storage = std::make_shared<InMemoryStorage>(CACHE_SIZE, LRU_EVICTION_POLICY);
        auto commandHandler = std::make_shared<CommandHandler>(storage);
        TCPServer server(6380, commandHandler);
        server.run();
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}