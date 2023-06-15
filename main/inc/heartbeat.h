#ifndef HEARTBEAT_H
#define HEARTBEAT_H

#include "ethernet.h" //also data.h -> utils.h
#include "wifi.h"
#include "libs/strip.h"

#define MAX_SEND_FAILURE 3

extern int last_layer, mesh_layer;
extern uint8_t serverTimeout;

strip_t* StartHeartbeat(strip_t*);
void HeartbeatHandler(uint8_t id, strip_t* childsStrip);
int SendHeartbeat();

int SendServerCountToNodes(uint8_t serverCount, uint8_t type);

int IncrementServerIpCount(bool type);

void TimerHBConfirmCallback();
void TimerHBConfirmInit();

extern int SendData(char*, size_t);
extern int _GetMeshAddr(mesh_data *r, mesh_addr_t* mesh_leaf);
#endif