#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include "heartbeat.h"


extern char ip_eth[LENGTH_IP];
extern char mac_eth[20];
extern char ip_wifi[LENGTH_IP];
extern char mac_wifi[20];

int SendToServer();
int SendData(char* msg, size_t len);
int ReceiveData();

#endif