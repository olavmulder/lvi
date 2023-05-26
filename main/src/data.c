#include "../inc/data.h"
/*
functions to handle the incoming data and making message string to send
in json format, with the help of cJson library.
https://github.com/DaveGamble/cJSON

*/
//strings for cJSON names
const char *TAG_DATA = "data";

char* hostnameServer = idName;
const char* nameID = "id";
const char* nameMAC = "mac";
const char* nameIP = "ip";
const char* namePort = "port";
const char* nameCMD = "cmd";
const char* nameGenState = "closeState";
const char* nameVoltageState = "voltageState";
const char* nameTemp = "temp";
const char* nameIsAlive = "isAlive";
const char* nameServerCount = "serverCount";
const char* nameServerType = "serverType";

/**
 * @brief checks json format string if it contains
 *  cmd && mac address. and return that back
 * so it can be handled in HandleIncomingData
 * @return ret struct, with id = -1 on failure && id = 0 on valid
 *      if id = 0 than mac && cmd can be checked
 */
int ExtractDataFromMsg(char* msg, mesh_data* r)
{
    
    cJSON *jsonGenState = NULL;
    cJSON *jsonVoltState = NULL;
    cJSON *jsonTemp = NULL;
    
    cJSON *objPtr = cJSON_Parse(msg);
    if(objPtr == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            //ESP_LOGI(TAG_DATA, "Error json parse: %s\n", error_ptr);
        }
        cJSON_Delete(objPtr);
        return -1;
    }   
    jsonGenState = cJSON_GetObjectItemCaseSensitive(objPtr, nameGenState);
    if( cJSON_IsNumber(jsonGenState))
    { 
        r->genState = jsonGenState->valueint;
    }

    jsonVoltState = cJSON_GetObjectItemCaseSensitive(objPtr, nameVoltageState);
    if( cJSON_IsNumber(jsonVoltState))
    { 
        r->voltageState = jsonVoltState->valueint;
    }

    jsonTemp = cJSON_GetObjectItemCaseSensitive(objPtr, nameTemp);
    if( cJSON_IsNumber(jsonTemp))
    {
        r->temp = jsonTemp->valuedouble;
    }
    //added
    cJSON_Delete(objPtr);
    return 0;

}

/**
 * @brief make string in cjson format to send as a hearbeat msg
 * add id, cmd, mac to json, and array with all his child, which contains
 * mac, ip, port, id & isAlive
 * @param msg msg if a buffer to set result string
 * @param dataStrip strip of device contains all the data of his childs
 * @return int 0 on succss, -1 on error
 */
int MakeMsgStringHeartbeat(uint8_t* msg, strip_t* dataStrip)
{
    cJSON *obj = cJSON_CreateObject();
    cJSON *data;
    cJSON *node;
    cJSON *id;
    cJSON *ip;
    cJSON *mac;
    cJSON *port;
    cJSON *isAlive;

    if (obj == NULL)
    {
        goto end;
    }
    if (cJSON_AddNumberToObject(obj, nameID, atoi(idName)) == NULL)
    {
        goto end;
    }
    if (cJSON_AddNumberToObject(obj, nameCMD, CMD_HEARTBEAT) == NULL)
    {
        goto end;
    }
    if (cJSON_AddStringToObject(obj, nameMAC, mac_wifi) == NULL)
    {
        goto end;
    }
    data = cJSON_CreateArray();
    if (data == NULL)
    {
        goto end;
    }
    cJSON_AddItemToObject(obj, "array", data);
    for(uint8_t i = 0; i < dataStrip->lenChildArr; i++)
    {
        node = cJSON_CreateObject();
        if(node == NULL)
        {
            goto end;
        }
        cJSON_AddItemToArray(data, node);
        id = cJSON_CreateNumber(dataStrip->childArr[i]->id);
        if (id == NULL)
        {
            goto end;
        }
        cJSON_AddItemToObject(node, nameID, id);

        port = cJSON_CreateNumber(dataStrip->childArr[i]->port);
        if (port == NULL)
        {
            goto end;
        }
        cJSON_AddItemToObject(node, namePort, port);
        
        ip = cJSON_CreateString(dataStrip->childArr[i]->ip_wifi);
        if (ip == NULL)
        {
            goto end;
        }
        cJSON_AddItemToObject(node, nameIP, ip);

        mac = cJSON_CreateString(dataStrip->childArr[i]->mac_wifi);
        if (mac == NULL)
        {
            goto end;
        }
        cJSON_AddItemToObject(node, nameMAC, mac);

        isAlive = cJSON_CreateNumber(dataStrip->childArr[i]->isAlive);
        if (mac == NULL)
        {
            goto end;
        }
        cJSON_AddItemToObject(node, nameIsAlive, isAlive);
    }
    char* string = cJSON_Print(obj);
    if(string == NULL)
    {
        fprintf(stderr, "Failed to print obj\n");
    }else
    {
        if(strlen(string) < 2000)
            snprintf((char*)msg, strlen(string)+1, "%s", string);
    }
    //ESP_LOGI(TAG_DATA, "made heartbead string %s", msg);
    cJSON_Delete(obj);
    return 0;
    end:
        if(obj != NULL)
            cJSON_Delete(obj);
        return -1;
}

