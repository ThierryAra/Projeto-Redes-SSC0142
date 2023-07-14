#include <iostream>
#include <pthread.h>
#include <signal.h>
#include "../libraries/client.hpp"
#include "../libraries/socket.hpp"

using namespace std;

char nickname[MESSAGE_SIZE] = {};
char channel[CHANNEL_NAME_SIZE] = {};

// Print de mensagem de erro e fechamento do socket em caso de falha
void errorMessage(int sock, const char *message) {
    perror(message);
    close(sock);
    exit(1);
}

// Fecha conexão com socket
void quit(int sock) {
    cout << "Saindo..." << endl;
    close(sock);
    exit(0);
}

// Ignorando 'crtlCHandler' padrao
void ctrlCHandler(int signal) {
    cout << "Para sair do programa use /q" << endl;
}

// Print de nome de usuário
void printNickname() {
    cout << "\r" << channel << " - " << nickname << endl;
    fflush(stdout);
}

// Adiciona um "newChar" no final de uma string 
void addEndStr(char *str, char newChar) {
    int size = strlen(str);

    if (newChar == str[size - 1]) return;
    if ((isalnum(str[size - 1])) || ispunct(str[size - 1])) str[size] = newChar;
    else str[size - 1] = newChar;
}

// Thread para receber mensagens do servidor
void *receiveMessageHandler(void *sock) {
    char buffer[MESSAGE_SIZE + NICKNAME_SIZE + CHANNEL_NAME_SIZE + 5] = {};
    int rcv;
    while (true) {
        rcv = recv(*(int *)sock, buffer, MESSAGE_SIZE + NICKNAME_SIZE + CHANNEL_NAME_SIZE + 5, 0);
        
        // Caso usuario perca conexao
        if (rcv == 0) {
            cout << "\rPerda de conexão com servidor..." << endl;
            fflush(stdout);
        
            quit(*(int *)sock);
        }
        
        // Caso haja algum erro lendo a mensagem
        if (rcv < 0) errorMessage(*(int *)sock, "Erro de leitura");
        
        // Caso de mudança dos canais do usuario (servidor envia uma "notificação" se a operação foi bem sucedida)
        if (buffer[0] == '/') {
            char message[MESSAGE_SIZE];
            sscanf(buffer, "/channel %s %[^\n]", channel, message);
            cout << "\r" << message << " " << channel;
            printf("%*c\n", 20, ' ');
            fflush(stdout);

            cout << "\r";
            fflush(stdout);
            printNickname();
        } else {
            addEndStr(buffer, '\0');
            cout << "\r" << buffer;
            printf("%*c\n", 20, ' ');
            fflush(stdout);
            printNickname();
        }

        // Confirmação de mensagem recebida
        bzero(buffer, MESSAGE_SIZE + NICKNAME_SIZE + CHANNEL_NAME_SIZE + 5);
        strcpy(buffer, "/ack");
        
        // Caso haja algum erro de escrita da mensagem
        if (send(*(int *)sock, buffer, strlen(buffer), MSG_DONTWAIT) < 0) 
            errorMessage(*(int *)sock, "Erro de escrita");
    }
}

// Thread para enviar mensagens ao servidor
void *sendMessageHandler(void *sock) {
    char buffer[MESSAGE_SIZE] = {};
    string message;
    int snd;
    while (true) {
        printNickname();
        getline(cin, message);
        int div = (message.length() > (MESSAGE_SIZE - 1)) ? (message.length() / (MESSAGE_SIZE - 1)) : 0;
    	
        // Se necessario, manda as mensagnes em blocos
        for (int i = 0; i <= div; i++) {
            bzero(buffer, MESSAGE_SIZE);
            message.copy(buffer, MESSAGE_SIZE - 1, (i * (MESSAGE_SIZE - 1)));

            if (buffer[0] != '/') addEndStr(buffer, '\n');
            
            if (send(*(int *)sock, buffer, strlen(buffer), MSG_DONTWAIT) < 0)
                errorMessage(*(int *)sock, "Erro de escrita");
        }

        // Desconexão do usuário (apos notificar o servidor)
        if (strcmp(buffer, "/q") == 0) quit(*(int *)sock);
    }
}

