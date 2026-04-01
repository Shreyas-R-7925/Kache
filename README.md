# Kache 🚀

Kache is a lightweight in-memory key-value store inspired by systems like Redis.
It supports basic operations such as **set, get, delete, and exists**.

---

## 📁 Project Structure

```
Kache/
├── include/        # Header files (interfaces + implementations)
├── src/            # Source files
├── tests/          # Unit tests (to be added)
├── bench/          # Benchmarks (to be added)
├── CMakeLists.txt  # Build configuration
├── README.md
```

---

## ⚙️ Features

* In-memory key-value storage
* Basic operations:

  * `SET`
  * `GET`
  * `DEL`
  * `EXISTS`
* Modular design using abstraction (`StorageEngine`)
* Easily extendable (LRU, TTL, persistence)

---

## 🛠️ Build & Run

### Prerequisites

* C++17 or above
* CMake installed

### Steps

# Clone the repo
```
git clone https://github.com/Shreyas-R-7925/Kache.git
cd Kache
```

# Build
```
mkdir build
cd build
cmake ..
make
``` 

# Run
```
./kache
./kache_server -> to run the TCP server
```
---

## 🧠 Design

* `StorageEngine` → Interface (defines behavior)
* `InMemoryStorage` → Concrete implementation using `unordered_map`

This design allows adding new storage engines without modifying existing code.

---

## 🚀 Future Improvements

* Persistence (disk storage)
* Benchmarking (compare with Redis)
* Unit testing

---

## 📌 Author

Shreyas R
