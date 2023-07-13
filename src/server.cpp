#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <vector>

using namespace std;

const int PORT = 8080;
const int BUFFER_SIZE = 4096;

struct ClientData {
    int clientSocket;
    string nickname;
    bool mute = false;
};

vector<ClientData> clients;

void broadcastMessage(const string& message) {
    for (const auto& client : clients) {
        string broadcastMessage = client.nickname + " > " + message + '\n';
        send(client.clientSocket, broadcastMessage.c_str(), broadcastMessage.length(), 0);
    }
}

void *clientThread(void *arg) {
    ClientData clientData = *(static_cast<ClientData*>(arg));
    int clientSocket = clientData.clientSocket;
    string clientNickname = clientData.nickname;
    char buffer[BUFFER_SIZE];

    // Enviar mensagem de boas-vindas para o cliente
    string welcomeMessage = "Bem-vindo ao chat! Você está conectado como: " + clientNickname;
    send(clientSocket, welcomeMessage.c_str(), welcomeMessage.length(), 0);

    while (true) {
        // Receber mensagem do cliente
        memset(buffer, 0, BUFFER_SIZE);
        if (recv(clientSocket, buffer, BUFFER_SIZE, 0) <= 0) {
            cerr << "Erro ao receber mensagem do cliente: " << clientNickname << endl;
            break;
        }

        string receivedMessage(buffer);

        if (receivedMessage == "/quit") {
            cout << "Cliente desconectado: " << clientNickname << endl;
            break;
        } else if (receivedMessage == "/ping") {
            // Enviar resposta de ping para o cliente
            string pingResponse = "pong\n";
            send(clientSocket, pingResponse.c_str(), pingResponse.length(), 0);
        }

        cout << "Cliente " << clientNickname << ": " << receivedMessage << endl;

        // Enviar mensagem para todos os clientes
        broadcastMessage(clientNickname + ": " + receivedMessage);
    }

    // Fechar o socket do cliente
    close(clientSocket);

    // Remover o cliente da lista de clientes conectados
    for (auto it = clients.begin(); it != clients.end(); ++it) {
        if (it->clientSocket == clientSocket) {
            clients.erase(it);
            break;
        }
    }

    delete static_cast<ClientData*>(arg);
    pthread_exit(NULL);
}

int main() {
    int serverSocket;
    struct sockaddr_in serverAddress;
    pthread_t threadId;
    char buffer[BUFFER_SIZE];

    // Criação do socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        cerr << "Erro ao criar o socket." << endl;
        return -1;
    }

    // Configuração do endereço do servidor
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(PORT);

    // Bind do socket ao endereço
    if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
        cerr << "Erro ao fazer o bind." << endl;
        return -1;
    }

    // Espera por conexões
    if (listen(serverSocket, 5) < 0) {
        cerr << "Erro ao esperar por conexões." << endl;
        return -1;
    }

    cout << "Aguardando conexões..." << endl;

    while (true) {
        int clientSocket;
        struct sockaddr_in clientAddress;
        socklen_t clientAddressLength = sizeof(clientAddress);

        // Aceita conexão do cliente
        clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddress, &clientAddressLength);
        if (clientSocket < 0) {
            cerr << "Erro ao aceitar conexão." << endl;
            break;
        }

        cout << "Conexão estabelecida." << endl;

        // Receber apelido do cliente
        memset(buffer, 0, BUFFER_SIZE);
        if (recv(clientSocket, buffer, BUFFER_SIZE, 0) <= 0) {
            cerr << "Erro ao receber apelido do cliente." << endl;
            break;
        }

        string clientNickname(buffer);

        cout << "Cliente conectado: " << clientNickname << endl;

        // Criar estrutura de dados para o cliente
        ClientData* clientData = new ClientData;
        clientData->clientSocket = clientSocket;
        clientData->nickname = clientNickname;

        // Adicionar cliente à lista de clientes conectados
        clients.push_back(*clientData);

        // Criar thread para lidar com o cliente
        if (pthread_create(&threadId, NULL, clientThread, static_cast<void*>(clientData)) != 0) {
            cerr << "Erro ao criar a thread para o cliente." << endl;
            break;
        }
    }

    // Fechar o socket do servidor
    close(serverSocket);

    return 0;
}