int MakeServerCountMsg(char* msg, size_t len, uint8_t serverCount, uint8_t serverType)
{
    cJSON *obj = cJSON_CreateObject();
    if (obj == NULL)
    {
        goto end;
    }
    if (cJSON_AddNumberToObject(obj, nameCMD, CMD_SET_SERVER_NUMBER) == NULL)
    {
        goto end;
    }
    if (cJSON_AddNumberToObject(obj, nameServerCount, serverCount) == NULL)
    {
        goto end;
    }
    if (cJSON_AddNumberToObject(obj, nameServerType, serverType) == NULL)
    {
        goto end;
    }
    char*dum =  cJSON_Print(obj);
    if (dum == NULL)
    {
        ESP_LOGE(TAG_DATA,"Failed to print monitor.\n");
        return -1;
    } 
    snprintf(msg, len, "%s", dum);
    //ESP_LOGW(TAG_DATA, "msg: %s\n", msg);

    free(dum);
    cJSON_Delete(obj);
    return 0;
    end:
        if(obj != NULL)
            cJSON_Delete(obj);
        return -1;
}
/**
 * @brief make a json string which is copied to msg variable
 * msg string contains data in json format to send 
 * 
 * @param msg char* [out]
 * @param r mesh_data* -> contains id & port & data values
 * @param ip char* ip address to send
 * @param port uint16_t port to send
 * @param gState set datavalue general status
 * @param tState set temperautre value
 * @param vState set voltag_DATAe status
 * @return int -1 on error 0 on OK
 */
int MakeMsgString(char* msg, mesh_data *r, char *ip, uint16_t *port,
                    bool gState, bool tState, bool vState)
{

    cJSON *obj = cJSON_CreateObject();
    if (obj == NULL)
    {
        goto end;
    }
    if(ip != NULL)
    {
        if (cJSON_AddStringToObject(obj, nameIP, ip) == NULL)
        {
            goto end;
        }
    }
    if(port != NULL)
    {
        if (cJSON_AddNumberToObject(obj, namePort, *port) == NULL)
        {
            goto end;
        }
    }
    if (cJSON_AddStringToObject(obj, nameMAC, r->mac) == NULL)
    {
        goto end;
    }
    if (cJSON_AddNumberToObject(obj, nameCMD, r->cmd) == NULL)
    {
        goto end;
    }
    if (cJSON_AddNumberToObject(obj, nameID, r->id) == NULL)
    {
        goto end;
    }
    if(gState){
        if (cJSON_AddNumberToObject(obj, nameGenState, r->genState) == NULL)
        {
            goto end;
        }
    }
    if(vState){
        if (cJSON_AddNumberToObject(obj, nameVoltageState, r->voltageState) == NULL)
        {
            goto end;
        }
    }
    if(tState){
        if (cJSON_AddNumberToObject(obj, nameTemp, (int)r->temp) == NULL)
        {
            goto end;
        }
    }
    char*dum =  cJSON_Print(obj);
    if (dum == NULL)
    {
        ESP_LOGE(TAG_DATA,"Failed to print monitor.\n");
        return -1;
    }  
    snprintf(msg, 500, "%s", dum);
    //ESP_LOGW(TAG_DATA, "msg: %s\n", msg);

    free(dum);
    cJSON_Delete(obj);
    return 0;
    end:
        if(obj != NULL)
            cJSON_Delete(obj);
        return -1;

}

