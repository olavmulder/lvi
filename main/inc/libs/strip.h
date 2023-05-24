#ifndef STRIP_H
#define STRIP_H

#include "../utils.h"




extern strip_t *monitoring_head;

void UpdateDependancy(strip_t* root);
Node* NewNode(int id, char* ip, char *mac, uint16_t port, bool isAlive);//data
strip_t* AddNodeToStrip(strip_t* strip, Node* n);
void DisplayMonitoringString();
/*void FindAllChildren(Node *node);
int MaxDepth(Node *root);
void UpdateDependancy(Node** root, int totalNodes);*/

extern void InitTimer(Node* node);
extern void TimerCallback(void *id);
#endif
