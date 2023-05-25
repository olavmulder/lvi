#ifndef DATA_H
#define DATA_H

#include "esp_wifi.h"
#include "esp_mesh.h"
#include "esp_mesh_internal.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "mesh_netif.h"

#include "libs/cJSON.h"
#include "libs/cJSON_Utils.h"
#include "utils.h"

extern char* hostnameServer;
extern const char* nameID;
extern const char* nameGenState;
extern const char* nameVoltageState;
extern const char* nameMAC;
extern const char* nameTemp;
extern const char* nameCMD;

extern volatile bool coapStarted;
extern volatile bool coapInitDone;

int HandleIncomingData(char* data, double* temp_data);
mesh_data HandleIncomingCMD(mesh_addr_t* from, char* data);


int ExtractDataFromMsg(char* msg, mesh_data* r);

int MakeMsgString(char* msg, mesh_data *r, char *ip, uint16_t *port,
                    bool gState, bool tState, bool vState);

int MakeMsgStringHeartbeat(uint8_t* msg, strip_t* data);
int MakeServerCountMsg(char* msg, size_t len, uint8_t serverCount, uint8_t serverType);
int ReceiveClient(CMD cmd, char* data);

extern void HeartbeatHandler(uint8_t id, strip_t*);
#endif