#include "peers.hpp"

int main(void) {
    int port;
    std::cout << "Enter port number to listen on (e.g., 8082, 8083): ";
    std::cin >> port;

    std::cout << "[DEBUG] Creating client_instance\n";
    auto* client_instance = new client(port);

    std::cout << "[DEBUG] Starting server thread\n";
    std::thread server_thread([client_instance]() {
        std::cout << "[DEBUG] Inside execute()\n";
        if (client_instance->execute() != 0) {
            perror("client execution fault");
        }
    });

    std::cout << "[DEBUG] Starting user interface\n";
    client_instance->user_interface();

    std::cout << "[DEBUG] Waiting for server thread\n";
    server_thread.join();

    std::cout << "[DEBUG] Cleaning up\n";
    delete client_instance;

    return 0;
}