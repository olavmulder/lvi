#include "../inc/heartbeat.h"

/**
 * @brief todo: add ethernet connection check
 * V In het \textit{heartbeat} protocol behoud elke \gls{lvi} connectie met de \textit{host}, 
 * door de info van de \textit{hosts} te kennen. 
 * 
... Met behulp van een datastructuur \textit{strip}, 
    een lijst met de centrale bedieningspanelen als \textit{head} en nodes er achter aan. 
-- Elke node heeft een lokale- en een \textit{monitoring-strip}. 
Lokale \textit{strips} zorgen voor het bijhouden van de broker en de 
    nodes die er lokaal in de gaten worden gehouden.

De monitoring strips houd bij welke nodes er in de gaten worden gehouden 
    door de node die lokaal in de gaten worden gehouden.

lijst bijhouden die alle childs bijhoud met hun status als struct

Door een signaal(\textit{heartbeat}) te sturen naar de volgende \textit{node}, 
    kan er worden ontdekt of die \textit{node} nog werkt.

Echter kan de node zelf aangeven dat de verbinding niet meer werkt in het geval dat de
 conformatie berichten van het CoAP protocol niet meer worden ontvangen, 
of omdat het aangeeft dat de verbinding is verbroken. 
Zijn er \textit{nodes} die niet meer te bereiken zijn, dan moet dit worden aangegeven bij het centrale bedieningspaneel, om vervolgens over te stappen op een draadloos netwerk. Als uit het \textit{heartbeat} protocol blijkt dat alle \textit{nodes} de centrale bedieningspositie niet kunnen bereiken, kan er over worden gestapt naar een andere centrale bedieningspositie. Op deze manier kan er dus worden aangepast op fouten.
 */
/**
 * Note: eth heartbeat via servers
 *       wifi heartbeat via nodes.
*/
const char* TAG_HEART = "Heartbeat";
static int timeoutID = -1;
static SemaphoreHandle_t xSemaphoreTimeoutID;
uint8_t serverTimeout = 0;
strip_t *monitoring_head;

/**
 * hearthbeat fault detection protocol. 5 seconds timer
 *  - if root cannot send to COAP multiple times -> try other server -> also fail next (circulair)
 *      SendTCPData increment ethSendingFailureCount, if too high -> init ethernet again 
 *      with ++currentServerIpCount
 *  
 * - if connected with eth and cannot send but hardware is ther -> try other server
 *      -if hardware isn't there switch to wifi
 * - Every device sends CMD_HEARTBEAT to his parents(up in mesh netwerk)
 * - A parent keeps tracking of receiving messages of his childs, if not received in time
 *      send status of that child to centraal bedienings paneel 
 *
 */

//only for eth implementation!!
void CheckIsServer(Node* node)
{
    ESP_LOGW(TAG_HEART, "%s server-ip:%s", __func__, node->ip_wifi);
    if(strcmp(node->ip_wifi, SERVER_IP[currentServerIPCount_ETH]) == 0 )
    {
        ESP_LOGE(TAG_HEART, "%s = equal", __func__);

        //is heartbeat timeout of server
        HandleSendFailure(false);
        //IncrementServerIpCount(eth);
    }else if(strcmp(node->ip_wifi, SERVER_IP[currentServerIPCount_WIFI]) == 0)
    {
        ESP_LOGE(TAG_HEART, "%s = equal", __func__);
        //is heartbeat timeout of server
        HandleSendFailure(true);
    }
    else{
        ESP_LOGE(TAG_HEART, "%s= not equal, %s != (%s || %s) ", __func__, 
            node->ip_wifi, SERVER_IP[currentServerIPCount_ETH], SERVER_IP[currentServerIPCount_WIFI]);

    }
}
/**
 * @brief 
 * 
 * @param id which is timed out
 * @return int -3, not found, -2 on not inited
 */
//PSD done, but check ->id row 92 & ->isAlive row 95
int FindTimedOutID(int id)
{
    ESP_LOGW(TAG_HEART, "timeout != -1 send message to central panal for id %d", id);
    
    if(monitoring_head->lenChildArr < 1)//is ! inited 
        return -2;
    for(uint8_t i = 0;i < monitoring_head->lenChildArr ;i++)
    {
        if(monitoring_head->childArr[i]->id == id)//if found timed out id
        {
            //set isAlive on false and restart timer
            monitoring_head->childArr[i]->isAlive = false;
            CheckIsServer(monitoring_head->childArr[i]);
            //stop first, to start timer again
            esp_timer_stop(monitoring_head->childArr[i]->periodic_timer);
            esp_timer_start_periodic(monitoring_head->childArr[i]->periodic_timer, HEARTBEAT_INTERVAL_MS_MAX_WAIT * 1000);
            return 0;
        }
    }
    return -3;
}
/**
 * @brief task that contiuesly checks if a timeoutID has arrived
 * so that the timer interrupt is as small as possible
 */
