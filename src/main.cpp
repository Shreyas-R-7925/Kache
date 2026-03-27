#include "../include/InMemoryStorage.h"
#include <iostream>

int main() {
    InMemoryStorage storage;

    storage.set("name", "Kache");
    storage.set("version", "1.0");

    if (storage.exists("name")) {
        std::string name = storage.get("name");
        std::cout << "Name: " << name << std::endl;
    }

    storage.del("version");

    if (!storage.exists("version")) {
        std::cout << "Version key deleted successfully." << std::endl;
    }

    return 0;
}