/* Mesh IP Internal Networking Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "../inc/coap.h"


/* CoAP server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

const char *TAG_COAP = "CoAP_server";
volatile bool coapInitDone = false;
static volatile bool have_response;
//if this compoles -> remove
//static volatile uint32_t msgID = 0; 

static coap_context_t  *ctx = NULL;
static coap_session_t *session = NULL;
static coap_address_t dst;

uint8_t currentServerIPCount = 0;

/**
 * @brief function to convert the mesh address from a string in the 
 * mesh_data struct to a mesh_addr_t variable, 
 * so it can be used in the esp_mesh_send fucntion
 * 
 * @param r 
 * @param mesh_leaf 
 */
int _GetMeshAddr(mesh_data *r, mesh_addr_t* mesh_leaf)
{
    //return when if mac || ip is empty
    if(strlen(r->mac) == 0 || strlen(r->ip) == 0)return -1;
    //'copy' r->mac to &mesh_leaf->addr
    sscanf(r->mac, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",&mesh_leaf->addr[0],
    &mesh_leaf->addr[1], &mesh_leaf->addr[2], &mesh_leaf->addr[3],
    &mesh_leaf->addr[4], &mesh_leaf->addr[5]);
    //set mip.port
    mesh_leaf->mip.port = r->port;
    //make ip addr and place in mesh_leaf from r->ip string
    uint8_t a,b,c,d;
    sscanf(r->ip, "%hhd.%hhd.%hhd.%hhd",&a,&b,&c,&d);
    esp_ip4_addr_t addr;
    esp_netif_set_ip4_addr(&addr, a,b,c,d);
    char buf[32];
    sprintf(buf, IPSTR, IP2STR(&addr));
    ip4addr_aton(buf, &mesh_leaf->mip.ip4);
    return 0;
}
/**
 * @brief coap message handler
 * if data is received over coap, handle here
 * 
 * @param s 
 * @param response 
 * @param received 
 * @param mid 
 */
static void message_handler(coap_session_t*s, 
                            const coap_pdu_t *response,
                            const coap_pdu_t *received,
                            const coap_mid_t mid)
{
        //ESP_LOGI(TAG_COAP, "%s", __func__);

        size_t len;
        const uint8_t* data;
        char* inData = NULL;
        
        if(coap_get_data(received, &len, &data) == 0)
        {
            ESP_LOGW(TAG_COAP, "%s, no data, can be heatbeat return msg", __func__);
            goto end;

        }
        //copy data to inData for futher use
        inData = (char*)malloc(len+1);
        snprintf(inData, len+1, "%s", data);
        
        ESP_LOGI(TAG_COAP, "%s, receive coap", __func__);
        //set data in mesh_data struct 'r';
        mesh_data r =  HandleIncomingCMD(NULL, inData);
        //check received cmd
        if(r.cmd != CMD_ERROR && r.cmd != CMD_TO_SERVER)
        {
            //check if it was our msg
            if(strcmp(mac_wifi, r.mac) == 0)
            {
                //ESP_LOGI(TAG_COAP, "%s, mac_wifi & inc mac equal", __func__);
                //
                if(ReceiveClient(r.cmd, inData)< 0)
                {
                    ESP_LOGW(TAG_COAP, "%s, client receive", __func__);
                }
            }
            else
            {
                //data for mesh device 
                //extract data and ip,port,mac & make json string from it to  send data
                mesh_addr_t mesh_leaf;
                //set address varibales in mesh_leaf, from mesh_data struct 'r'
                _GetMeshAddr(&r, &mesh_leaf);
                char msg[500];
                //get the received states etc to send over wifi mesh to 'mesh_leaf'
                if(ExtractDataFromMsg(inData, &r) == 0)
                {
                    //char* res =NULL;
                    if(r.cmd == CMD_INIT_INVALID || r.cmd == CMD_INIT_VALID)
                        MakeMsgString(msg, &r, r.ip, &r.port, false, false, false);
                    else
                        MakeMsgString(msg, &r, r.ip, &r.port, true, true, true);
                    
                
                    mesh_data_t data_temp;
                    data_temp.data = (uint8_t*)msg;
                    data_temp.size = sizeof(msg);
                    //ESP_LOGI(TAG_COAP, "%s: esp_mesh_send: %s", __func__, data_temp.data);
                    data_temp.proto = MESH_PROTO_BIN;
                    data_temp.tos = MESH_TOS_P2P;
                    esp_mesh_send(&mesh_leaf, &data_temp, 
                            MESH_DATA_P2P, NULL, 0);
                }
            }
        }
        end:
            have_response = 1;
            //ESP_LOGW(TAG_COAP, "have response = 1");
            if(inData != NULL)
                free(inData);
}

/**
 * @brief init coap client, only when ip is given
 * 
 * @return int -1 on error 0 on OK
 */