/**
 * @brief handles incoming command, mac, ip, port and id aka the default 
 * values. Returs mesh_data struct in the incoming data from the json string data
 * 
 * @param data incoming data json format from the type char*
 * @return mesh_data 
 */
mesh_data HandleIncomingCMD(mesh_addr_t *from, char* data)
{
    mesh_data r;
    //r.id = -1;
    cJSON *jsonMAC = NULL;
    cJSON *jsonCMD = NULL;
    cJSON *jsonPort = NULL;
    cJSON *jsonID = NULL;
    cJSON *jsonIP = NULL;
    
    r.cmd = CMD_ERROR;
    //check if data is json formated
    cJSON *objPtr = cJSON_Parse(data);
    if(objPtr != NULL)
    {
        jsonCMD = cJSON_GetObjectItemCaseSensitive(objPtr, nameCMD);
        if(cJSON_IsNull(jsonCMD)){
            cJSON_Delete(objPtr);
            ESP_LOGE(TAG_DATA, "jsoncmd is null");
            return r;
        }
        r.cmd = jsonCMD->valueint;
        if(r.cmd == CMD_SET_SERVER_NUMBER)
        {
            cJSON *jsonServerCount;
            cJSON *jsonServertype;
            jsonServerCount = cJSON_GetObjectItemCaseSensitive(objPtr, nameServerCount);
            jsonServertype = cJSON_GetObjectItemCaseSensitive(objPtr, nameServerType);
            if(cJSON_IsNumber(jsonServerCount) && cJSON_IsNumber(jsonServertype) )
            {
                if(jsonServertype->valueint == 0)
                {
                    currentServerIPCount_ETH = jsonServerCount->valueint;
                }
                else if(jsonServertype->valueint == 1)
                {
                    currentServerIPCount_WIFI = jsonServerCount->valueint;
                }
            }
            cJSON_Delete(objPtr);
            return r;

        }
        if(r.cmd == CMD_HEARTBEAT)
        {
            //
            jsonID = cJSON_GetObjectItemCaseSensitive(objPtr, nameID);
            jsonMAC = cJSON_GetObjectItemCaseSensitive(objPtr, nameMAC);

            if( jsonID == NULL && !cJSON_IsNumber(jsonID) 
            &&  jsonMAC == NULL && !cJSON_IsString(jsonMAC))
            {
                ESP_LOGE(TAG_DATA, "%s:id is null", __func__);
            }else{
                cJSON *node = NULL;
                cJSON *data = NULL;
                
                /*if(from == NULL)
                {
                    ESP_LOGW(TAG_DATA, "%s: from is NULL", __func__);
                    cJSON_Delete(objPtr);
                    return r;
                }*/
                //ESP_LOGW(TAG_DATA, "%s: call heartbeat handler: id:%d, port:%d", __func__, jsonID->valueint, from->mip.port);
                strip_t *child = (strip_t*)malloc(sizeof(strip_t));
                child->childArr = (Node**)malloc(sizeof(Node*));
                child->childArr[0] = (Node*)malloc(sizeof(Node));
                child->lenChildArr = 1;
                
                //data(id, mac, ip, port) of sender
                child->childArr[0]->id = jsonID->valueint;
                if(from != NULL){
                    snprintf(child->childArr[0]->ip_wifi, LENGTH_IP, IPSTR, IP2STR(&from->mip.ip4));     
                    child->childArr[0]->port = from->mip.port;
                }
                else
                {
                    jsonIP = cJSON_GetObjectItemCaseSensitive(objPtr, nameIP);
                    jsonPort = cJSON_GetObjectItemCaseSensitive(objPtr, namePort);
                    if(cJSON_IsString(jsonIP))
                        snprintf(child->childArr[0]->ip_wifi, LENGTH_IP, "%s", jsonIP->valuestring);     
                    if(cJSON_IsNumber(jsonPort))
                        child->childArr[0]->port = jsonPort->valueint;
                }               
                snprintf(child->childArr[0]->mac_wifi, 20, "%s", jsonMAC->valuestring);
                child->childArr[0]->isAlive = true;

                //ESP_LOGI(TAG_DATA, "%s, get send data", __func__);
               
                data = cJSON_GetObjectItemCaseSensitive(objPtr, "array");
                if(data != NULL)
                {
                    cJSON_ArrayForEach(node, data)
                    {
                        //all the data of the sender's childs
                        Node *temp = malloc(sizeof(Node));
                        cJSON *mac_in_Arr = cJSON_GetObjectItemCaseSensitive(node, nameMAC);
                        cJSON *ip_in_Arr = cJSON_GetObjectItemCaseSensitive(node, nameIP);
                        cJSON *id_in_Arr = cJSON_GetObjectItemCaseSensitive(node, nameID);
                        cJSON *port_in_Arr = cJSON_GetObjectItemCaseSensitive(node, namePort);
                        cJSON *isAlive_in_Arr = cJSON_GetObjectItemCaseSensitive(node, nameIsAlive);
                        
                        if(cJSON_IsNumber(id_in_Arr))
                            temp->id = id_in_Arr->valueint;
                        if(cJSON_IsString(ip_in_Arr))
                            strcpy(temp->ip_wifi, ip_in_Arr->valuestring);
                        if(cJSON_IsString(mac_in_Arr))
                            strcpy(temp->mac_wifi, mac_in_Arr->valuestring);
                        if(cJSON_IsNumber(port_in_Arr)) 
                            temp->port = port_in_Arr->valueint; 
                        if(cJSON_IsNumber(isAlive_in_Arr))   
                            temp->isAlive = isAlive_in_Arr->valueint;

                        child->childArr = realloc(child->childArr, sizeof(Node)* child->lenChildArr+1);
                        child->childArr[child->lenChildArr] = temp;
                        child->lenChildArr++;
                    }
                }
                //ESP_LOGE(TAG_DATA, "%s, goto heartbeat handler", __func__);
                HeartbeatHandler(jsonID->valueint, child);
                //free malloced strip_t & childArr from HandleCMD function
                //ESP_LOGI(TAG_DATA, "%s, lenchild %d", __func__, child->lenChildArr);
                for(uint8_t i = 0; i < child->lenChildArr;i++)
                    free(child->childArr[i]);
                free(child->childArr);
                free(child);
                //ESP_LOGI(TAG_DATA, "%s free done", __func__); 
            }
            
            cJSON_Delete(objPtr);
            return r;
        }
        else if(CMD_ERROR == r.cmd)
        {
            cJSON_Delete(objPtr);
            return r;
        }
        else
        {
            jsonMAC =   cJSON_GetObjectItemCaseSensitive(objPtr, nameMAC);
            jsonID =    cJSON_GetObjectItemCaseSensitive(objPtr, nameID);
            jsonIP =    cJSON_GetObjectItemCaseSensitive(objPtr, nameIP);
            jsonPort =  cJSON_GetObjectItemCaseSensitive(objPtr, namePort);
            
            if(jsonMAC == NULL){
                //ESP_LOGE(TAG_DATA, "mac is null");
            }else{
                strcpy(r.mac, jsonMAC->valuestring);
            }

            if( jsonID == NULL )
            {
                //ESP_LOGE(TAG_DATA, "id is null");
            }else{
                r.id = jsonID->valueint;
            }
            if(jsonIP == NULL)
            {
                //ESP_LOGE(TAG_DATA, "ip is null");
            }
            else{
                strcpy(r.ip, jsonIP->valuestring);
            }

            if(jsonPort == NULL)
            {
                //ESP_LOGE(TAG_DATA, "port is null");
            }
            else{
                r.port = jsonPort->valueint;
            }
            cJSON_Delete(objPtr);
            return r;
        }
        
    }
    else
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            ESP_LOGE(TAG_DATA, "Error json parse in handle incomming cmd: %s\n", error_ptr);
        }
        r.cmd = CMD_ERROR;
        cJSON_Delete(objPtr);
        return r;
    }      
}

