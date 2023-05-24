#ifndef WIFI_H
#define WIFI_H

#include "coap.h"

#include "esp_heap_caps.h"
#include "esp_heap_task_info.h"
#include "esp_heap_trace.h"

extern int8_t amountMeshClients;
extern mesh_addr_t mesh_parent_addr;


void WiFiInit();
void WiFiDestroy();

void mesh_event_handler(void *arg, esp_event_base_t event_base,
                    int32_t event_id, void *event_data);
void ip_event_handler(void *arg, esp_event_base_t event_base,
                    int32_t event_id, void *event_data);
void esp_mesh_root_status(void *arg);
void esp_mesh_leaf_status(void *arg);

esp_err_t esp_mesh_comm_start(void);

void mesh_scan_done_handler(int num);


int SendWiFiMesh(uint8_t* tx_buf, size_t len, bool broadcast);
int ReceiveWiFiMesh();


int SendWiFiMeshRoot(uint8_t *tx_buf, size_t  len);
int SendWiFiMeshLeaf(uint8_t *tx_buf, size_t  len);
int SendWiFiMeshHeartbeat(uint8_t *tx_buf, size_t len);

int ReceiveWiFiMeshRoot(mesh_addr_t *from, mesh_data *r, uint8_t* data);
int ReceiveWiFiMeshLeaf(mesh_data *r, uint8_t* data);

extern int IncrementServerIpCount(bool type);
#endif