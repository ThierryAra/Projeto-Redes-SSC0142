#include <iostream>
#include <pthread.h>
#include <signal.h>
#include "../libraries/server.hpp"
#include "../libraries/socket.hpp"

using namespace std;

struct sClientList {
    int socket;
    bool received;
    int attempts;
    ClientList *prev;
    ClientList *next;
    char ip[INET6_ADDRSTRLEN];
    char name[NICKNAME_SIZE];
    bool isAdmin;
    bool muted;
    char channels[MAX_CHANNELS][CHANNEL_NAME_SIZE];
    int numberOfChannels;
    ChannelList *activeChannel;
    ClientList *activeInstance;
    ClientList *mainNode;
};

typedef struct sSendInfo {
    ClientList *node;
    char *message;
    ChannelList *channelRoot;
} SendInfo;

typedef struct sThreadInfo {
    ClientList *clientRoot;
    ClientList *clientNode;
    ChannelList *channelRoot;
} ThreadInfo;

struct sChannelList {
    ChannelList *prev;
    ChannelList *next;
    char name[CHANNEL_NAME_SIZE];
    ClientList *clients;
};

// Criacao de novo no de cliente
ClientList *createClient(int sockFd, char *ip) {
    ClientList *newNode = (ClientList *) malloc(sizeof(ClientList));
    newNode->socket = sockFd;
    newNode->received = false;
    newNode->attempts = 0;
    newNode->prev = NULL;
    newNode->next = NULL;
    strcpy(newNode->ip, ip);
    strcpy(newNode->name, "\0");
    newNode->isAdmin = false;
    newNode->muted = false;
    newNode->activeChannel = NULL;
    newNode->mainNode = NULL;
    newNode->numberOfChannels = 0;

    for (int i = 0; i < MAX_CHANNELS; ++i){
        newNode->channels[i][0] = '\0';
    }

    return newNode;
}

// Criacao de novo no de canal
ChannelList *createChannelNode(char *name, ClientList *root) {
    ChannelList *newNode = (ChannelList *) malloc(sizeof(ChannelList));

    newNode->prev = NULL;
    newNode->next = NULL;
    strcpy(newNode->name, name);
    newNode->clients = root;

    return newNode;
}

// Desconectando um cliente de um canal ativo
void disconnectClient(ClientList *client, ChannelList *channel) {
    bool canCloseChannel = false;

    cout << "Desconectando " << client->name << endl;
    close(client->socket);

    // Removendo um cliente da lista de links duplos e mantendo os links
    if (client->next == NULL)
        client->prev->next = NULL;
    else {
        client->prev->next = client->next;
        client->next->prev = client->prev;
    }

    if (client->numberOfChannels > 0) {
        ChannelList *tmp = channel->next;
        for (int i = 0; i < MAX_CHANNELS; ++i){
            if (client->channels[i][0] != '\0') {
                while (tmp != NULL) {
                    // Encontrando o canal que o cliente esta deixando
                    if (!strcmp(client->channels[i], tmp->name)) {
                        ClientList *clientTmp = tmp->clients->next;
                        while (clientTmp != NULL) {
                            if (!strcmp(client->name, clientTmp->name))
                                clientTmp->prev->next = clientTmp->next;
                                if (clientTmp->next != NULL)
                                    clientTmp->next->prev = clientTmp->prev;
                                // Caso nao haja nenhum cliente sobrando nao pode ser deletado
                                else if (clientTmp->prev->prev == NULL)
                                    canCloseChannel = true;

                            free(clientTmp);
                            clientTmp = NULL;
                            break;
                        }

                        clientTmp = clientTmp->next;
                    }

                    // Deleting a channel because there's no more clients in it
                    client->channels[i][0] = '\0';
                    if (canCloseChannel)
                        deleteChannel(tmp);
                    break;
                }

                tmp = tmp->next;
            }
        }
    }

    free(client);
    client = NULL;
}

