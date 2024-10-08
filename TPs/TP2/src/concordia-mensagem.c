#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../include/struct.h"

void send_to_deamon(ConcordiaRequest *request){
    int fd = open(FIFO, O_WRONLY);
    if (fd == -1){
        perror("Erro ao abrir o pipe para requests");
        exit(EXIT_FAILURE);
    }


    if(write(fd, request, sizeof(ConcordiaRequest)) == -1){
        perror("Falha ao enviar o request");
        close(fd);
        exit(EXIT_FAILURE);
    }

    close(fd);
    printf("Efetuando o seu pedido.\n");
}

void read_from_daemon(){
    char fifoName[128];
    snprintf(fifoName, sizeof(fifoName), "/var/lib/concordia/fifos/fifo_%d", getpid());

    if (mkfifo(fifoName, 0660) == -1) {
        perror("Error creating return FIFO \n");
    }

    int fd2 = open(fifoName, O_RDONLY);
    if(fd2 == -1){
        perror("Error opening FIFO");
        return;
    }

    ssize_t bytes_read;
    char databuffer[MSG_SIZE];

    if((bytes_read = read(fd2, databuffer, sizeof(databuffer))) > 0){
        printf("%s\n", databuffer);
    }

    unlink(fifoName);
}

char* obter_usuario_atual() {
    char* usuario = getenv("USER");
    return usuario;
}

void enviar_mensagem(char *dest, char *msg, ConcordiaRequest *request) {
    request->flag = MENSAGEM;
    snprintf(request->command, COMMAND_SIZE, "enviar");
    snprintf(request->dest, usersize, "%s", dest);
    snprintf(request->msg, MSG_SIZE, "%s", msg);
    char *user = obter_usuario_atual();
    snprintf(request->user, usersize,"%s", user);

    printf("Enviando mensagem para %s\n", dest);
    send_to_deamon(request);

    read_from_daemon();
}

void listar_mensagens(int all, ConcordiaRequest *request) {
    request->flag = MENSAGEM;
    snprintf(request->command, COMMAND_SIZE, "listar");
    request->all_mid = all;
    char *user = obter_usuario_atual();
    snprintf(request->user, usersize,"%s", user);

    send_to_deamon(request);

    char fifoName[128];
    snprintf(fifoName, sizeof(fifoName), "/var/lib/concordia/fifos/fifo_%d", getpid());

    // printf("fifoName %s", fifoName);

    if (mkfifo(fifoName, 0660) == -1) {
        perror("Error creating return FIFO \n");
    }

    int fd2 = open(fifoName, O_RDONLY);
    if(fd2 == -1){
        perror("Error opening FIFO");
        return;
    }

    ssize_t bytes_read;
    char databuffer[12];
    int msgLength;
    if((bytes_read = read(fd2, databuffer, sizeof(databuffer))) > 0){
        if(strcmp(databuffer, "No messages") == 0){
            printf("No messages to list\n");
            close(fd2);
            return;
        }
        databuffer[bytes_read] = '\0';
        msgLength = atoi(databuffer);
    }

    char buffer[msgLength];

    // ler a mensagem final
    bytes_read = read(fd2, buffer, msgLength);
    if (bytes_read <= 0) {
        perror("Error reading the message from FIFO");
        close(fd2);
        return ;
    }

    buffer[bytes_read] = '\0';
    printf("%s\n", buffer);
}

void ler_mensagem(int mid, ConcordiaRequest *request) {
    request->flag = MENSAGEM;
    snprintf(request->command, COMMAND_SIZE, "ler");
    request->all_mid = mid-1;
    char *user = obter_usuario_atual();
    snprintf(request->user, usersize,"%s", user);

    send_to_deamon(request);

    printf("Mensagem recebida:\n");

    read_from_daemon();
}

void responder_mensagem(int mid, char *msg, ConcordiaRequest *request) {
    request->flag = MENSAGEM;
    snprintf(request->command, COMMAND_SIZE, "responder");
    request->all_mid = mid-1;
    snprintf(request->msg, MSG_SIZE, "%s", msg);
    char *user = obter_usuario_atual();
    snprintf(request->user, usersize,"%s", user);

    send_to_deamon(request);
    printf("Enviando resposta...\n");

    read_from_daemon();
}

void remover_mensagem(int mid, ConcordiaRequest *request) {
    request->flag = MENSAGEM;
    snprintf(request->command, COMMAND_SIZE, "remover");
    request->all_mid = mid-1;
    char *user = obter_usuario_atual();
    snprintf(request->user, usersize,"%s", user);

    send_to_deamon(request);
    printf("Removendo mensagem de índice: %d\n", request->all_mid);

    read_from_daemon();
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <comando> [opções]\n", argv[0]);
        return EXIT_FAILURE;
    }

    ConcordiaRequest *request = malloc(sizeof(ConcordiaRequest));
    if (!request) {
        perror("Falha ao alocar memória para o request");
        return EXIT_FAILURE;
    }

    request->flag = MENSAGEM;
    request->pid = getpid();

    if (strcmp(argv[1], "enviar") == 0 && argc == 4) {
        enviar_mensagem(argv[2], argv[3], request);
    } 
    else if (strcmp(argv[1], "listar") == 0) {
        int all = (argc == 3 && strcmp(argv[2], "-a") == 0) ? 1 : 0;
        listar_mensagens(all, request);
    } 
    else if (strcmp(argv[1], "ler") == 0 && argc == 3) {
        ler_mensagem(atoi(argv[2]), request);
    } 
    else if (strcmp(argv[1], "responder") == 0 && argc == 4) {
        responder_mensagem(atoi(argv[2]), argv[3], request);
    } 
    else if (strcmp(argv[1], "remover") == 0 && argc == 3) {
        remover_mensagem(atoi(argv[2]), request);
    } 
    else {
        fprintf(stderr, "Comando inválido ou argumentos incorretos.\n");
        return EXIT_FAILURE;
    }

    free(request);

    return EXIT_SUCCESS;
}