/**
 * @brief sets the data (voltag_DATAe state, general state & temperature)
 * from the incoming data, if its is a root, pointer to temp_data have
 * to be send also, so it can be forwared.
 * 
 * @param data char* in json format
 * @param temp_data int*[out] to set temperature data 
 * @return int -1 on error 0 on OK
 */
int HandleIncomingData(char* data, double* temp_data)
{   
    cJSON *jsonGenState = NULL;
    cJSON *jsonVoltState = NULL;
    cJSON *jsonTemp = NULL;
    
    cJSON *objPtr = cJSON_Parse(data);
    if(objPtr == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            ESP_LOGE(TAG_DATA, "Error json parse in handle incoming data: %s\n", error_ptr);
        }
        cJSON_Delete(objPtr);
        return -1;
    }
    //received data for use so precess it
    jsonGenState = cJSON_GetObjectItemCaseSensitive(objPtr, nameGenState);
    jsonVoltState = cJSON_GetObjectItemCaseSensitive(objPtr, nameVoltageState);
    jsonTemp = cJSON_GetObjectItemCaseSensitive(objPtr, nameTemp);
    
    if (cJSON_IsNumber(jsonTemp))
    {
        //if root gets data, it have to send the temperature so get it and pass to data pointer
        if(temp_data != NULL)
            *temp_data = jsonTemp->valuedouble;
        else
            curReceivedTemp = jsonTemp->valuedouble;
    }
    if( cJSON_IsNumber(jsonGenState) )
    {
        int tempGenState = jsonGenState->valueint;
        if(tempGenState < -1 || tempGenState > 2)
        {
            ESP_LOGW(TAG_DATA, "not allowed general state");
        }
        else
        {
            curGeneralState = (ClosingState)tempGenState;
        }
    }
    if(cJSON_IsNumber(jsonVoltState))
    {

        int tempCurVoltageState = jsonVoltState->valueint;

        if(tempCurVoltageState < 0 || tempCurVoltageState > 1){
            ESP_LOGW(TAG_DATA,"not allowed voltag_DATAe state");
        }
        else
        {
            curVoltageState = (VoltageFreeState)tempCurVoltageState;
        }
    }
    cJSON_Delete(objPtr);
    return 0;
   
}
/**
 * @brief update incoming data
 * 
 * @param cmd cmd type
 * @param data char* data string
 * @return int -1 on failure, 0 on success
 */
int ReceiveClient(CMD cmd, char* data)
{
    switch (cmd)
    {
        case CMD_TO_CLIENT:
            if(HandleIncomingData(data, NULL) != 0)
            {
                ESP_LOGW(TAG_DATA,"handleIncomingData error");
                return -1;
            }
            break;
        case CMD_INIT_VALID:
            initSuccessfull = true; 
            break;
        case CMD_INIT_INVALID:
            initSuccessfull = false;
            curGeneralState = Err;
            break;
        default:
            ESP_LOGE(TAG_DATA, "received cmd:%d, not allowed for \
                        client", cmd);
            return -1;
            break;
    }
    return 0;
}