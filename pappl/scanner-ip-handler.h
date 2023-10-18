#ifndef SCANNER_IP_HANDLER_H
#define SCANNER_IP_HANDLER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Struct definition
typedef struct IPNode
{
    char ip[16];
    time_t last_used;
    struct IPNode *next;
} IPNode;

// External variable declaration
extern IPNode *currentIP;

// Function prototypes
void addOrUpdateIP(IPNode **head, const char *ip);
void printIPs(IPNode *head);
bool removeFirstIP(IPNode **head);
int isMatchingRequest(const char *request);

#endif /* SCANNER_IP_HANDLER_H */
