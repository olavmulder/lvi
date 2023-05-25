#ifndef UTILS_H
#define UTILS_H
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include "config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_sleep.h"

#include "driver/gpio.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "lvgl.h"
#include "driver/i2c.h"

#define ID "2"

//#include "lvgl_helpers.h"
#define LV_TICK_PERIOD_MS 1
#define AMOUNT_ETH_CONNECTION_FAILURE 10
#define LENGTH_IP 20

#define AMOUNT_SERVER_ADDRS 2

typedef enum{
  COMMUNICATION_WIRELESS, 
  COMMUNICATION_WIRED, 
  COMMUNICATION_NONE
}CommunicationState;

typedef enum {
   Go, NoGo
}VoltageFreeState;

typedef enum{
  Err = -1, R = 0, G, Y
}ClosingState;

typedef enum _CMD{
   CMD_ERROR, CMD_INIT_VALID, CMD_INIT_INVALID, CMD_INIT_SEND,
   CMD_TO_CLIENT, CMD_TO_SERVER, CMD_HEARTBEAT, CMD_SET_ERR, 
   CMD_SET_SERVER_NUMBER
}CMD;

typedef struct{
    //mesh node info
    int id;
    char mac[LENGTH_IP];
    char ip[LENGTH_IP];
    uint16_t port;
    //msg type
    CMD cmd;
    //data
    int8_t voltageState;
    int8_t genState;
    double temp;
}mesh_data;

//hearbeat
typedef struct _host_t
{
   char* ipAddr;
   uint16_t port;
}host_t;

typedef struct _node_t
{
   uint8_t id;
   char ip_wifi[LENGTH_IP];
   char mac_wifi[LENGTH_IP];
   uint16_t port;
   bool isAlive;
   esp_timer_handle_t periodic_timer;
}Node;

typedef struct _strip_t
{
   Node **childArr;//was idArr
   uint8_t lenChildArr;
   /*uint8_t num_childs;
   _node_t **childs;*/
}strip_t;

//states
extern volatile ClosingState curGeneralState;
extern volatile VoltageFreeState curVoltageState;
extern double curMeasuredTemp;
extern double curReceivedTemp;
extern volatile CommunicationState comState;


//wifi/eth connection varables
extern volatile bool initSuccessfull;
extern volatile bool is_mesh_connected;
extern volatile bool is_eth_connected;

extern volatile bool gotIPAddress;
extern volatile bool heartbeatEnable;

//ip stirngs
extern esp_ip4_addr_t s_current_ip;
extern const char *SERVER_IP[AMOUNT_SERVER_ADDRS]; //server ip's,
extern char ip_eth[LENGTH_IP];
extern char mac_eth[LENGTH_IP];
extern char ip_wifi[LENGTH_IP];
extern char mac_wifi[LENGTH_IP];

//hearbeat variables
extern int8_t currentServerIPCount_ETH;
extern uint8_t currentServerIPCount_WIFI;

extern uint8_t serverTimeout;

extern char idName[5];

SemaphoreHandle_t xSemaphoreWiFiMesh;
SemaphoreHandle_t xSemaphoreCOAP;
SemaphoreHandle_t xSemaphoreI2C;
SemaphoreHandle_t xSemaphoreEth;
SemaphoreHandle_t xGuiSemaphore;

#endif