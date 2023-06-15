#include "../inc/wifi.h"

#define RX_SIZE          (750)
#define TX_SIZE          (750)

const char *TAG_WIFI = "wifi_mesh";

static const uint8_t MESH_ID[6] = { 0x77, 0x77, 0x77, 0x77, 0x77, 0x77};

int8_t rssi = 0;

mesh_addr_t mesh_parent_addr;

static int mesh_layer = -1;
static esp_netif_t *netif_sta = NULL;
esp_ip4_addr_t s_current_ip;

/**
 * @brief send (heartbeat msg) to parent in mesh netwerk
 * if root, then send to coap server,
 * if not connected to mesh return -2
 * 
 * @param tx_buf mesg in uint8_t*
 * @param len length of msg in bytes
 * @return int -2 no mesh connection, -3 no parent found, 
 */
int SendWiFiMeshHeartbeat(uint8_t *tx_buf, size_t  len)
{
    int res = -1;
    if(esp_mesh_is_root())
    {
        
        res = SendWiFiMeshRoot(tx_buf, len);
        if(res < 0)
        {
            ESP_LOGW(TAG_WIFI, "%s; send wifi mesh root error %d", __func__, res);
        }
    }
    else
    {
        //if(!is_mesh_connected)return -2;
        //set mesh_data_t parameters to send to parent
        mesh_data_t data_tx;
        data_tx.data = tx_buf;
        data_tx.size = len;
        data_tx.proto = MESH_PROTO_BIN;
        data_tx.tos = MESH_TOS_P2P;
        /*mesh_addr_t broadcast_group_id = {
            .addr = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}    // Working
        };*/
        res = esp_mesh_send(&mesh_parent_addr, &data_tx, MESH_DATA_P2P, NULL, 0); //1
        if (res != ESP_OK) {
            ESP_LOGW(TAG_WIFI, "%s Error esp_mesh_send %s", __func__, esp_err_to_name(res));
            return -1;
        }
    }
    return res;
}

/**
 * @brief send normal msg to root in mesh netwerk
 * 
 * @param tx_buf uint8_t buffer
 * @param len length of buffer
 * @return int -1 on error, 0 on success
 */
int SendWiFiMeshLeaf(uint8_t *tx_buf, size_t  len)
{
    mesh_data_t data_tx;
    data_tx.data = tx_buf;
    data_tx.size = len;
    data_tx.proto = MESH_PROTO_BIN;
    data_tx.tos = MESH_TOS_P2P;
   
    esp_err_t ret;
    ret = esp_mesh_send(NULL, &data_tx, 0, NULL, 0); 
    if (ret != ESP_OK) {
        ESP_LOGW(TAG_WIFI, "%s Error esp_mesh_send %s", __func__, esp_err_to_name(ret));
        return -1;
    }
    return 0;
}
/**
 * @brief send msg over coap, call again on return -2
 * 
 * @param tx_buf msg uint8_t*
 * @param len size of msg
 * @return int -2 semaphore taken, -1 error on sending
 */
int SendWiFiMeshRoot(uint8_t *tx_buf, size_t  len)
{
//send data to coap server
    if(!coapInitDone)  
    { 
      ESP_LOGW(TAG_WIFI, "%s coap not done", __func__);

        if(CoAP_Client_Init() != 0)return -1;
    }

    int ret = CoAP_Client_Send((char*)tx_buf, len);
    if (ret < 0)
    {
        //failure so try later next server
        //IncrementServerIpCount(true);
        //ESP_LOGW(TAG_WIFI, "cient send went %d", ret);
       // CoAP_Client_Init();
        //return -1;
    }
    return 0;
}
/**
 * @brief send data over wifi mesh
 * select sending funciton depending on if it is a root or not
 * 
 * @param tx_buf data msg
 * @param len len of msg
 * @return int -2 when mesh is not connected
 *          -1 on sending error, 0 on success
 */
