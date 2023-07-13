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
            cout << "Disconnected from server" << endl;
            close(client_socket);
            exit(0);
        }

        cout << buffer << endl;
    }
}

void sendMessage(const string& message) {
    if (send(client_socket, message.c_str(), message.size(), 0) == -1) {
        cerr << "Failed to send message to server" << endl;
    }
}

void handleSignal(int signal) {
    if (signal == SIGINT) {
        cout << "Closing connection..." << endl;
        close(client_socket);
        exit(0);
    }
}

int main() {
    signal(SIGINT, handleSignal);

    struct sockaddr_in server_addr;
    char buffer[MAX_MESSAGE_SIZE];

    // Create socket
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    // Set server address and port
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(PORT);

    // Connect to server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to connect to server");
        exit(EXIT_FAILURE);
    }

    cout << "Connected to server\n\n";

    // Start receiving thread
    thread receive_thread(receiveMessages);

    // Set an username
    string username;
    cout << "Enter your username: ";
    getline(cin, username);
    sendMessage(username);

    // Receive welcome message from server
    memset(buffer, 0, MAX_MESSAGE_SIZE);
    if (recv(client_socket, buffer, MAX_MESSAGE_SIZE, 0) <= 0) {
        std::cerr << "Error receiving welcome message from the server." << std::endl;
        return -1;
    }

    cout << buffer << "\n\n";

    // Communication loop
    string message;
    while (true) {
        cout << "Enter a message: ";
        getline(cin, message);

        // Check if it's a command
        if (message == "/quit") {
            break;
        } else if (message == "/ping") {
            sendMessage("/ping");
            continue;
        } else if (message == "/connect") {
            cout << "You are already connected" << endl;
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
