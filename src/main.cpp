#include <algorithm>
#include <iostream>
#include <memory>
#include <string>

#include "CommandHandler.h"
#include "InMemoryStorage.h"
#include "server/TCPServer.h"
#include "constants/KacheConstants.h"

namespace {

std::string normalizePolicy(std::string rawValue) {
    std::transform(rawValue.begin(), rawValue.end(), rawValue.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return rawValue;
}

std::string parseEvictionPolicy(int argc, char* argv[]) {
    std::string policy = kache::constants::kLruEvictionPolicy;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument != kache::constants::kEvictionPolicyFlag) {
            throw std::invalid_argument("Unknown argument: " + argument);
        }

        if (index + 1 >= argc) {
            throw std::invalid_argument("Missing value for --eviction-policy");
        }

        policy = normalizePolicy(argv[++index]);
        if (policy != kache::constants::kLruEvictionPolicy && policy != kache::constants::kLfuEvictionPolicy) {
            throw std::invalid_argument("Unsupported eviction policy: " + policy);
        }
    }

    return policy;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const std::string evictionPolicy = parseEvictionPolicy(argc, argv);
        auto storage = std::make_shared<InMemoryStorage>(kache::constants::kDefaultCacheSize, evictionPolicy);
        auto commandHandler = std::make_shared<CommandHandler>(storage);
        TCPServer server(kache::constants::kServerPort, commandHandler);
        std::cout << "Using eviction policy: " << evictionPolicy << '\n';
        server.run();
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
