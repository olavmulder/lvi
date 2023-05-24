#ifndef ETHERNET_H
#define ETHERNET_H

#include "driver/uart.h"
#include "esp_intr_alloc.h"
#include "soc/uart_struct.h"

#include "data.h"
#ifdef CORE2
#define UART_PORT_NUM UART_NUM_2
//#define UART_RX_PIN 16//GPIO_NUM_13
//#define UART_TX_PIN 17//GPIO_NUM_14

#endif
#define TCP_SOCKET_SERVER "192.168.2.103" //"192.168.178.25"
#define TCP_SOCKET_SERVER_PORT 8080


//moslty used in init
int UARTInit();
int InitEthernetConnection();
int CheckDeviceConnect();
int CheckETHConnect();
int CreateTCPClient(const char* ip, int port, int num);
int GetMAC();
int SetTCPMode(bool mode);
int SetMultiConnection();

//msg handling
char* waitMsg();
int SendTCPData(char* buffer, size_t size, int num);
int ReceiveTCPData(char* data, size_t amountMaxRead, int num);
int GetReturnValue(unsigned long waitTime, const char* msgToFilterOn);

int ReceiveEth();

extern int IncrementServerIpCount(bool type);

#endif