// Removendo todos os clientes de um canal ativo e fechando-o
void closeChannel(ChannelList *channel) {
    cout << "\nFechando canal " << channel->name << endl;

    ClientList *tmpClient;
    ClientList *root = channel->clients;

    // Femovendo os nos presentes no canal
    while (root != NULL) {
        cout << "\nRemovendo " << root->name << " do canal." << endl;
        tmpClient = root;
        root = root->next;
        free(tmpClient);
        tmpClient = NULL;
    }
}

// Delecao de canal vazio
void deleteChannel(ChannelList *channel) {
    cout << "Deletando " << channel->name << endl;

    // Mantaining the links of the double linked list
    if (channel->next == NULL)
        channel->prev->next = NULL;
    else {
        channel->prev->next = channel->next;
        channel->next->prev = channel->prev;
    }

    free(channel);
    channel = NULL;
}

//Em caso de falha, printa mensagem de erro e fecha conexao
void errorMessage(const char *message) {
    perror(message);
    exit(1);
}

// Ignorando 'crtlCHandler' padrao
void ctrlCHandler(int sig) {
    cout << "Para sair, use /q" << endl;
}

// Adiciona um "newChar" no final de uma string 
void addEndStr(char *str, char newchar) {
    int size = strlen(str);
    if (newchar == str[size - 1])
        return;
    if ((isalnum(str[size - 1])) || ispunct(str[size - 1]))
        str[size] = newchar;
    else
        str[size - 1] = newchar;
}

// Fechamento do servidor
void *closeServer(void *server) {
    ClientList *client = ((ThreadInfo *)server)->clientRoot;
    ChannelList *channel = ((ThreadInfo *)server)->channelRoot;
    ClientList *tmpClient;
    ChannelList *tmpChannel;

    // Ouvindo a aplicao server side
    while (true) {
        char input[MESSAGE_SIZE];
        cin >> input;

        // Fechando todos os canais que possuem clientes ativos
        if (!strcmp(input, "/q")) {
            while (channel != NULL) {
                closeChannel(channel);
                tmpChannel = channel;
                channel = channel->next;
                free(tmpChannel);
                tmpChannel = NULL;
            }

            // Fechando todos os sockets dos clientes
            while (client != NULL) {
                cout << "\nFechando socktfd: " << client->socket << endl;
                close(client->socket);
                tmpClient = client;
                client = client->next;
                free(tmpClient);
                tmpClient = NULL;
            }

            free(server);
            server = NULL;
            cout << "Fechando servidor..." << endl;
            exit(0);
        } else {
            cout << "Comando desconhecido" << endl;
            cout << "\tLista de comandos:" << endl << "Quit: /q" << endl;
        }
    }
}

// Envio de mensagem do servidor a um cliente
void serverMessage(ChannelList *channel, ClientList *client, char message[]) {
    // Definindo o thread com a mensagem, o cliente e o canal que ele precisa seguir
    pthread_t sendThread;
    SendInfo *sendInfo = (SendInfo *) malloc(sizeof(SendInfo));
    sendInfo->node = client;
    sendInfo->channelRoot = channel;
    sendInfo->message = (char *) malloc(sizeof(char) * (MESSAGE_SIZE + NICKNAME_SIZE + CHANNEL_NAME_SIZE + 5));
    strcpy(sendInfo->message, message);

    // Se não houver problema na criação do thread a mensagem é enviada ao cliente
    if (pthread_create(&sendThread, NULL, sendMessage, (void *)sendInfo))
        errorMessage("Erro na criacao de Thread");
    pthread_detach(sendThread);
}

