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
 *      send status of that child to 'centraal bedieningspaneel'
 *
 */
#include "../inc/heartbeat.h"

const char* TAG_HEART = "Heartbeat";

uint8_t serverTimeout = 0;
strip_t *monitoring_head;
volatile bool heartbeat_confirm_timer_init = false;
esp_timer_handle_t heartbeat_confirm_timer;

/**
 * @brief on send failure to server, amount is incrementet
 * if higher dan max send_failure, go to next server ip .
 * @param type false = eth, true = wifi
 */
void _HandleServerFailure(bool type)
{
    static uint8_t amount;
    amount++;
    if(amount > MAX_SEND_FAILURE)
    {
        amount = 0;
        IncrementServerIpCount(type);
    }
    ESP_LOGW(TAG_HEART, "%s; set curGeneralState = Err", __func__);
    curGeneralState = Err;
}
//init timer, is called in hearbeatstart
void TimerHBConfirmInit()
{
    if(heartbeat_confirm_timer_init == false)
    {
        esp_timer_create_args_t hb_confirm_timer_args = {
            .callback = &TimerHBConfirmCallback,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "timer_hb_confirm",
        };
        ESP_ERROR_CHECK(esp_timer_create(&hb_confirm_timer_args, &heartbeat_confirm_timer));
        ESP_ERROR_CHECK(
        esp_timer_start_periodic(heartbeat_confirm_timer, HEARTBEAT_INTERVAL_MS_MAX_WAIT * 1000));
        heartbeat_confirm_timer_init = true;
    }
}
/**
 * @brief if hb_confirm timer goes off, no reaction from server than
 *  set curGeneralState on Err
 * and send a set Err message to all my childs in monitoring list
 */
void TimerHBConfirmCallback()
{
    //if not received confirm in time = state is black when on WiFi
    //make error message to all childs in monitoring list.

    if(is_mesh_connected && comState == COMMUNICATION_WIRELESS)
    {   
        mesh_data r = {
            .id = atoi(idName),
            .cmd = CMD_SET_ERR 
        };
        ESP_LOGW(TAG_HEART, "timer hb configrm set errr");
        memcpy(r.mac, mac_wifi, sizeof(r.mac));
        char msg[50];
        MakeMsgString(msg, &r, NULL, NULL, false, false, false);
        //if(!esp_mesh_is_root())
        //{
            //get all mesh address from childs, stored in monitoring_head
            mesh_addr_t mesh_leaf[monitoring_head->lenChildArr];
            size_t len = monitoring_head->lenChildArr;
            for(uint8_t i = 0; i < len;i++)
            {
                mesh_data r;
                memcpy(r.ip, monitoring_head->childArr[i]->ip_wifi, sizeof(r.ip));
                memcpy(r.mac, monitoring_head->childArr[i]->mac_wifi, sizeof(r.mac));
                r.port = monitoring_head->childArr[i]->port;
                _GetMeshAddr(&r, &mesh_leaf[i]);
            }
            //send to all childs
            mesh_data_t data_tx;
            data_tx.data = (uint8_t*)msg;
            data_tx.size = strlen(msg);
            data_tx.proto = MESH_PROTO_BIN;
            data_tx.tos = MESH_TOS_P2P;
            for(uint8_t i  = 0; i < len; i++)
            {
                int ret = esp_mesh_send(&mesh_leaf[i], &data_tx, MESH_DATA_P2P, NULL, 0); 
                if (ret != ESP_OK) {
                    ESP_LOGW(TAG_HEART, "%s Error esp_mesh_send %s", __func__, esp_err_to_name(ret));
                }
            }
            //memset(data_tx)
        /*}
        else
        {
            //send to coap server
            CoAP_Client_Send(msg, strlen(msg));
        }*/
    }
    //restart timer

    _HandleServerFailure( ((comState == COMMUNICATION_WIRED) ? false : true) );
    esp_timer_stop(heartbeat_confirm_timer);
    esp_timer_start_periodic(heartbeat_confirm_timer, HEARTBEAT_INTERVAL_MS_MAX_WAIT * 1000);
}