int main() {
    struct sockaddr_in serverAddr, clientAddr;
    int serverAddrLen = sizeof(serverAddr);
    int clientAddrLen = sizeof(clientAddr);
    int sock = 0;
    char ip[INET6_ADDRSTRLEN] = {};
    char buffer[MESSAGE_SIZE] = {};
    char command[MESSAGE_SIZE] = {};

    strcpy(channel, "#none");
    bzero(nickname, NICKNAME_SIZE);
    bzero(buffer, MESSAGE_SIZE);
    bzero((char *)&clientAddr, clientAddrLen);
    bzero((char *)&serverAddr, serverAddrLen);

    // Configurando nome de usuario
    do {
        cout << "Digite seu nome (1~50 letras)" << endl << "Uso: /user <username>" << endl;
        if (fgets(buffer, MESSAGE_SIZE - 1, stdin) != NULL && buffer[0] == '/') {
            sscanf(buffer, "%s %s", command, nickname);
            
            if (strcmp(command, "/user") == 0) addEndStr(nickname, '\0');
            else cout << "Comando invalido" << endl;
        }
    } while(strlen(nickname) < 1 || strlen(nickname) > NICKNAME_SIZE - 1);

    // Configurando conexao
    if ((sock = socket(AF_INET, SOCK_STREAM, PROTOCOL)) < 0)
        errorMessage(sock, "\nErro de criacao do socket\n");
    
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    int addr = 1;
    
    do {
        if (addr <= 0) cout << "Endereco invalido ";

        cout << "Por favor, digite o endereco do servidor (endereco padrao: " << LOCALHOST << ")." << endl << "Por padrão, digite '/': ";

        if (fgets(ip, MESSAGE_SIZE - 1, stdin) != NULL) addEndStr(ip, '\0');
        if (!strcmp(ip, "/")) addr = inet_pton(AF_INET, LOCALHOST, &serverAddr.sin_addr);
        else addr = inet_pton(AF_INET, ip, &serverAddr.sin_addr);

    } while(addr <= 0);

    signal(SIGINT, ctrlCHandler);

    // Usuario agora pode conectar com o servidor ou sair
    cout << endl << "Conectar: /c" << endl << "Sair: /q" << endl;
    while (true) {
        if (fgets(buffer, 12, stdin) != NULL) addEndStr(buffer, '\0');

        if (!strcmp(buffer, "/c")) break;
        else if (!strcmp(buffer, "/q")) quit(sock);
        else cout << "Comando desconhecido" << endl;
    }

    // Caso usuario continue com a conexao
    if (connect(sock, (struct sockaddr *)&serverAddr, serverAddrLen) < 0)
        errorMessage(sock, "\nErro de conexao");

    getsockname(sock, (struct sockaddr *)&clientAddr, (socklen_t *)&clientAddrLen);

    clientAddr.sin_port = htons(PORT);
    cout << "Conectado ao servidor: " << inet_ntoa(serverAddr.sin_addr) << ": " << ntohs(serverAddr.sin_port) << endl;
    cout << "Voce eh: " << inet_ntoa(clientAddr.sin_addr) << ": " << ntohs(clientAddr.sin_port) << endl;
    
    send(sock, nickname, NICKNAME_SIZE, 0);

    // Depois da conexao, comeca envio e recebimento de threads
    pthread_t receiveMessageThread;
    if (pthread_create(&receiveMessageThread, NULL, receiveMessageHandler, &sock) != 0)
        errorMessage(sock, "\nCreate pthread error");
    
    pthread_t sendMessageThread;
    if (pthread_create(&sendMessageThread, NULL, sendMessageHandler, &sock) != 0)
        errorMessage(sock, "\nCreate pthread error");

    pthread_join(receiveMessageThread, NULL);
    pthread_join(sendMessageThread, NULL);

    return 0;
}