// Tenta enviar 5 mensagens ao cliente. Se todas falharem desconecta o cliente
void *sendMessage(void *info) {
    SendInfo *sendInfo = (SendInfo *)info;
    int snd;

    do {
        snd = send(sendInfo->node->socket, sendInfo->message, MESSAGE_SIZE + NICKNAME_SIZE + CHANNEL_NAME_SIZE + 5, 0);
        usleep(WAIT_ACK);
    } while (snd >= 0 && !sendInfo->node->mainNode->received && sendInfo->node->mainNode->attempts < 5);

    if (sendInfo->node->mainNode->attempts == 5 || snd < 0) {
        disconnectClient(sendInfo->node, sendInfo->channelRoot);
    } else if(sendInfo->node->mainNode->received) {
        sendInfo->node->mainNode->received = false;
        sendInfo->node->mainNode->attempts = 0;
        free(sendInfo->message);
        free(sendInfo);
    }
}

// Envio de mensagem para todos os clientes
void sendAllClients(ChannelList *channel, ClientList *clients, ClientList *client, char message[]) {
    ClientList *tmp = clients->next;
    int snd;

    // Se não houver problema na criação do thread a mensagem é enviada ao cliente
    while (tmp != NULL) {
        if (client->socket != tmp->socket) {
            cout << "Enviar para: " << tmp->name << " >> " << message;

            // Usa thread para enviar a mensagem do cliente para todos os clientes no mesmo canal
            pthread_t sendThread;
            SendInfo *sendInfo = (SendInfo *) malloc(sizeof(SendInfo));
            sendInfo->node = tmp;
            sendInfo->channelRoot = channel;
            sendInfo->message = (char *) malloc(sizeof(char) * (MESSAGE_SIZE + NICKNAME_SIZE + CHANNEL_NAME_SIZE + 5));
            strcpy(sendInfo->message, message);

            if (pthread_create(&sendThread, NULL, sendMessage, (void *)sendInfo) != 0)
                errorMessage("Erro na criacao de Thread");
            pthread_detach(sendThread);
        }
        tmp = tmp->next;
    }
}

// Cria um novo canal, se necessário, muda o canal do cliente para o selecionado
void joinChannel(char *channelName, ChannelList *channel, ClientList *client) {
    ChannelList *tmp = channel;
    bool createNewChannel = true;

    // Procura por existencia previa do canal
    while (tmp->next != NULL) {
        if (!strcmp(channelName, tmp->next->name)) {
            createNewChannel = false;
            tmp = tmp->next;
            break;
        }

        tmp = tmp->next;
    }

    // Criando um novo canal
    char message[MESSAGE_SIZE];
    if (createNewChannel) {
        // Reached the max number of channels
        if (client->mainNode->numberOfChannels == MAX_CHANNELS) {
            sprintf(message, "Nao foi possivel criar o canal '%s'. Limite de no numero canais atingido.\n", channelName);
            serverMessage(channel, client->mainNode, message);
            return;
        }

        // Criando um novo no para um novo canal
        ClientList *newClient = createClient(client->socket, client->ip);
        newClient->mainNode = client->mainNode;
        strcpy(newClient->name, client->mainNode->name);
        newClient->isAdmin = true;

        ClientList *rootNode = createClient(0, "0");
        strcpy(rootNode->name, "root");
        ChannelList *newChannel = createChannelNode(channelName, rootNode);
        tmp->next = newChannel;
        newChannel->prev = tmp;

        newChannel->clients->next = newClient;
        newClient->prev = newChannel->clients;

        // Trocando canal do cliente
        client->mainNode->activeChannel = newChannel;
        client->mainNode->activeInstance = newClient;

        for (int i = 0; i < MAX_CHANNELS; ++i) {
            if (client->channels[i][0] == '\0') {
                strcpy(client->mainNode->channels[i], channelName);
                break;
            }
        }
        client->mainNode->numberOfChannels++;

        sprintf(message, "/channel %s Criado. Voce foi tranferido para este canal.", channelName);
        serverMessage(channel, client->mainNode, message);
    } else { // O canal ja existe, basta trocar o canal ativo do cliente
        bool isInChannel = false;
        for (int i = 0; i < MAX_CHANNELS; ++i) {
            if (!strcmp(client->channels[i], tmp->name)) {
                isInChannel = true;
                client->mainNode->activeChannel = tmp;

                ClientList *instance = tmp->clients;
                while (instance != NULL) {
                    if (!strcmp(instance->name, client->name)) {
                        client->mainNode->activeInstance = instance;
                        break;
                    }

                    instance = instance->next;
                }

                sprintf(message, "/channel %s Transferido para o canal.", channelName);
                serverMessage(channel, client->mainNode, message);

                break;
            }
        }

        // O canal existe, mas o cliente não está no canal, anexe o cliente ao canal
        if (!isInChannel) {
            if (client->mainNode->numberOfChannels == MAX_CHANNELS) {
                sprintf(message, "Nao pode se juntar %s. Limite de canais alcançado.\n", channelName);
                serverMessage(channel, client->mainNode, message);
                return;
            }

            ClientList *newClient = createClient(client->socket, client->ip);
            newClient->mainNode = client->mainNode;
            strcpy(newClient->name, client->mainNode->name);

            newClient->next = tmp->clients->next->next;
            newClient->prev = tmp->clients->next;
            if (tmp->clients->next->next != NULL)
                tmp->clients->next->next->prev = newClient;
            tmp->clients->next->next = newClient;

            client->mainNode->activeChannel = tmp;
            client->mainNode->activeInstance = newClient;

            for (int i = 0; i < MAX_CHANNELS; ++i) {
                if (client->mainNode->channels[i][0] == '\0') {
                    strcpy(client->mainNode->channels[i], channelName);
                    break;
                }
            }

            client->mainNode->numberOfChannels++;

            sprintf(message, "/channel %s Voce se juntou ao canal.", channelName);
            serverMessage(channel, newClient->mainNode, message);

            sprintf(message, "%s - %s se juntou ao canal.\n", channelName, newClient->name);
            sendAllClients(channel, tmp->clients, newClient->mainNode, message);
        }
    }
}