int SendWiFiMesh(uint8_t* tx_buf, size_t len, bool broadcast)
{
    int res = -1;
    if(!is_mesh_connected)
    {
        ESP_LOGE(TAG_WIFI, "%s mesh not connected", __func__);
        res = -2;
    }
    else if(broadcast)
    {
        mesh_data_t data_tx;
        data_tx.data = tx_buf;
        data_tx.size = len;
        data_tx.proto = MESH_PROTO_BIN;
        data_tx.tos = MESH_TOS_P2P;
        mesh_addr_t broadcast_addr = { 
            .addr = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }
        };
        int res = esp_mesh_send(&broadcast_addr, &data_tx, MESH_DATA_P2P, NULL, 0);
        if (res != ESP_OK) {
            ESP_LOGW(TAG_WIFI, "%s Error esp_mesh_send %s", __func__, esp_err_to_name(res));
            return -1;
        }
        else 
            return 0;
    }
    else if(esp_mesh_is_root() == true)
    {
        res = SendWiFiMeshRoot(tx_buf, len);
        if(res < 0)
            ESP_LOGW(TAG_WIFI, "%s Send wifi mesh root: %d", __func__, res);
        if(res == 2)
        {
            do{
                res = SendWiFiMeshRoot(tx_buf, len);
            }while(res == 2);
        }
    }
    else
    {
        res = SendWiFiMeshLeaf(tx_buf, len);
        if(res < 0)
            ESP_LOGW(TAG_WIFI, "%s Send wifi mesh leaf: %d", __func__, res);
    }
    return res;
}

/**
 * @brief Receive data on wifi as leaf,
 *  handle incoming data 
 * 
 * @param r pointer mesh_data sturct
 * @param data char* to received string
 * @return int -1 on failure, 0 on success
 */
int ReceiveWiFiMeshLeaf(mesh_data *r, uint8_t* data)
{
    //receive own data
    int ret = -1;
    if(strcmp(r->mac, mac_wifi) == 0)
    {
        ret = ReceiveClient(r->cmd, (char*)data);
    }
    return ret;
}
/**
 * @brief 
 * 
 * @param from mesh_addr where the message came from
 * @param r    mesh_data pointer to get get data and handle it
 * @param data msg string to handle the incoming data
 * @return int -3 on invalid cmd, -2 taken semaphore, -1 failure, 0 success
 */
int ReceiveWiFiMeshRoot(mesh_addr_t *from, mesh_data *r, uint8_t* data)
{
    if(r->cmd == CMD_TO_SERVER || r->cmd == CMD_INIT_SEND || r->cmd == CMD_SET_ERR)
    {
        char ip[LENGTH_IP];
        snprintf(ip, LENGTH_IP,IPSTR, IP2STR(&from->mip.ip4));
        uint16_t port = from->mip.port;
        //only add temperature, because server only needs temp
        if(HandleIncomingData((char*)data, &r->temp) != 0)
        {
            ESP_LOGW(TAG_WIFI,"handle incoming data error");
            return -1;
        }
        char buf[500];
        if(MakeMsgString(buf, r, ip,&port,false,true,false ) != 0)
        {
            ESP_LOGW(TAG_WIFI,"MakeMsgString error");
            return -1;
        }
        return SendWiFiMeshRoot((uint8_t*)buf, strlen(buf));   
    }
    return -3;

}
/**
 * @brief checks if there is a msg received over the mesh network
 * if so read it and check what kind of msg it is(CMD__*)
 * if there is data, call read leaf/ read root depending on if
 * device is root or not
 * @return int <0 ERROR,  0 success
 */
int ReceiveWiFiMesh()
{
    uint8_t rx_buf[RX_SIZE] = { 0, };
    mesh_addr_t from;
    mesh_data_t data_rx;
    int flag = 0;
    data_rx.data = rx_buf;
    data_rx.size = sizeof(rx_buf);
    
    int res = -1;
    bool bufIsFull=false;
    mesh_data r;
    //not portMAX_DELAY, because eth has also to be checked in this task
    esp_mesh_recv(&from, &data_rx, 0, &flag, NULL, 0); 
    //set buffer is full if received data
    if(strlen((char*)rx_buf) != 0)bufIsFull = true;
    while(bufIsFull)
    {
        r = HandleIncomingCMD(&from, (char*)rx_buf);
        //if it was CMD_HEARTBEAT it is already handled in last function,
        //so return 0
        if(r.cmd == CMD_HEARTBEAT)return 0;
        if(esp_mesh_is_root())
        {
            res = ReceiveWiFiMeshRoot(&from, &r, rx_buf);
            if(res < 0)
            {
                ESP_LOGW(TAG_WIFI, "%s: res: %d", __func__, res);
            }
        }
        else
        {
            res = ReceiveWiFiMeshLeaf(&r, rx_buf);
            if(res < 0)
            {
                ESP_LOGW(TAG_WIFI, "%s: res: %d", __func__, res);
            }
        }
        //handle buffer
        size_t len = strlen((char*)rx_buf);
        memcpy(rx_buf, rx_buf+len, RX_SIZE-len);
        if(strlen((char*)rx_buf) > 0)bufIsFull = true;
        else bufIsFull = false;
    }
    return res;
}

