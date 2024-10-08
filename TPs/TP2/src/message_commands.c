#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <sys/wait.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <dirent.h>
#include <time.h>
#include <string.h>
#include <dirent.h>

#include "../include/struct.h"
#include "../include/files_struct.h"
#include "../include/group_commands.h"
#include "../include/utils.h"

void enviar_message(ConcordiaRequest request, char* uFolderPath, char* gFolderPath){
    // syslog(LOG_NOTICE, "Entrei enviar: %s\n", request.dest);
    // char dest[16];
    // strncpy(dest, request.dest, 16);
    int flagG = 1;
    char **foldersWAccess;
    int numDirs;

    numDirs = getUserGroups(uFolderPath, gFolderPath, request.user, &foldersWAccess);
     
    char *folderPath = selectDestino(foldersWAccess, numDirs, request.dest);

    if(!folderPath ){
        flagG = 0;
        folderPath = malloc(strlen(uFolderPath) + strlen(request.dest) + 2); 
        if (folderPath != NULL) {
        snprintf(folderPath, strlen(uFolderPath) + strlen(request.dest) + 2, "%s/%s", uFolderPath, request.dest);
        } else {
            syslog(LOG_ERR, "Memory allocation failed for folderPath\n");
            exit(EXIT_FAILURE);
        }
    }

    struct stat st;
    if (stat(folderPath, &st) == -1) {
        syslog(LOG_ERR, "Folder doesnt exist: %s\n", folderPath);
        exit(EXIT_FAILURE);
    }

    char timestamp[20];
    generate_timestamp(timestamp);

    int tam = strlen(request.msg);
    int id = getHighestID(folderPath);
    char fileName[250];
    snprintf(fileName, sizeof(fileName), "%s/%d;%s;%s;%s;%d;0;0;0.txt", folderPath, id+1, request.dest, request.user,  timestamp, tam);

    int file = open(fileName, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (file < 0) {
        if (errno == ENOENT) {
            syslog(LOG_ERR, "File '%s' does not exist.\n", fileName);
        } else if (errno == EACCES) {
            syslog(LOG_ERR, "Permission denied to open file '%s'.\n", fileName);
        } else {
            syslog(LOG_ERR, "Error opening file '%s': %s\n", fileName, strerror(errno));
        }
        exit(EXIT_FAILURE);
    }

    // syslog(LOG_NOTICE, "Tamanho buffer do request: %d\n", tam);
    char msg[513];
    strncpy(msg, request.msg, 512);
    syslog(LOG_NOTICE, "Tamanho buffer: %d\n", tam);
    msg[tam] = '\0';
    write(file, msg, sizeof(char) * tam);

    close(file);
    
    if(flagG == 1){
        exec_setfacl(folderPath, request.dest);
    }

    syslog(LOG_NOTICE, "Message written to %s in file: %s\n", request.dest, fileName);

    char confirmation[128];
    snprintf(confirmation, sizeof(confirmation), "Enviada mensagem para %s com sucesso!", request.dest);

    returnListToClient(request.pid, confirmation);
}

void ler_message(ConcordiaRequest request, char* uFolderPath, char* gFolderPath){
    int i = request.all_mid;
    char** foldersWAccess;
    int numDirs;
    int groupFlag = 1;

    numDirs = getUserGroups(uFolderPath, gFolderPath, request.user, &foldersWAccess);

    int numFiles;
    numFiles = count_Allfiles(foldersWAccess, numDirs);

    struct FileInfo sortedFiles[numFiles];

    sort_Allfiles(foldersWAccess, numDirs, sortedFiles, request.user);
    free(foldersWAccess);

    if (numFiles <= 0) {
        syslog(LOG_ERR, "Error sorting files.\n");
        free(foldersWAccess); 
        exit(EXIT_FAILURE);
    }

    if (sortedFiles == NULL) {
        syslog(LOG_ERR, "Error sorting files.\n");
        exit(EXIT_FAILURE);
    }
    
    int tam = sortedFiles[i].tam;

    char folderPath[128];
    if (strcmp(sortedFiles[i].name, request.user) != 0) {
        groupFlag = 0; 
        strcpy(folderPath, gFolderPath);
    } else {
        strcpy(folderPath, uFolderPath);
    }

    strcat(folderPath, "/");
    strcat(folderPath, sortedFiles[i].name);

    char fileName[256];
    snprintf(fileName, sizeof(fileName), "%s/%s", folderPath, sortedFiles[i].fileName);
    syslog(LOG_NOTICE, "file: %s tam: %d\n", fileName, tam);

    int file = open(fileName, O_RDONLY);
    if (file < 0) {
        if (errno == ENOENT) {
            syslog(LOG_ERR, "File '%s' does not exist.\n", fileName);
        } else if (errno == EACCES) {
            syslog(LOG_ERR, "Permission denied to open file '%s'.\n", fileName);
        } else {
            syslog(LOG_ERR, "Error opening file '%s': %s\n", fileName, strerror(errno));
        }
        exit(EXIT_FAILURE);
    }

    char msg[tam];
    msg[tam] = '\0';

    read(file, msg, tam);

    close(file);

    syslog(LOG_NOTICE, "Message read: %s\n", msg);

    if (groupFlag == 0) {
        file = open(fileName, O_WRONLY | O_APPEND);
        if (file < 0) {
            syslog(LOG_ERR, "Error opening file '%s': %s\n", fileName, strerror(errno));
            exit(EXIT_FAILURE);
        }
        struct FileInfo fich[1];
        parseFileName(fileName, fich);

        syslog(LOG_NOTICE, "FILE NAME IS %s", fileName);


        int check = checkIfRead(fileName, request.user, fich[0].tam, fich[0].read);

        syslog(LOG_NOTICE, "CHECK RESULT = %d", check);

        if (check == 0){
            syslog(LOG_NOTICE, "CHECK ENTROU = %d", check);
            char userReading[128];
            snprintf(userReading, sizeof(userReading), ";%s", request.user);
            write(file, userReading, strlen(userReading));
            close(file);
            groupFlag = strlen(userReading) + sortedFiles[i].read;
        }
    }

    if(sortedFiles[i].read == 0){
        char updateName[512];                                                                                         
        snprintf(updateName, sizeof(updateName), "%s/%d;%s;%s;%02d-%02d-%04d|%02d:%02d:%02d;%d;%d;%d;%d.txt", folderPath, sortedFiles[i].id, sortedFiles[i].name, sortedFiles[i].nameSender, sortedFiles[i].day, sortedFiles[i].month, sortedFiles[i].year, sortedFiles[i].hour, sortedFiles[i].minute, sortedFiles[i].second, sortedFiles[i].tam, groupFlag, sortedFiles[i].nReplys, sortedFiles[i].isReply);
        rename(fileName, updateName);
    }

    returnListToClient(request.pid, msg);
}

void responder_message(ConcordiaRequest request, char* uFolderPath, char* gFolderPath){
    int i = request.all_mid;
    // syslog(LOG_NOTICE, "Responder ler: %s\n", request.user);
    char** foldersWAccess;
    int numDirs;

    numDirs = getUserGroups(uFolderPath, gFolderPath, request.user, &foldersWAccess);

    int numFiles;
    numFiles = count_Allfiles(foldersWAccess, numDirs);

    struct FileInfo sortedFiles[numFiles];

    sort_Allfiles(foldersWAccess, numDirs, sortedFiles, request.user);
    free(foldersWAccess);

    if (numFiles <= 0) {
        syslog(LOG_ERR, "Error sorting files.\n");
        free(foldersWAccess); 
        exit(EXIT_FAILURE);
    }

    if (sortedFiles == NULL) {
        syslog(LOG_ERR, "Error sorting files.\n");
        exit(EXIT_FAILURE);
    }

    char folderPath[128];
    if (strcmp(sortedFiles[i].name, request.user) != 0) {
        strcpy(folderPath, gFolderPath);
    } else {
        strcpy(folderPath, uFolderPath);
    }

    strcat(folderPath, "/");
    strcat(folderPath, sortedFiles[i].name);

    syslog(LOG_NOTICE,"responder a %s", folderPath);

    char timestamp[20];
    generate_timestamp(timestamp);

    int tam = strlen(request.msg);
    int id = getHighestID(folderPath);

    char fileName[250];
    snprintf(fileName, sizeof(fileName), "%s/%d;%s;%s;%s;%d;0;0;%d.txt", folderPath, id+1, sortedFiles[i].name, request.user,  timestamp, tam, sortedFiles[i].id);
    // syslog(LOG_NOTICE, "Entrei enviar: %s\n", fileName);

    int file = open(fileName, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (file < 0) {
        if (errno == ENOENT) {
            syslog(LOG_ERR, "File '%s' does not exist.\n", fileName);
        } else if (errno == EACCES) {
            syslog(LOG_ERR, "Permission denied to open file '%s'.\n", fileName);
        } else {
            syslog(LOG_ERR, "Error opening file '%s': %s\n", fileName, strerror(errno));
        }
        exit(EXIT_FAILURE);
    }

    // syslog(LOG_NOTICE, "Tamanho buffer do request: %d\n", tam);
    char msg[513];
    strncpy(msg, request.msg, 512);
    syslog(LOG_NOTICE, "Tamanho buffer: %d\n", tam);
    msg[tam] = '\0';
    write(file, msg, sizeof(char) * tam);

    close(file);
    
    syslog(LOG_NOTICE, "Reply written to %s in file: %s\n", sortedFiles[i].nameSender, fileName);

    char updateName[512];
    char repliedFile[512];
    snprintf(repliedFile, sizeof(repliedFile), "%s/%s", folderPath, sortedFiles[i].fileName);                                                                                 //para onde foi enviado, quem enviou
    snprintf(updateName, sizeof(updateName), "%s/%d;%s;%s;%02d-%02d-%04d|%02d:%02d:%02d;%d;%d;%d;%d.txt", folderPath, sortedFiles[i].id, sortedFiles[i].name, sortedFiles[i].nameSender, sortedFiles[i].day, sortedFiles[i].month, sortedFiles[i].year, sortedFiles[i].hour, sortedFiles[i].minute, sortedFiles[i].second, sortedFiles[i].tam, sortedFiles[i].read, sortedFiles[i].nReplys + 1, sortedFiles[i].isReply);
    rename(repliedFile, updateName);
    syslog(LOG_NOTICE, "Updated name from %s to file: %s\n", sortedFiles[i].fileName, updateName);


    char confirmation[128];
    snprintf(confirmation, sizeof(confirmation), "Enviada resposta para %s com sucesso!", sortedFiles[i].nameSender);

    returnListToClient(request.pid, confirmation);
}

void remover_message(ConcordiaRequest request, char* uFolderPath, char* gFolderPath){
    int i = request.all_mid;
    syslog(LOG_NOTICE, "Responder ler: %s\n", request.user);
    char** foldersWAccess;
    int numDirs;

    numDirs = getUserGroups(uFolderPath, gFolderPath, request.user, &foldersWAccess);

    int numFiles;
    numFiles = count_Allfiles(foldersWAccess, numDirs);

    struct FileInfo sortedFiles[numFiles];

    sort_Allfiles(foldersWAccess, numDirs, sortedFiles, request.user);
    free(foldersWAccess);

    if (numFiles <= 0) {
        syslog(LOG_ERR, "Error sorting files.\n");
        free(foldersWAccess); 
        exit(EXIT_FAILURE);
    }

    if (sortedFiles == NULL) {
        syslog(LOG_ERR, "Error sorting files.\n");
        exit(EXIT_FAILURE);
    }

    char folderPath[128];
    if (strcmp(sortedFiles[i].name, request.user) != 0) {
        strcpy(folderPath, gFolderPath);
    } else {
        strcpy(folderPath, uFolderPath);
    }

    strcat(folderPath, "/");
    strcat(folderPath, sortedFiles[i].name);

    char fileRemove[264];
    snprintf(fileRemove, sizeof(fileRemove), "%s/%s", folderPath, sortedFiles[i].fileName);
    if (remove(fileRemove) == 0) {
        syslog(LOG_NOTICE, "File '%s' has been successfully removed.\n", fileRemove);
    } else {
        syslog(LOG_PERROR, "Error removing file %s\n", fileRemove);
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "Mensagem removida com sucesso!");

    returnListToClient(request.pid, msg);
}


void returnListToClient2(int pid, char *message, int len){
    char fifoName[128];
    snprintf(fifoName, sizeof(fifoName), "/var/lib/concordia/fifos/fifo_%d", pid);
    int fd = open(fifoName, O_WRONLY);
    if (fd == -1){
        syslog(LOG_ERR, "Error opening return fifo: %s \n", strerror(errno));
        return;
    }

    char lenStr[32];
    snprintf(lenStr, sizeof(lenStr), "%d", len);

    strcat(lenStr, "\n");

    syslog(LOG_NOTICE, "lenstr  = %s", lenStr);
    if (write(fd, lenStr, strlen(lenStr)) == -1) {
        syslog(LOG_ERR, "Failed to write length to FIFO: %s", strerror(errno));
        close(fd);
        return;
    }
    sleep(0.3);

    // Write the actual message
    if (write(fd, message, strlen(message)) == -1) {
        syslog(LOG_ERR, "Failed to write message to FIFO: %s", strerror(errno));
    }

    close(fd);
}

void listar_message(ConcordiaRequest request, char* uFolderPath, char* gFolderPath){
    syslog(LOG_NOTICE, "Processing listing for user: %s\n", request.user);

    char** foldersWAccess;
    int numDirs = getUserGroups(uFolderPath, gFolderPath, request.user, &foldersWAccess);
    int numFiles = count_Allfiles(foldersWAccess, numDirs);
    struct FileInfo* sortedFiles = malloc(sizeof(struct FileInfo) * numFiles);
    sort_Allfiles(foldersWAccess, numDirs, sortedFiles, request.user);
    free(foldersWAccess);

    if (numFiles <= 0) {
        syslog(LOG_ERR, "No files to sort.\n");
        char *msg = "No messages";
        returnListToClient(request.pid, msg);
        free(sortedFiles);
        return;
    }

    char *full_msg = escreverLista(sortedFiles, numFiles, request.all_mid, request.user);
    if (full_msg) {
        syslog(LOG_NOTICE, "full message: %s\n", full_msg);

        int len = strlen(full_msg);
        returnListToClient2(request.pid, full_msg, len);
        free(full_msg);
    } else {
        syslog(LOG_ERR, "Failed to generate listing message.\n");
    }
   
    free(sortedFiles);
}