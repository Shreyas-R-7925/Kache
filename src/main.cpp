#include "../include/InMemoryStorage.h"
#include <iostream>
#include <thread>

int main() {
    InMemoryStorage storage;

    storage.set("name", "shreyas");
    auto val1 = storage.get("name");

    if (val1) {
        std::cout << "name: " << *val1 << std::endl;
    }

    storage.set("temp", "123", std::chrono::seconds(2));

    auto val2 = storage.get("temp");
    std::cout << "temp before expiry: " << (val2 ? *val2 : "null") << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(3));

    auto val3 = storage.get("temp");
    std::cout << "temp after expiry: " << (val3 ? *val3 : "null") << std::endl;

    return 0;
}