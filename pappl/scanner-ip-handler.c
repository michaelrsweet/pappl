#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

typedef struct IPNode
{
  char ip[16];
  time_t last_used;
  struct IPNode *next;
} IPNode;

// The current IP
IPNode *currentIP = NULL;

void addOrUpdateIP(IPNode **head, const char *ip)
{
  IPNode *current = *head;
  IPNode *prev = NULL;

  while (current != NULL)
  {
    if (strcmp(current->ip, ip) == 0)
    {
      current->last_used = time(NULL);
      currentIP = current; // Set the global currentIP to this IP
      return;
    }
    prev = current;
    current = current->next;
  }

  IPNode *new_node = (IPNode *)malloc(sizeof(IPNode));
  if (new_node == NULL)
  {
    perror("Unable to allocate memory for new IPNode");
    exit(EXIT_FAILURE);
  }

  strncpy(new_node->ip, ip, sizeof(new_node->ip));
  new_node->last_used = time(NULL);
  new_node->next = NULL;

  if (prev == NULL)
  {
    *head = new_node;
  }
  else
  {
    prev->next = new_node;
  }

  currentIP = new_node;
}
bool removeFirstIP(IPNode **head)
{
  if (*head == NULL)
  {
    return false;
  }

  IPNode *nodeToRemove = *head;
  *head = (*head)->next;

  if (currentIP == nodeToRemove)
  {
    currentIP = *head;
  }
  free(nodeToRemove);
  return true;
}
int isMatchingRequest(const char *request)
{
  char constructedRequest[256];
  sprintf(constructedRequest, "/eSCL/ScanJobs/%s/NextDocument", currentIP->ip);
  return strcmp(request, constructedRequest) == 0;
}
void printIPs(IPNode *head)
{
  IPNode *current = head;
  while (current != NULL)
  {
    printf("IP: %s, Last used: %s", current->ip, ctime(&(current->last_used)));
    current = current->next;
  }
}