void TimeoutTask()
{
    while(1)
    {
        int tempId = -1;
        if(timeoutID != -1)
        {
            if( xSemaphoreTake( xSemaphoreTimeoutID, ( TickType_t ) 10 ) == pdTRUE )
            {   
                tempId = timeoutID;
                timeoutID = -1;
                xSemaphoreGive(xSemaphoreTimeoutID);
            }
            FindTimedOutID(tempId);
        }
        vTaskDelay(100/portTICK_RATE_MS);
    }
    vTaskDelete(NULL);

}
/**
 * @brief funcion is called when timer goes off, set timoutId so timeOuttask
 * can handle it
 * 
 * @param id 
 */
void TimerCallback(void *id)
{
    Node*temp = id;
    ESP_LOGI(TAG_HEART, "timer call back for id %d",temp->id);
    if( xSemaphoreTake( xSemaphoreTimeoutID, ( TickType_t ) 10 ) == pdTRUE )
    {
        timeoutID = temp->id;
        xSemaphoreGive(xSemaphoreTimeoutID);
    }
}
/**
 * @brief init timer&start it
 * 
 * @param node 
 */
void InitTimer(Node* node)
{
    esp_timer_create_args_t periodic_timer_args = {
            .callback = &TimerCallback,
            .arg = node,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "timer_callback",
        };

    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &node->periodic_timer));
    ESP_ERROR_CHECK(
        esp_timer_start_periodic(node->periodic_timer, HEARTBEAT_INTERVAL_MS_MAX_WAIT * 1000));
}
/**
 * @brief Set the Alive variable in Node struct
 * 
 * @param id 
 * @param isAlive 
 */
void SetAlive(uint8_t id, bool isAlive)
{
    for(uint8_t i = 0; i < monitoring_head->lenChildArr;i++)
    {
        if(monitoring_head->childArr[i]->id == id)
        {
            monitoring_head->childArr[i]->isAlive = isAlive;
        }
    }
}
/**
 * @brief called when CMD_HEARTBEAT is received,   if counter == 0 add id.
 * search for id,if found restart timer ,
 *  if not found add id to list 
 * @param id int
 */
//PSD done, only add isaive row 210
void HeartbeatHandler(uint8_t id, strip_t* childsStrip)
{
    ESP_LOGI(TAG_HEART, "%s id: %d len: %d", __func__, id, childsStrip->lenChildArr);
    //add all nodes from this child to own monitoring head
    //AddNodeToStrip checkt if id is already present
    for(uint8_t i = 0; i < childsStrip->lenChildArr; i++)
    {
        monitoring_head = AddNodeToStrip(monitoring_head, childsStrip->childArr[i]);
    }
    //ESP_LOGI(TAG_HEART, "%s, added anything", __func__);
    
    //if not empty search node in strip
    for(uint8_t i = 0;i < monitoring_head->lenChildArr; i++)
    {
        //if found restart timer and return
        if(monitoring_head->childArr[i]->id == id)
        {
            ESP_LOGI(TAG_HEART, "hb handler: restart timer of id %d", monitoring_head->childArr[i]->id);
            if(esp_timer_is_active(monitoring_head->childArr[i]->periodic_timer))
            {
                ESP_ERROR_CHECK(esp_timer_stop(monitoring_head->childArr[i]->periodic_timer));
                ESP_ERROR_CHECK(esp_timer_start_periodic(monitoring_head->childArr[i]->periodic_timer, 
                    HEARTBEAT_INTERVAL_MS_MAX_WAIT * 1000));
            }
            //SetAlive(id, true);
            monitoring_head->childArr[i]->isAlive = true;

            ESP_LOGI(TAG_HEART, "%s timer started", __func__);

        }
    }
    //debug
    DisplayMonitoringString();
    return;
}
/**
 * @brief send hearbeat msg over wire
 * 
 * @return -2 make msg error, -1 send error, 0 success
 */
int SendHeartbeatWired()
{
    //ESP_LOGI(TAG_HEART, "send wired heartbeat");
    //set data
    mesh_data data;
    data.id = atoi(idName);
    data.cmd = CMD_HEARTBEAT;
    strcpy(data.mac, mac_eth);
    uint16_t port = TCP_SOCKET_SERVER_PORT;
    char msg[500];
    if(MakeMsgString(msg, &data, ip_eth, &port, false, false, false) < 0)
    {
        ESP_LOGW(TAG_HEART, "%s, cant make string", __func__);
        return -2;
    }
    else
    {
        if(SendData(msg, strlen(msg)) < 0)
        {
            ESP_LOGW(TAG_HEART, "%s, cant send data", __func__);
            return -1;
        }
        return 0;
    }
}

/**
 * @brief send heartbeat msg to mesh_parent, if root send to coap server
 * 
 */
