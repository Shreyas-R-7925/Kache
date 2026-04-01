#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 6380
#define BUFFER_SIZE 1024

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    // 1. Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("socket failed");
        return 1;
    }

    // 2. Allow reuse of address/port
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 3. Bind
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        return 1;
    }

    // 4. Listen
    if (listen(server_fd, 5) < 0) {
        perror("listen");
        return 1;
    }

    std::cout << "Server listening on port " << PORT << "...\n";

    while (true) {
        // 5. Accept connection
        client_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (client_socket < 0) {
            perror("accept");
            continue;
        }

        std::cout << "Client connected\n";

        // 6. Read data
        int valread = read(client_socket, buffer, BUFFER_SIZE);
        if (valread > 0) {
            std::cout << "Received: " << buffer << std::endl;

            // 7. Send response
            const char* response = "OK\r\n";
            send(client_socket, response, strlen(response), 0);
        }

        close(client_socket);
    }

    close(server_fd);
    return 0;
}