// Informando ao administrador de um canal o IP de um cliente específico
bool whoIs(ClientList *admin, char *username) {
    char buffer[MESSAGE_SIZE] = {};
    ClientList *tmp = admin->mainNode->activeChannel->clients->next;

    // Busca por cliente requisitado
    while (tmp != NULL) {
        if (!strcmp(tmp->name, username)) {
            sprintf(buffer, "%s - Usuario(%s): %s\n", admin->mainNode->activeChannel->name, username, tmp->ip);
            int snd = send(admin->socket, buffer, MESSAGE_SIZE, 0);
            if (snd < 0)
                return false;
            return true;
        }

        tmp = tmp->next;
    }

    // O cliente especificado nao esta no canal
    sprintf(buffer, "Usuario '%s' nao esta no canal\n", username);
    int snd = send(admin->socket, buffer, MESSAGE_SIZE, 0);
    if (snd < 0)
        return false;
    return true;
}

// Permite ao administrador ativar/desativar um cliente no canal
void muteUser(ChannelList *channel, ClientList *admin, char *username, bool mute) {
    ClientList *tmp = admin->mainNode->activeChannel->clients->next;
    char message[MESSAGE_SIZE] = {};

    // Busca por cliente
    while (tmp != NULL) {
        if (!strcmp(tmp->name, username)) {
            tmp->muted = mute;
            if (mute) {
                sprintf(message, "%s - Voce foi mutado(a) por %s.\n", admin->mainNode->activeChannel->name, admin->name);
                serverMessage(channel, tmp->mainNode, message);

                sprintf(message, "%s - %s foi mutado(a).\n", admin->mainNode->activeChannel->name, tmp->name);
                serverMessage(channel, admin->mainNode, message);
            } else {
                sprintf(message, "%s - Voce foi desmutado(a) por %s.\n", admin->mainNode->activeChannel->name, admin->name);
                serverMessage(channel, tmp->mainNode, message);

                sprintf(message, "%s - %s foi desmutado(a).\n", admin->mainNode->activeChannel->name, tmp->name);
                serverMessage(channel, admin->mainNode, message);
            }

            return;
        }
        tmp = tmp->next;
    }

    // If the client isn't in the channel
    sprintf(message, "Usuario '%s' nao esta no canal.\n", username);
    send(admin->socket, message, MESSAGE_SIZE, 0);
}

