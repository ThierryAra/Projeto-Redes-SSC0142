#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

const int PORT = 8080;
const int BUFFER_SIZE = 4096;

int main() {
    int serverSocket, clientSocket;
    struct sockaddr_in serverAddress, clientAddress;
    char buffer[BUFFER_SIZE];

    // Criação do socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Erro ao criar o socket." << std::endl;
        return -1;
    }

    // Configuração do endereço do servidor
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(PORT);

    // Bind do socket ao endereço
    if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "Erro ao fazer o bind." << std::endl;
        return -1;
    }

    // Espera por conexões
    if (listen(serverSocket, 5) < 0) {
        std::cerr << "Erro ao esperar por conexões." << std::endl;
        return -1;
    }

    std::cout << "Aguardando conexões..." << std::endl;

    socklen_t clientAddressLength = sizeof(clientAddress);
    clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddress, &clientAddressLength);
    if (clientSocket < 0) {
        std::cerr << "Erro ao aceitar conexão." << std::endl;
        return -1;
    }

    std::cout << "Conexão estabelecida." << std::endl;

    while (true) {
        // Receber mensagem do cliente
        memset(buffer, 0, BUFFER_SIZE);
        if (recv(clientSocket, buffer, BUFFER_SIZE, 0) <= 0) {
            std::cerr << "Erro ao receber mensagem do cliente." << std::endl;
            break;
        }

        std::string receivedMessage(buffer);

        if (receivedMessage == "exit") {
            std::cout << "Cliente desconectado." << std::endl;
            break;
        }

        std::cout << "Cliente: " << receivedMessage << std::endl;

        // Enviar mensagem para o cliente
        std::string response;
        std::cout << "Servidor: ";
        std::getline(std::cin, response);

        if (response == "exit") {
            std::cout << "Desconectando..." << std::endl;
            break;
        }

        if (response.length() > BUFFER_SIZE) {
            std::cout << "A mensagem é muito grande." << std::endl;
            continue;
        }

        if (send(clientSocket, response.c_str(), response.length(), 0) < 0) {
            std::cerr << "Erro ao enviar mensagem para o cliente." << std::endl;
            break;
        }
    }

    // Fechar os sockets
    close(clientSocket);
    close(serverSocket);

    return 0;
}
