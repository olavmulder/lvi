#ifndef HEARTBEAT_H
#define HEARTBEAT_H

#include "ethernet.h" //also data.h -> utils.h
#include "wifi.h"
#include "libs/strip.h"

#define MAX_SEND_FAILURE 3
#define HEARTBEAT_INTERVAL_MS   5000  // Send a heartbeat every 5 seconds
#define HEARTBEAT_INTERVAL_MS_MAX_WAIT   2*HEARTBEAT_INTERVAL_MS  // Send a heartbeat every 5 seconds

extern uint8_t serverTimeout;
void StartHeartbeat();
void HeartbeatHandler(uint8_t id, strip_t* childsStrip);
int SendHeartbeat();

int SendServerCountToNodes(uint8_t serverCount, uint8_t type);

int IncrementServerIpCount(bool type);
void HandleSendFailure(bool type);
extern int SendData(char*, size_t);

#endif