// Permite que o administrador expulse um cliente do canal
void kickUser(ChannelList *channel, ClientList *admin, char *username) {
    bool canCloseChannel = false;
    ClientList *tmp = admin->mainNode->activeChannel->clients->next->next;
    char message[MESSAGE_SIZE];

    // Encontrando cliente
    while (tmp != NULL) {
        if (!strcmp(tmp->name, username)) {
            tmp->prev->next = tmp->next;
            if (tmp->next != NULL)
                tmp->next->prev = tmp->prev;
            else if (tmp->prev->prev == NULL)
                canCloseChannel = true;

            for (int i = 0; i < MAX_CHANNELS; ++i) {
                if (!strcmp(tmp->mainNode->channels[i], admin->mainNode->activeChannel->name)) {
                    tmp->mainNode->channels[i][0] = '\0';
                    break;
                }
            }

            // Envio de mensagem ao cliente expulso
            sprintf(message, "/channel #none Você foi expulso do canal %s por %s. Transferido para", admin->mainNode->activeChannel->name, admin->name);
            serverMessage(channel, tmp->mainNode, message);

            // Mensagem de aos clientes remanescentes 
            sprintf(message, "%s - %s foi expulso(a) do canal.\n", admin->mainNode->activeChannel->name, tmp->name);
            sendAllClients(channel, admin->mainNode->activeChannel->clients, tmp->mainNode, message);

            tmp->mainNode->activeChannel = channel;
            tmp->mainNode->activeInstance = tmp->mainNode;

            tmp->mainNode->numberOfChannels--;

            free(tmp);
            tmp = NULL;
            if (canCloseChannel)
                deleteChannel(admin->mainNode->activeChannel);
            return;
        }

        tmp = tmp->next;
    }

    sprintf(message, "User '%s' is not on this channel\n", username);
    send(admin->socket, message, MESSAGE_SIZE, 0);
}

// O cliente pode deixar o servidor especificado, mas ainda continua conectado ao servidor
void leaveChannel(ChannelList *channel, ClientList *client, char *channelName) {
    char message[MESSAGE_SIZE];
    bool canCloseChannel = false;

    ChannelList *tmpChannel = channel->next;

    while (tmpChannel != NULL) {
        if (!strcmp(tmpChannel->name, channelName))
            break;
        
        tmpChannel = tmpChannel->next;
    }

    if (tmpChannel != NULL) {
        ClientList *tmp = tmpChannel->clients->next;
        while (tmp != NULL) {
            if (!strcmp(client->name, tmp->name)) {
                tmp->prev->next = tmp->next;
                if (tmp->next != NULL)
                    tmp->next->prev = tmp->prev;
                else if (tmp->prev->prev == NULL)
                    canCloseChannel = true;
            }

            for (int i = 0; i < MAX_CHANNELS; ++i) {
                if (!strcmp(tmp->mainNode->channels[i], tmpChannel->name)) {
                    // Se o canal ainda existir e o administrador o abandonar, altere o administrador
                    if (tmpChannel->clients->next != NULL)
                        tmpChannel->clients->next->isAdmin = true;
                    tmp->mainNode->channels[i][0] = '\0';
                    break;
                }
            }

            // Mensagem de aviso ao cliente que deixou o canal
            sprintf(message, "Voce saiu do canal %s.\n", tmpChannel->name);
            serverMessage(channel, tmp->mainNode, message);

            tmp->mainNode->numberOfChannels--;

            // Clientes que ficaram no canal mandem mensagem
            sprintf(message, "%s - %s saiu do canal.\n", tmpChannel->name, tmp->name);
            sendAllClients(channel, tmpChannel->clients, tmp->mainNode, message);

            if (!strcmp(client->mainNode->activeChannel->name, channelName)) {
                tmp->mainNode->activeChannel = channel;
                tmp->mainNode->activeInstance = tmp->mainNode;

                sprintf(message, "/channel #none Voce saiu do canal %s. Transferido para", tmpChannel->name);
                serverMessage(channel, tmp->mainNode, message);
            }

            free(tmp);
            tmp = NULL;
            if (canCloseChannel)
                deleteChannel(tmpChannel);
            return;
        }

        tmp = tmp->next;
    } else {
        sprintf(message, "Esse canal não existe.\n");
        serverMessage(channel, client->mainNode, message);
    }
}