int CoAP_Client_Init()
{
    if(!gotIPAddress)
    {
        ESP_LOGW(TAG_COAP, "no ip");
        return -1;
    }
    //if(!esp_mesh_is_root())return -1;

    coap_startup();
    /* Set logging level */
    coap_set_log_level(LOG_WARNING);
    if(!coapInitDone){
        //ESP_LOGI(TAG_COAP, "GOT ip address");
        /* resolve destination address where server should be sent */
        /*if(currentServerIPCount > 2)
        {
            ESP_LOGE(TAG_COAP,"currentServerIPCount is to big: %d", currentServerIPCount);
            currentServerIPCount = 0;
            return -1;
        }*/        
        if (Resolve_address(SERVER_IP[currentServerIPCount_WIFI], "5683", &dst) < 0) {
            ESP_LOGE(TAG_COAP,  "failed to resolve address\n");
            CoAP_Client_Clear();
            return -1;
        }

        /* create CoAP context and a client session */
        if (!(ctx = coap_new_context(NULL))) {
                ESP_LOGE(TAG_COAP,  "cannot create libcoap context\n");
                CoAP_Client_Clear();
                return -1;
        }
        /* Support large responses */
        coap_context_set_block_mode(ctx,
                        COAP_BLOCK_USE_LIBCOAP | COAP_BLOCK_SINGLE_BODY);
        //return -1 when not obtained ip address.
        if (!(session = coap_new_client_session(ctx, NULL, &dst,
                                                        COAP_PROTO_TCP))) {
                ESP_LOGE(TAG_COAP, "cannot create client session\n");
                CoAP_Client_Clear();
                return -1;
        }
        coap_register_response_handler(ctx, message_handler);
        coapInitDone = true;
        is_mesh_connected = true;
        ESP_LOGI(TAG_COAP, "coap init done");
        return 0;
    }
    else
    {
        ESP_LOGW(TAG_COAP, "already init");
        return -1;
    }

}
/**
 * @brief clear client, mostly on sending error
 * after this client init have to be called again
 * coapInitDone is also set to false to indicate that
 */
void CoAP_Client_Clear()
{
    ESP_LOGW(TAG_COAP,"fisnish coap client");
    curGeneralState = Err;
    if(session){
        coap_session_release(session);
        session = NULL;
    }
    if(ctx)
        coap_free_context(ctx);
    coap_cleanup();
    coapInitDone = false;
    is_mesh_connected = false;
}
/**
 * @brief send message 
 * after 2 sec no response - > error clear init and return -1 
 * @return int return -1 on failer init again..
 */
int CoAP_Client_Send(char* msg, size_t len)
{
    static unsigned char token[8];
    size_t tokenlength;
    if(!coapInitDone)
    {
        ESP_LOGW(TAG_COAP, "%s client init NOT Done",__func__);   
        return -1;
    }

    coap_pdu_t* pdu = coap_pdu_init(COAP_MESSAGE_CON,
        COAP_REQUEST_CODE_GET,
        coap_new_message_id(session),
        coap_session_max_pdu_size(session));

    if (!pdu) {
            coap_log( LOG_EMERG, "cannot create PDU\n" );
            CoAP_Client_Clear();
            return -1;
    }
    if(!session){
        ESP_LOGE(TAG_COAP, "%s, no session", __func__);
        coap_delete_pdu(pdu);
        return -1;
    }
    coap_session_new_token(session, &tokenlength, token);
    coap_add_token(pdu, tokenlength, token);
    /* add a Uri-Path option */
    coap_add_option(pdu, COAP_OPTION_URI_PATH,strlen(SERVER_NAME) ,
                            (const uint8_t*)SERVER_NAME);
    
    coap_add_data(pdu, len, (const uint8_t*)msg);
    //coap_show_pdu(LOG_WARNING, pdu);
    /* and send the PDU */
    coap_send(session, pdu);
    int time = 0;
    while (have_response == 0){
            if(ctx != NULL)
                coap_io_process(ctx, COAP_IO_NO_WAIT);
            else
                return -5;
            //if there is no response in 5 second time, init again...
            //timer_get_counter_time_sec(TIMER_GROUP_0, TIMER_0, &time);
            vTaskDelay(50 / portTICK_RATE_MS);
            time++;
            if(time > (5000 /50)){
                ESP_LOGW(TAG_COAP, "timer went off");
                CoAP_Client_Clear();
                have_response = 0;
                //serverTimeout++;
                return -1;
            }
    }
    have_response = 0;
    return 0;
}

//function needed by coap init
int Resolve_address(const char *host, const char *service, coap_address_t *dst)
{

  struct addrinfo *res, *ainfo;
  struct addrinfo hints;
  int error, len=-1;

  memset(&hints, 0, sizeof(hints));
  memset(dst, 0, sizeof(*dst));
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_family = AF_UNSPEC;

  error = getaddrinfo(host, service, &hints, &res);

  if (error != 0) {
    return error;
  }

  for (ainfo = res; ainfo != NULL; ainfo = ainfo->ai_next) {
    switch (ainfo->ai_family) {
        case AF_INET6:
        case AF_INET:
                len = dst->size = ainfo->ai_addrlen;
                memcpy(&dst->addr.sin6, ainfo->ai_addr, dst->size);
                goto finish;
        default:
        ;
        }
  }
  finish:
    freeaddrinfo(res);
    return len;
}