/**
 * @brief standaard mesh event handler
 * 
 * @param arg 
 * @param event_base 
 * @param event_id 
 * @param event_data 
 */
void mesh_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    mesh_addr_t id = {0,};
    static uint16_t last_layer = 0;

    switch (event_id) {
    case MESH_EVENT_STARTED: {
        esp_mesh_get_id(&id);
        ESP_LOGI(TAG_WIFI, "<MESH_EVENT_MESH_STARTED>ID:"MACSTR"", MAC2STR(id.addr));
        mesh_layer = esp_mesh_get_layer();

    }
    break;
    case MESH_EVENT_STOPPED: {
        ESP_LOGI(TAG_WIFI, "<MESH_EVENT_STOPPED>");
        mesh_layer = esp_mesh_get_layer();
    }
    break;
    case MESH_EVENT_CHILD_CONNECTED: {
        mesh_event_child_connected_t *child_connected = (mesh_event_child_connected_t *)event_data;
        ESP_LOGI(TAG_WIFI, "<MESH_EVENT_CHILD_CONNECTED>aid:%d, "MACSTR"",
                 child_connected->aid,
                 MAC2STR(child_connected->mac));
        amountMeshClients++;    
    }
    break;
    case MESH_EVENT_CHILD_DISCONNECTED: {
        mesh_event_child_disconnected_t *child_disconnected = (mesh_event_child_disconnected_t *)event_data;
        ESP_LOGI(TAG_WIFI, "<MESH_EVENT_CHILD_DISCONNECTED>aid:%d, "MACSTR"",
                 child_disconnected->aid,
                 MAC2STR(child_disconnected->mac));
        
        amountMeshClients--;
        if(amountMeshClients < 0)amountMeshClients=0;
    }
    break;
    case MESH_EVENT_ROUTING_TABLE_ADD: {
        mesh_event_routing_table_change_t *routing_table = (mesh_event_routing_table_change_t *)event_data;
        ESP_LOGW(TAG_WIFI, "<MESH_EVENT_ROUTING_TABLE_ADD>add %d, new:%d, layer:%d",
                 routing_table->rt_size_change,
                 routing_table->rt_size_new, mesh_layer);
    }
    break;
    case MESH_EVENT_ROUTING_TABLE_REMOVE: {
        mesh_event_routing_table_change_t *routing_table = (mesh_event_routing_table_change_t *)event_data;
        ESP_LOGW(TAG_WIFI, "<MESH_EVENT_ROUTING_TABLE_REMOVE>remove %d, new:%d, layer:%d",
                 routing_table->rt_size_change,
                 routing_table->rt_size_new, mesh_layer);
    }
    break;
    case MESH_EVENT_NO_PARENT_FOUND: {
        mesh_event_no_parent_found_t *no_parent = (mesh_event_no_parent_found_t *)event_data;
        ESP_LOGI(TAG_WIFI, "<MESH_EVENT_NO_PARENT_FOUND>scan times:%d",
                 no_parent->scan_times);
    }
    break;
    case MESH_EVENT_PARENT_CONNECTED: {
        mesh_event_connected_t *connected = (mesh_event_connected_t *)event_data;
        esp_mesh_get_id(&id);
        mesh_layer = connected->self_layer;
        memcpy(&mesh_parent_addr.addr, connected->connected.bssid, 6);
        ESP_LOGI(TAG_WIFI,
                 "<MESH_EVENT_PARENT_CONNECTED>layer:%d-->%d, parent:"MACSTR"%s, ID:"MACSTR", duty:%d",
                 last_layer, mesh_layer, MAC2STR(mesh_parent_addr.addr),
                 esp_mesh_is_root() ? "<ROOT>" :
                 (mesh_layer == 2) ? "<layer2>" : "", MAC2STR(id.addr), connected->duty);
        last_layer = mesh_layer;
        is_mesh_connected = true;
        if (esp_mesh_is_root()) {
            esp_netif_dhcpc_stop(netif_sta);
            esp_netif_dhcpc_start(netif_sta);
        }
    }
    break;
    case MESH_EVENT_PARENT_DISCONNECTED: {
        mesh_event_disconnected_t *disconnected = (mesh_event_disconnected_t *)event_data;
        ESP_LOGI(TAG_WIFI,
                 "<MESH_EVENT_PARENT_DISCONNECTED>reason:%d",
                 disconnected->reason);
        
        is_mesh_connected = false;
        //if(comState == COMMUNICATION_WIRELESS)
        //    curGeneralState = Err;
        mesh_layer = esp_mesh_get_layer();
    }
    break;
    case MESH_EVENT_LAYER_CHANGE: {
        mesh_event_layer_change_t *layer_change = (mesh_event_layer_change_t *)event_data;
        mesh_layer = layer_change->new_layer;
        ESP_LOGI(TAG_WIFI, "<MESH_EVENT_LAYER_CHANGE>layer:%d-->%d%s",
                 last_layer, mesh_layer,
                 esp_mesh_is_root() ? "<ROOT>" :
                 (mesh_layer == 2) ? "<layer2>" : "");
        last_layer = mesh_layer;
    }
    break;
    case MESH_EVENT_ROOT_ADDRESS: {
        mesh_event_root_address_t *root_addr = (mesh_event_root_address_t *)event_data;
        ESP_LOGI(TAG_WIFI, "<MESH_EVENT_ROOT_ADDRESS>root address:"MACSTR"",
                 MAC2STR(root_addr->addr));
    }
    break;
    case MESH_EVENT_VOTE_STARTED: {
        mesh_event_vote_started_t *vote_started = (mesh_event_vote_started_t *)event_data;
        ESP_LOGI(TAG_WIFI,
                 "<MESH_EVENT_VOTE_STARTED>attempts:%d, reason:%d, rc_addr:"MACSTR"",
                 vote_started->attempts,
                 vote_started->reason,
                 MAC2STR(vote_started->rc_addr.addr));
    }
    break;
    case MESH_EVENT_VOTE_STOPPED: {
        ESP_LOGI(TAG_WIFI, "<MESH_EVENT_VOTE_STOPPED>");
        break;
    }
    case MESH_EVENT_ROOT_SWITCH_REQ: {
        mesh_event_root_switch_req_t *switch_req = (mesh_event_root_switch_req_t *)event_data;
        ESP_LOGI(TAG_WIFI,
                 "<MESH_EVENT_ROOT_SWITCH_REQ>reason:%d, rc_addr:"MACSTR"",
                 switch_req->reason,
                 MAC2STR( switch_req->rc_addr.addr));
    }
    break;
    case MESH_EVENT_ROOT_SWITCH_ACK: {
        /* new root */
        mesh_layer = esp_mesh_get_layer();
        esp_mesh_get_parent_bssid(&mesh_parent_addr);
        ESP_LOGI(TAG_WIFI, "<MESH_EVENT_ROOT_SWITCH_ACK>layer:%d, parent:"MACSTR"", mesh_layer, MAC2STR(mesh_parent_addr.addr));
    }
    break;
    case MESH_EVENT_TODS_STATE: {
        mesh_event_toDS_state_t *toDs_state = (mesh_event_toDS_state_t *)event_data;
        ESP_LOGI(TAG_WIFI, "<MESH_EVENT_TODS_REACHABLE>state:%d", *toDs_state);
    }
    break;
    case MESH_EVENT_ROOT_FIXED: {
        mesh_event_root_fixed_t *root_fixed = (mesh_event_root_fixed_t *)event_data;
        ESP_LOGI(TAG_WIFI, "<MESH_EVENT_ROOT_FIXED>%s",
                 root_fixed->is_fixed ? "fixed" : "not fixed");
    }
    break;
    case MESH_EVENT_ROOT_ASKED_YIELD: {
        mesh_event_root_conflict_t *root_conflict = (mesh_event_root_conflict_t *)event_data;
        ESP_LOGI(TAG_WIFI,
                 "<MESH_EVENT_ROOT_ASKED_YIELD>"MACSTR", rssi:%d, capacity:%d",
                 MAC2STR(root_conflict->addr),
                 root_conflict->rssi,
                 root_conflict->capacity);
    }
    break;
    case MESH_EVENT_CHANNEL_SWITCH: {
        mesh_event_channel_switch_t *channel_switch = (mesh_event_channel_switch_t *)event_data;
        ESP_LOGI(TAG_WIFI, "<MESH_EVENT_CHANNEL_SWITCH>new channel:%d", channel_switch->channel);
    }
    break;
    case MESH_EVENT_SCAN_DONE: {
        mesh_event_scan_done_t *scan_done = (mesh_event_scan_done_t *)event_data;
        ESP_LOGI(TAG_WIFI, "<MESH_EVENT_SCAN_DONE>number:%d",
                 scan_done->number);
    }
    break;
    case MESH_EVENT_NETWORK_STATE: {
        mesh_event_network_state_t *network_state = (mesh_event_network_state_t *)event_data;
        ESP_LOGI(TAG_WIFI, "<MESH_EVENT_NETWORK_STATE>is_rootless:%d",
                 network_state->is_rootless);
    }
    break;
    case MESH_EVENT_STOP_RECONNECTION: {
        ESP_LOGI(TAG_WIFI, "<MESH_EVENT_STOP_RECONNECTION>");
    }
    break;
    case MESH_EVENT_FIND_NETWORK: {
        mesh_event_find_network_t *find_network = (mesh_event_find_network_t *)event_data;
        ESP_LOGI(TAG_WIFI, "<MESH_EVENT_FIND_NETWORK>new channel:%d, router BSSID:"MACSTR"",
                 find_network->channel, MAC2STR(find_network->router_bssid));
    }
    break;
    case MESH_EVENT_ROUTER_SWITCH: {
        mesh_event_router_switch_t *router_switch = (mesh_event_router_switch_t *)event_data;
        ESP_LOGI(TAG_WIFI, "<MESH_EVENT_ROUTER_SWITCH>new router:%s, channel:%d, "MACSTR"",
                 router_switch->ssid, router_switch->channel, MAC2STR(router_switch->bssid));
    }
    break;
    case MESH_EVENT_PS_PARENT_DUTY: {
        mesh_event_ps_duty_t *ps_duty = (mesh_event_ps_duty_t *)event_data;
        ESP_LOGI(TAG_WIFI, "<MESH_EVENT_PS_PARENT_DUTY>duty:%d", ps_duty->duty);
    }
    break;
    case MESH_EVENT_PS_CHILD_DUTY: {
        mesh_event_ps_duty_t *ps_duty = (mesh_event_ps_duty_t *)event_data;
        ESP_LOGI(TAG_WIFI, "<MESH_EVENT_PS_CHILD_DUTY>cidx:%d, "MACSTR", duty:%d", ps_duty->child_connected.aid-1,
                MAC2STR(ps_duty->child_connected.mac), ps_duty->duty);
    }
    break;
    default:
        ESP_LOGI(TAG_WIFI, "unknown id:%d", event_id);
        break;
    }
}

