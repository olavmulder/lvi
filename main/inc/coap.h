#ifndef COAP_H
#define COAP_H

#include "data.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "driver/timer.h"
#include "coap3/coap.h"

//is server
#define SIZE_OF_DATA_ARRAY 100
#define SERVER_NAME "server"

int CoAP_Client_Send(char* msg, size_t len);
int CoAP_Client_Init();
void CoAP_Client_Clear();


int Resolve_address(const char *host, const char *service, coap_address_t *dst);

#endif