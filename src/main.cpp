#include "InMemoryStorage.h"
#include <iostream>
#include <thread>

int main() {

    InMemoryStorage cache(2, "LRU");

    cache.set("a", "1");
    cache.set("b", "2");

    std::cout << "Get a: " << cache.get("a").value_or("NULL") << "\n"; 

    cache.get("a");

    cache.set("c", "3");

    std::cout << "Get b (should be NULL): " << cache.get("b").value_or("NULL") << "\n";
    std::cout << "Get a (should exist): " << cache.get("a").value_or("NULL") << "\n";
    std::cout << "Get c (should exist): " << cache.get("c").value_or("NULL") << "\n";

    cache.set("d", "4", std::chrono::seconds(2));

    std::cout << "Get d immediately: " << cache.get("d").value_or("NULL") << "\n";

    std::cout << "Sleeping 3 seconds...\n";
    std::this_thread::sleep_for(std::chrono::seconds(3));

    std::cout << "Get d after expiry (should be NULL): " 
              << cache.get("d").value_or("NULL") << "\n";

    cache.set("e", "5");

    std::cout << "Exists e (1=true): " << cache.exists("e") << "\n";
    std::cout << "Exists x (0=false): " << cache.exists("x") << "\n";

    cache.del("e");
    std::cout << "Exists e after delete (0=false): " << cache.exists("e") << "\n";

    cache.set("f", "6", std::chrono::seconds(2));

    std::cout << "Waiting 6 seconds for cleanup thread...\n";
    std::this_thread::sleep_for(std::chrono::seconds(6));

    std::cout << "Get f after cleanup (should be NULL): "
              << cache.get("f").value_or("NULL") << "\n";

    return 0;
}