void ip_event_handler(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    ESP_LOGI(TAG_WIFI, "<IP_EVENT_STA_GOT_IP>IP:" IPSTR, IP2STR(&event->ip_info.ip));
    snprintf(ip_wifi, LENGTH_IP, IPSTR, IP2STR(&event->ip_info.ip));
    s_current_ip.addr = event->ip_info.ip.addr;
    gotIPAddress = true;
    CoAP_Client_Init();
}
/**
 * @brief wifi init, call it only once
 * 
 */
void WiFiInit(void)
{
    
    ESP_ERROR_CHECK(nvs_flash_init());
    /*  tcpip initialization */
    ESP_ERROR_CHECK(esp_netif_init());
    /*  event initialization */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    /*  create network interfaces for mesh (only station instance saved for further manipulation, soft AP instance ignored */
    ESP_ERROR_CHECK(esp_netif_create_default_wifi_mesh_netifs(&netif_sta, NULL));
    /*  wifi initialization */
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&config));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_start());
    /*  mesh initialization */
    ESP_ERROR_CHECK(esp_mesh_init());
    
    ESP_ERROR_CHECK(esp_event_handler_register(MESH_EVENT, ESP_EVENT_ANY_ID, &mesh_event_handler, NULL));
    /*  set mesh topology */
    ESP_ERROR_CHECK(esp_mesh_set_topology(CONFIG_MESH_TOPOLOGY));
    /*  set mesh max layer according to the topology */
    ESP_ERROR_CHECK(esp_mesh_set_max_layer(CONFIG_MESH_MAX_LAYER));
    ESP_ERROR_CHECK(esp_mesh_set_vote_percentage(1));
    ESP_ERROR_CHECK(esp_mesh_set_xon_qsize(128));
