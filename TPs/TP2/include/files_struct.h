#ifndef FILES_STRUCT_H
#define FILES_STRUCT_H

#include "./struct.h"

struct FileInfo {
    char fileName[128];
    char name[64];  // nome para onde foi enviado
    char nameSender[64]; // nome de quem enviou
    int id, day, month, year, hour, minute, second, tam, read, nReplys, isReply;  // isReply é o 0 caso não seja e caso seja é o id da mensagem a que respondeu
};

void parseFileName(char *filename, struct FileInfo *info);
int count_files(const char *folderPath);
int compareFileInfo(const void *a, const void *b);
int getHighestID(const char *userFolderPath);
void sort_files(char *folderPath, struct FileInfo sortedFiles[]);
void generate_timestamp(char *timestamp);

#endif 