int SendHeartbeatMesh()
{
    uint8_t tx_buf[2000];
    //make message
    //ESP_LOGI(TAG_HEART, "%s make msg string", __func__);
    //ip addr is not set..
    if(MakeMsgStringHeartbeat(tx_buf, monitoring_head) != 0)
    {
        ESP_LOGW(TAG_HEART, "%s make msg string error", __func__);
        return -1;
    }
    //ESP_LOGI(TAG_HEART, "%s; msg: %s", __func__, tx_buf);
    int res =  SendWiFiMeshHeartbeat(tx_buf, strlen((char*)tx_buf));
    return res;
}

/**
 * @brief select type of communication, 
 * and send over that communication type
 * @return int -1 failure, -2 no init,  0 success
 */
int SendHeartbeat()
{
    int res = -1;
    /*if(!initSuccessfull)
        res = -2
    else*/ if(comState == COMMUNICATION_WIRELESS)
    {
        res = SendHeartbeatMesh();
        ESP_LOGI(TAG_HEART, "%s: send wifi mesh heart: %d", __func__, res);

    }
    else if(comState == COMMUNICATION_WIRED)
    {
        res = SendHeartbeatWired();
        ESP_LOGI(TAG_HEART, "%s: send wired heart: %d", __func__, res);

    }else{

        ESP_LOGE(TAG_HEART, "no valid selected type of communication (wired or wireless)");
    }
    return res;
}

/**
 * @brief task that sends hearbeat message very HEATBEAT_INTERVAL_MS/1000 sec.
 * 
 */
//PSD done
void StartHeartbeat()
{
    //make task that runs and check if a id is timed out from the heatbeat timer
    ESP_LOGI(TAG_HEART, "begin %s", __func__);
    xSemaphoreTimeoutID = xSemaphoreCreateMutex();
    monitoring_head = malloc(sizeof(strip_t));
    monitoring_head->childArr =(Node**)malloc(sizeof(Node*));
    monitoring_head->childArr[0] =(Node*)malloc(sizeof(Node));
    //set data of monitoring_head 0 = own data
    /*monitoring_head->childArr[0]->id = atoi(idName);
    strcpy(monitoring_head->childArr[0]->mac_wifi, mac_wifi);
    strcpy(monitoring_head->childArr[0]->ip_wifi, "\0");
    monitoring_head->childArr[0]->port = 0;*/

    monitoring_head->lenChildArr = 0;
    if( xSemaphoreTimeoutID != NULL)
    {
        xTaskCreatePinnedToCore(TimeoutTask, "timeout_heartbeat_task", 2048, NULL, 4,
                NULL, 1);
    }
}


/**
 * @brief increment server ip count & send current counter to all devices
 * in network, to let them know
 * 
 * @param type true = wifi, false = eth
 */
//PSD done
int IncrementServerIpCount(bool type)
{
    if(type)//wifi =1
    {
        currentServerIPCount_WIFI++;
        if(currentServerIPCount_WIFI >AMOUNT_SERVER_ADDRS-1)
        {
            currentServerIPCount_WIFI = 0;
        }

        //return SendServerCountToNodes(currentServerIPCount_WIFI, 1);
    }
    else //eth =0
    {
        currentServerIPCount_ETH++;
        if(currentServerIPCount_ETH >AMOUNT_SERVER_ADDRS-1)
        {
            currentServerIPCount_ETH = 0;
        }
        ESP_LOGW(TAG_HEART, "%s; currentServerIPCount = %d", __func__, currentServerIPCount_ETH);
        //return SendServerCountToNodes(currentServerIPCount_ETH, 0);
    }
    return 0;
}
/**
 * @brief on send failure to server, amount is incrementet
 * if higher dan max send_failure, go to next server ip .
 * @param type false = eth, true = wifi
 */
//psd done
void HandleSendFailure(bool type)
{
    static uint8_t amount;
    amount++;
    if(amount > MAX_SEND_FAILURE)
    {
        amount = 0;
        IncrementServerIpCount(type);
    }
    curGeneralState = Err;
}        //send broadcast to evry node in network,
        //to increment currentServerIPCount_ETH

int SendServerCountToNodes(uint8_t serverCount, uint8_t type)
{
    int res;
    char msg[500];
    size_t len = sizeof(msg);
    if(MakeServerCountMsg(msg, len, serverCount, type) != 0)
    {
        return -1;
    }
    switch(comState)
    {
        case COMMUNICATION_WIRED:
            //broadcast = 0
            res = SendTCPData(msg, strlen(msg), 0);
            break;
        case COMMUNICATION_WIRELESS:
            res = SendWiFiMesh((uint8_t*)msg, strlen(msg), true);
            break;
        default:
            ESP_LOGW(TAG_HEART, "%s, invalid comState", __func__);
            return -1;
    }
    return res;
}