#ifdef CONFIG_MESH_ENABLE_PS
    /* Enable mesh PS function */
    ESP_ERROR_CHECK(esp_mesh_enable_ps());
    /* better to increase the associate expired time, if a small duty cycle is set. */
    ESP_ERROR_CHECK(esp_mesh_set_ap_assoc_expire(60));
    /* better to increase the announce interval to avoid too much management traffic, if a small duty cycle is set. */
    ESP_ERROR_CHECK(esp_mesh_set_announce_interval(600, 3300));
#else
    /* Disable mesh PS function */
    ESP_ERROR_CHECK(esp_mesh_disable_ps());
    ESP_ERROR_CHECK(esp_mesh_set_ap_assoc_expire(360));
#endif
    mesh_cfg_t cfg = MESH_INIT_CONFIG_DEFAULT();
    /* mesh ID */
    memcpy((uint8_t *) &cfg.mesh_id, MESH_ID, 6);
    /* router */
    cfg.channel = CONFIG_MESH_CHANNEL;
    cfg.router.ssid_len = strlen(CONFIG_MESH_ROUTER_SSID);
    memcpy((uint8_t *) &cfg.router.ssid, CONFIG_MESH_ROUTER_SSID, cfg.router.ssid_len);
    memcpy((uint8_t *) &cfg.router.password, CONFIG_MESH_ROUTER_PASSWD,
           strlen(CONFIG_MESH_ROUTER_PASSWD));
    /* mesh softAP */
    ESP_ERROR_CHECK(esp_mesh_set_ap_authmode(CONFIG_MESH_AP_AUTHMODE));
    cfg.mesh_ap.max_connection = CONFIG_MESH_AP_CONNECTIONS;
    cfg.mesh_ap.nonmesh_max_connection = CONFIG_MESH_NON_MESH_AP_CONNECTIONS;
    memcpy((uint8_t *) &cfg.mesh_ap.password, CONFIG_MESH_AP_PASSWD,
           strlen(CONFIG_MESH_AP_PASSWD));
    ESP_ERROR_CHECK(esp_mesh_set_config(&cfg));
    /* mesh start */
    //esp_mesh_set_self_organized(true, false);
    ESP_ERROR_CHECK(esp_mesh_start());
