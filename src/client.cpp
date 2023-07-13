#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include <signal.h>

using namespace std;

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define MAX_MESSAGE_SIZE 4096

int client_socket;

void receiveMessages() {
    char buffer[MAX_MESSAGE_SIZE];

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_read = recv(client_socket, buffer, sizeof(buffer), 0);

        if (bytes_read <= 0) {
            cout << "Desconectado do servidor." << endl;
            close(client_socket);
            exit(0);
        }

        cout << buffer << endl;
    }
}

void sendMessage(const string& message) {
    if (send(client_socket, message.c_str(), message.size(), 0) == -1) {
        cerr << "Falha em enviar mensagem ao servidor." << endl;
    }
}

void handleSignal(int signal) {
    if (signal == SIGINT) {
        cout << "Fechando conexão..." << endl;
        close(client_socket);
        exit(0);
    }
}

int main() {
    signal(SIGINT, handleSignal);

    struct sockaddr_in server_addr;
    char buffer[MAX_MESSAGE_SIZE];

    // Criacao de socket
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Falha em criar o socket");
        exit(EXIT_FAILURE);
    }

    // Configurando endereço e porta do servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(PORT);

    // Conexao com servidor
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Falha em se conectar ao servidor");
        exit(EXIT_FAILURE);
    }

    cout << "Conectado ao servidor\n";

    // Start receiving thread
    thread receive_thread(receiveMessages);

    // Set an username
    string username;
    cout << "Insira seu nome de usuario: ";
    getline(cin, username);
    sendMessage(username);

    // Receive welcome message from server
    memset(buffer, 0, MAX_MESSAGE_SIZE);
    if (recv(client_socket, buffer, MAX_MESSAGE_SIZE, 0) <= 0) {
        std::cerr << "Erro ao receber mensagem de boas vindas do servidor." << std::endl;
        return -1;
    }

    cout << buffer << "\n\n";

    // Communication loop
    string message;
    while (true) {
        cout << "Insira sua mensagem: ";
        getline(cin, message);

        // Check if it's a command
        if (message == "/quit") {
            break;
        } else if (message == "/ping") {
            sendMessage("/ping");
            continue;
        } else if (message == "/connect") {
            cout << "Você ja esta conectado" << endl;
            continue;
        }

        // Send message to server
        sendMessage(message);
    }

    // Close socket
    close(client_socket);

    // Wait for receive thread to finish
    receive_thread.join();

    return 0;
}