// Thread fpara receber mensagens do cliente
void *clientHandler(void *info) {
    int leaveFlag = 0;
    char recvBuffer[MESSAGE_SIZE] = {};
    char sendBuffer[MESSAGE_SIZE + NICKNAME_SIZE + CHANNEL_NAME_SIZE + 5] = {};
    ThreadInfo *tInfo = (ThreadInfo *)info;
    char message[MESSAGE_SIZE] = {};
    char command[MESSAGE_SIZE] = {};
    char argument[MESSAGE_SIZE] = {};

    cout << tInfo->clientNode->name << " (" << tInfo->clientNode->ip << ")"
         << " (" << tInfo->clientNode->socket << ")"
         << " entrou no servidor.\n";
    sprintf(sendBuffer, "Servidor: %s entrou no servidor.     \n", tInfo->clientNode->name);
    sendAllClients(tInfo->channelRoot, tInfo->clientRoot, tInfo->clientNode, sendBuffer);

    while (true) {
        if (leaveFlag)
            break;

        bzero(recvBuffer, MESSAGE_SIZE);
        bzero(sendBuffer, MESSAGE_SIZE + NICKNAME_SIZE + CHANNEL_NAME_SIZE + 5);
        bzero(command, MESSAGE_SIZE);
        bzero(argument, MESSAGE_SIZE);

        int rcv = recv(tInfo->clientNode->socket, recvBuffer, MESSAGE_SIZE, 0);

        if (rcv <= 0) {
            leaveFlag = 1;
            continue;
        }

        // Verifica o comando que o cliente está solicitando para o servidor
        if (recvBuffer[0] == '/') {
            sscanf(recvBuffer, "%s %s", command, argument);
            addEndStr(command, '\0');
            addEndStr(argument, '\0');

            if (!strcmp(command, "/ack")) {
                tInfo->clientNode->received = true;
                tInfo->clientNode->attempts = 0;
                cout << tInfo->clientNode->name << " recebeu a mensagem" << endl;
            } else if (!strcmp(command, "/q")) {
                cout << tInfo->clientNode->name << " (" << tInfo->clientNode->ip << ")"
                     << " (" << tInfo->clientNode->socket << ")"
                     << " saiu do servidor.\n";
                sprintf(sendBuffer, "Servidor: %s saiu do servidor.     \n", tInfo->clientNode->name);
                sendAllClients(tInfo->channelRoot, tInfo->clientRoot, tInfo->clientNode, sendBuffer);
                leaveFlag = 1;
            } else if (!strcmp(command, "/p")) {
                sprintf(sendBuffer, "Servidor: pong\n");
                serverMessage(tInfo->channelRoot, tInfo->clientNode, sendBuffer);
            } else if (!strcmp(command, "/j")) {
                if (argument[0] == '&' || argument[0] == '#') {
                    joinChannel(argument, tInfo->channelRoot, tInfo->clientNode);
                } else {
                    sprintf(message, "Forma de nome invalido. O nome do canal precisa começar com '#' ou '&'.\n.");
                    serverMessage(tInfo->channelRoot, tInfo->clientNode, message);
                }
            } else if (!strcmp(command, "/l")) {
                if (argument[0] == '&' || argument[0] == '#') {
                    leaveChannel(tInfo->channelRoot, tInfo->clientNode, argument);
                } else {
                    sprintf(message, "Forma de nome invalido. O nome do canal precisa começar com '#' ou '&'.\n.");
                    serverMessage(tInfo->channelRoot, tInfo->clientNode, message);
                }
            } else if (!strcmp(command, "/w")) {
                if (tInfo->clientNode->activeInstance->isAdmin) {
                    if (!whoIs(tInfo->clientNode->activeInstance, argument)) {
                        leaveFlag = 1;
                    }
                } else {
                    sprintf(message, "Comando invalido. Você nao e o administrador deste canal.\n");
                    serverMessage(tInfo->channelRoot, tInfo->clientNode->activeInstance, message);
                }
            } else if (!strcmp(command, "/k")) {
                if (tInfo->clientNode->activeInstance->isAdmin) {
                    kickUser(tInfo->channelRoot, tInfo->clientNode->activeInstance, argument);
                } else {
                    sprintf(message, "Comando invalido. Você nao e o administrador deste canal.\n");
                    serverMessage(tInfo->channelRoot, tInfo->clientNode->activeInstance, message);
                }
            } else if (!strcmp(command, "/m")) {
                if (tInfo->clientNode->activeInstance->isAdmin) {
                    muteUser(tInfo->channelRoot, tInfo->clientNode->activeInstance, argument, true);
                } else {
                    sprintf(message, "Comando invalido. Você nao e o administrador deste canal.\n");
                    serverMessage(tInfo->channelRoot, tInfo->clientNode->activeInstance, message);
                }
            } else if (!strcmp(command, "/um")) {
                if (tInfo->clientNode->activeInstance->isAdmin) {
                    muteUser(tInfo->channelRoot, tInfo->clientNode->activeInstance, argument, false);
                } else {
                    sprintf(message, "Comando invalido. Você nao e o administrador deste canal.\n");
                    serverMessage(tInfo->channelRoot, tInfo->clientNode->activeInstance, message);
                }
            } else if (!strcmp(command, "/h")) {
                sprintf(message,
                        "    Comandos de usuario:\n/q = Sair\n/p = Checar conexao\n/j channelName = Entrar em um canal\n/l channelName = Deixar um canal\n\n    Comandos do ADM:\n/w user = Mostra IP de usuario\n/m user = Desativar as mensagens do usuário no canal\n/um user = Habilita as mensagens do usuário no canal\n/k user = Expulsa usuário do canal\n");
                serverMessage(tInfo->channelRoot, tInfo->clientNode->activeInstance, message);
            } else {
                sprintf(message, "Comando desconhecido Use /h para ver a lista de comandos disponíveis.\n");
                serverMessage(tInfo->channelRoot, tInfo->clientNode->activeInstance, message);
            }
        } else if (!tInfo->clientNode->activeInstance->muted) {
            sprintf(sendBuffer, "%s - %s: %s", tInfo->clientNode->activeChannel->name, tInfo->clientNode->name, recvBuffer);
            sendAllClients(tInfo->channelRoot, tInfo->clientNode->activeChannel->clients, tInfo->clientNode->activeInstance, sendBuffer);
        }
    }

    // Se deixar o sinalizador se tornar verdadeiro, o cliente será desconectado
    disconnectClient(tInfo->clientNode, tInfo->channelRoot);
    free(tInfo);
}