#ifdef CONFIG_MESH_ENABLE_PS
    /* set the device active duty cycle. (default:10, MESH_PS_DEVICE_DUTY_REQUEST) */
    ESP_ERROR_CHECK(esp_mesh_set_active_duty_cycle(CONFIG_MESH_PS_DEV_DUTY, CONFIG_MESH_PS_DEV_DUTY_TYPE));
    /* set the network active duty cycle. (default:10, -1, MESH_PS_NETWORK_DUTY_APPLIED_ENTIRE) */
    ESP_ERROR_CHECK(esp_mesh_set_network_duty_cycle(CONFIG_MESH_PS_NWK_DUTY, CONFIG_MESH_PS_NWK_DUTY_DURATION, CONFIG_MESH_PS_NWK_DUTY_RULE));
#endif
    ESP_LOGI(TAG_WIFI, "mesh starts successfully, heap:%d, %s<%d>%s, ps:%d\n",  esp_get_minimum_free_heap_size(),
             esp_mesh_is_root_fixed() ? "root fixed" : "root not fixed",
             esp_mesh_get_topology(), esp_mesh_get_topology() ? "(chain)":"(tree)", esp_mesh_is_ps_enabled());
    //set wifi_mac addrr
    uint8_t mac_addr[6];
    esp_wifi_get_mac(ESP_IF_WIFI_STA, mac_addr);
    snprintf(mac_wifi, 20, MACSTR, MAC2STR(mac_addr));
}