void CheckIsServer(Node* node)
{
    ESP_LOGW(TAG_HEART, "%s server-ip:%s", __func__, node->ip_wifi);
    if(strcmp(node->ip_wifi, SERVER_IP[currentServerIPCount_ETH]) == 0 )
    {
        ESP_LOGE(TAG_HEART, "%s = equal", __func__);

        //is heartbeat timeout of server
        _HandleServerFailure(false);
    }else if(strcmp(node->ip_wifi, SERVER_IP[currentServerIPCount_WIFI]) == 0)
    {
        ESP_LOGE(TAG_HEART, "%s = equal", __func__);
        //is heartbeat timeout of server
        _HandleServerFailure(true);
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
 * @brief funcion is called when timer goes off, set timoutId so timeOuttask
 * can handle it
 * 
 * @param id 
 */
void TimerCallback(void *id)
{
    Node*temp = id;
    ESP_LOGI(TAG_HEART, "timer call back for id %d",temp->id);
    FindTimedOutID(temp->id);
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
void SetAlive(strip_t *strip, uint8_t id, bool isAlive)
{
    for(uint8_t i = 0; i < strip->lenChildArr;i++)
    {
        if(strip->childArr[i]->id == id)
        {
            strip->childArr[i]->isAlive = isAlive;
        }
    }
}
/**
 * @brief called when CMD_HEARTBEAT is received,   if counter == 0 add id.
 * search for id,if found restart timer ,
 *  if not found add id to list 
 * @param id int
 */
void HeartbeatHandler(uint8_t id, strip_t* childsStrip)
{
    //ESP_LOGI(TAG_HEART, "%s id: %d len: %d", __func__, id, childsStrip->lenChildArr);
    //add all nodes from this child to own monitoring head
    //AddNodeToStrip checkt if id is already present
    for(uint8_t i = 0; i < childsStrip->lenChildArr; i++)
    {
        monitoring_head = AddNodeToStrip(monitoring_head, childsStrip->childArr[i]);
    }    
    //if not empty search node in strip
    for(uint8_t i = 0;i < monitoring_head->lenChildArr; i++)
    {
        //if found restart timer and return
        if(monitoring_head->childArr[i]->id == id)
        {
            //ESP_LOGI(TAG_HEART, "hb handler: restart timer of id %d", monitoring_head->childArr[i]->id);
            if(esp_timer_is_active(monitoring_head->childArr[i]->periodic_timer))
            {
                ESP_ERROR_CHECK(esp_timer_stop(monitoring_head->childArr[i]->periodic_timer));
                ESP_ERROR_CHECK(esp_timer_start_periodic(monitoring_head->childArr[i]->periodic_timer, 
                    HEARTBEAT_INTERVAL_MS_MAX_WAIT * 1000));
            }
            monitoring_head->childArr[i]->isAlive = true;
            //ESP_LOGI(TAG_HEART, "%s timer started", __func__);
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
    if(MakeMsgStringHeartbeat(tx_buf, monitoring_head) != 0)
    {
        ESP_LOGW(TAG_HEART, "%s make msg string error", __func__);
        return -1;
    }
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
    if(comState == COMMUNICATION_WIRELESS)
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
strip_t* StartHeartbeat(strip_t *strip)
{
    //make task that runs and check if a id is timed out from the heatbeat timer
    ESP_LOGI(TAG_HEART, "begin %s", __func__);
    strip = malloc(sizeof(strip_t));
    strip->childArr =(Node**)malloc(sizeof(Node*));
    strip->childArr[0] =(Node*)malloc(sizeof(Node));

    strip->lenChildArr = 0;
    TimerHBConfirmInit();
    return strip;
}
/**
 * @brief increment server ip count & send current counter to all devices
 * in network, to let them know
 * 
 * @param type true = wifi, false = eth
 */
int IncrementServerIpCount(bool type)
{
    if(type)//wifi =1
    {
        currentServerIPCount_WIFI++;
        if(currentServerIPCount_WIFI >AMOUNT_SERVER_ADDRS-1)
        {
            currentServerIPCount_WIFI = 0;
        }
    }
    else //eth =0
    {
        currentServerIPCount_ETH++;
        if(currentServerIPCount_ETH >AMOUNT_SERVER_ADDRS-1)
        {
            currentServerIPCount_ETH = 0;
        }
        ESP_LOGW(TAG_HEART, "%s; currentServerIPCount = %d", __func__, currentServerIPCount_ETH);
    }
    return 0;
}

/*
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

*/