int main() {
    int serverFd = 0, clientFd = 0;
    int opt = 1;
    char nickname[NICKNAME_SIZE] = {};
    ThreadInfo *info = NULL;

    signal(SIGINT, ctrlCHandler);

    // Cria descritor de arquivo para o soquete
    if ((serverFd = socket(AF_INET, SOCK_STREAM, PROTOCOL)) < 0)
        errorMessage("Socket failed.");
    
    // Forçando o socket a ser conectado na porta 8080
    if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
        errorMessage("setsockopt");

    struct sockaddr_in serverAddr, clientAddr;
    socklen_t serverAddrSize = sizeof(serverAddr);
    socklen_t clientAddrSize = sizeof(clientAddr);
    bzero((char *)&serverAddr, serverAddrSize);
    bzero((char *)&clientAddr, clientAddrSize);

    // Configurando a estrutura do serverAdd para ser usado na chamada do bind
    // Ordem dos bytes do servidor
    serverAddr.sin_family = AF_INET;

    // Preencha automaticamente com o endereço IP do host atual
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // Convertendo o valor short int do host para a ordem de byte da rede
    serverAddr.sin_port = htons(PORT);

    // Vincula o soquete ao endereço IP atual na porta
    if (bind(serverFd, (struct sockaddr *)&serverAddr, serverAddrSize) < 0)
        errorMessage("Falha na vinculacao");

    // Dizendo ao soquete para ouvir as conexões de entrada. O tamanho máximo para a fila de pendências é 5
    if (listen(serverFd, MAX_CONNECTIONS) < 0)
        errorMessage("Ouvindo");

    // Imprimindo o IP do servidor
    getsockname(serverFd, (struct sockaddr *)&serverAddr, (socklen_t *)&serverAddrSize);
    cout << "Iniciar servidor em: " << inet_ntoa(serverAddr.sin_addr) << ": " << ntohs(serverAddr.sin_port) << endl;

    // Criando a lista dupla encadeada inicial para os clientes, a raiz(root) é o servidor
    ClientList *clientRoot = createClient(serverFd, inet_ntoa(serverAddr.sin_addr));

    ClientList *channelRootAdmin = createClient(0, "0");
    strcpy(channelRootAdmin->name, "root");
    ChannelList *channelRoot = createChannelNode("#root", channelRootAdmin);

    info = (ThreadInfo *) malloc(sizeof(ThreadInfo));
    info->clientRoot = clientRoot;
    info->clientNode = NULL;
    info->channelRoot = channelRoot;

    // Thread that will catch the server input, used to catch the quit command '/q'
    pthread_t inputThreadId;
    if (pthread_create(&inputThreadId, NULL, closeServer, (void *)info) != 0)
        errorMessage("Erro em thread de entrada");
    
    // Servidor em funcionamento aceitando novos clientes
    while (true) {
        if ((clientFd = accept(serverFd, (struct sockaddr *)&clientAddr, &clientAddrSize)) < 0) {
            cout << "Erro de aceitacao do cliente" << endl;
            continue;
        }

        // Printando IP do cliente
        cout << "Cliente " << inet_ntoa(clientAddr.sin_addr) << " : " << ntohs(clientAddr.sin_port) << " entrou." << endl;

        // Cria um novo cleinte e adiona ele a lista
        ClientList *node = createClient(clientFd, inet_ntoa(clientAddr.sin_addr));
        node->mainNode = node;
        node->activeInstance = node;
        node->muted = true;

        // Nomeando cliente
        recv(node->socket, nickname, NICKNAME_SIZE, 0);
        strcpy(node->name, nickname);

        ClientList *last = clientRoot;
        while (last->next != NULL) {
            last = last->next;
        }

        node->prev = last;
        last->next = node;

        bzero(nickname, NICKNAME_SIZE);

        info = (ThreadInfo *) malloc(sizeof(ThreadInfo));
        info->clientRoot = clientRoot;
        info->clientNode = node;
        info->channelRoot = channelRoot;

        // Cada cliente possui uma nova thread
        pthread_t id;
        if (pthread_create(&id, NULL, clientHandler, (void *)info)) {
            cout << "Erro na criacao de Thread" << endl;

            // Se necessario, remover cliente
            disconnectClient(node, channelRoot);
            pthread_detach(id);
        }
    }

    return 0;
}
