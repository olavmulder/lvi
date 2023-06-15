#include "../inc/communication.h"
const char *TAG_COMM = "communication";
/**
 * @brief receive data depenings on current communication mode
 * 
 * @return int -1 on failure -2 on no data, -3 not my data, -4 no communication
 *    -5 unexpected value comState, 0 on success 
 */
int ReceiveData()
{
 
   if(is_eth_connected)
   {
      if(ReceiveEth() == 0){
         //ESP_LOGI(TAG_COMM, "received eth");
      }
   }
   if(is_mesh_connected)
   {
      if(ReceiveWiFiMesh() == 0){
         //ESP_LOGI(TAG_COMM, "received wifi");
      }else
      {
         //ESP_LOGW(TAG_COMM, "tried received wifi but failed");
      }
   }
   return 0;
}
/**
 * @brief calls sending function depending on which kind of 
 * communication is selected.
 * 
 * @param msg char* to send
 * @param len length of msg in bytes
 * @return int -1 on error, 0 on success
 */
int SendData(char* msg, size_t len)
{
   int res = -1;
    switch(comState)
    {
      case COMMUNICATION_NONE:
         ESP_LOGW(TAG_COMM, "COMMUNICATION_NONE");
         break;
      case COMMUNICATION_WIRED:
         if(xSemaphoreTake( xSemaphoreEth, ( TickType_t ) 1000 ) == pdTRUE)
         {
               res = SendTCPData(msg, len, currentServerIPCount_ETH+1);
            if(res < 0)
            {
               ESP_LOGW(TAG_COMM, "send eth failed");
               is_eth_connected = false;
            }else
            {
               is_eth_connected = true;
            }
            xSemaphoreGive(xSemaphoreEth);
         }
         break;
      case COMMUNICATION_WIRELESS:
         res =  SendWiFiMesh((uint8_t*)msg, len, false);
         //if res is negatif, there is no connection, so mesh_connect is false
         if(res < 0)
         {
            ESP_LOGW(TAG_COMM, "send mesh failed");
            is_mesh_connected = false;
         }else{
            is_mesh_connected = true;
         }
         break;
      default:
         ESP_LOGW(TAG_COMM, "COMMUNICATION INVALID");
         break;      
    }
    return res;
}
/**
 * @brief 
 * first it makes a msg string with the data to send(temperature, id etc)
 * after that it calss de send data function 
 * @return int -2 on no_comState -1 error, 0 
 */
int SendToServer()
{
   mesh_data r;
   char buf[500];
   if(!initSuccessfull)
      r.cmd = CMD_INIT_SEND;
   else
      r.cmd = CMD_TO_SERVER;
   
   r.id = atoi(idName);
   r.temp = curMeasuredTemp;
   
   char tmp[LENGTH_IP];
   if(comState == COMMUNICATION_WIRELESS)
   {
      strcpy(r.mac, mac_wifi);
      uint16_t port = 0;
      if(esp_mesh_is_root())
      {
         uint16_t port = 0;

         if(MakeMsgString(buf, &r, ip_wifi, &port, false, true, false) != 0)
         {
            ESP_LOGW(TAG_COMM, "Make msg string error");
            return -1;
         }
      }
      else
      {
         if(MakeMsgString(buf, &r, NULL, NULL, false, true, false) != 0)
         {
            ESP_LOGW(TAG_COMM, "Make msg string error");
            return -1;
         }
      }
   }
   else if(comState == COMMUNICATION_WIRED)
   {
      strcpy(tmp, ip_eth);
      uint16_t port = TCP_SOCKET_SERVER_PORT;
      strcpy(r.mac, mac_eth);
      if(MakeMsgString(buf, &r, tmp, &port, false, true, false) != 0)
      {
         ESP_LOGW(TAG_COMM, "Make msg string error");
         return -1;
      }
   }
   else
   {
      ESP_LOGW(TAG_COMM, "comstate == NONE");
      return -2;
   }
   return SendData(buf, strlen(buf));
}
