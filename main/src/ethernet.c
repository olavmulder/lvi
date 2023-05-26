#include "../inc/ethernet.h"
//https://docs.espressif.com/projects/esp-at/en/latest/esp32/AT_Command_Set/TCP-IP_AT_Commands.html#at-cipsend-send-data-in-the-normal-transmission-mode-or-wi-fi-passthrough-mode
const char *TAG_ETH = "ethernet";

#define PATTERN_CHR_NUM    (3)         /*!< Set the number of consecutive and identical characters received by receiver which defines a UART pattern*/
#define BUF_SIZE (1024)
#define RX_BUF_SIZE (BUF_SIZE)

static volatile bool uartTaskStarted = false;
QueueHandle_t uart2_queue;

static volatile bool isRead = false;
uint8_t* dtmp;
size_t size;

static void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    size_t buffered_size;
    dtmp = (uint8_t*) malloc(RX_BUF_SIZE);
    while(1){
        //Waiting for UART event.
        if(xQueueReceive(uart2_queue, (void * )&event, (portTickType)portMAX_DELAY)) {
            bzero(dtmp, RX_BUF_SIZE);
            ESP_LOGI(TAG_ETH, "uart[%d] event:", UART_PORT_NUM);
            switch(event.type) {
                //Event of UART receving data
                //We'd better handler data event fast, there would be much more data events than
                //other types of events. If we take too much time on data event, the queue might
                //be full.
                case UART_DATA:
                    //data = waitMsg(1000);
                    uart_read_bytes(UART_PORT_NUM, dtmp, event.size, portMAX_DELAY);

                    //ESP_LOGI(TAG_ETH, "[DATA EVT]:");
                    uart_flush_input(UART_PORT_NUM);
                    ESP_ERROR_CHECK(uart_wait_tx_done(UART_PORT_NUM, (portTickType)portMAX_DELAY));
                    uart_write_bytes(UART_PORT_NUM, (const char*) dtmp, event.size);
                    ESP_LOGI(TAG_ETH, "[UART DATA]: %s", dtmp);

                    break;
                //Event of HW FIFO overflow detected
                case UART_FIFO_OVF:
                    ESP_LOGI(TAG_ETH, "hw fifo overflow");
                    // If fifo overflow happened, you should consider adding flow control for your application.
                    // The ISR has already reset the rx FIFO,
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(UART_PORT_NUM);
                    xQueueReset(uart2_queue);
                    break;
                //Event of UART ring buffer full
                case UART_BUFFER_FULL:
                    ESP_LOGI(TAG_ETH, "ring buffer full");
                    // If buffer full happened, you should consider encreasing your buffer size
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(UART_PORT_NUM);
                    xQueueReset(uart2_queue);
                    break;
                //Event of UART RX break detected
                case UART_BREAK:
                    ESP_LOGI(TAG_ETH, "uart rx break");
                    break;
                //Event of UART parity check error
                case UART_PARITY_ERR:
                    ESP_LOGI(TAG_ETH, "uart parity error");
                    break;
                //Event of UART frame error
                case UART_FRAME_ERR:
                    ESP_LOGI(TAG_ETH, "uart frame error");
                    break;
                //UART_PATTERN_DET
                case UART_PATTERN_DET:
                    uart_get_buffered_data_len(UART_PORT_NUM, &buffered_size);
                    int pos = uart_pattern_pop_pos(UART_PORT_NUM);
                    ESP_LOGI(TAG_ETH, "[UART PATTERN DETECTED] pos: %d, buffered size: %d", pos, buffered_size);
                    if (pos == -1) {
                        // There used to be a UART_PATTERN_DET event, but the pattern position queue is full so that it can not
                        // record the position. We should set a larger queue size.
                        // As an example, we directly flush the rx buffer here.
                        uart_flush_input(UART_PORT_NUM);
                    } else {
                        uart_read_bytes(UART_PORT_NUM, dtmp, pos, 100 / portTICK_PERIOD_MS);
                        uint8_t pat[PATTERN_CHR_NUM + 1];
                        memset(pat, 0, sizeof(pat));
                        uart_read_bytes(UART_PORT_NUM, pat, PATTERN_CHR_NUM, 100 / portTICK_PERIOD_MS);
                        ESP_LOGI(TAG_ETH, "read data: %s", dtmp);
                        ESP_LOGI(TAG_ETH, "read pat : %s", pat);
                    }
                    break;
                //Others
                default:
                    ESP_LOGI(TAG_ETH, "uart event type: %d", event.type);
                    break;
            }
        }

    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}
typedef struct _buffer_t{
    char* data;
    size_t size;
    size_t capacity;
}buffer_t;
//added
buffer_t* _HandleUARTBuffer(buffer_t* buffer, int len, uint8_t* temp_buf)
{
    if(buffer->size + len > buffer->capacity)
    {
        size_t new_capacity = buffer->size +len;
        buffer->data = realloc(buffer->data, new_capacity);
        if(buffer->data != NULL)
            buffer->capacity = new_capacity;
    }
    memcpy(buffer->data + buffer->size, temp_buf, len);
    buffer->size +=len; 
    return buffer;
}
char* waitMsg(unsigned long time)
{

    buffer_t buffer;
    buffer.data = malloc(100);
    buffer.size = 0;
    buffer.capacity = 100;

    unsigned long start = esp_timer_get_time() / 1000;
    while (1) {
        size_t availableBytes = 0;
        uart_get_buffered_data_len(UART_NUM_2, &availableBytes);
        if (availableBytes > 0 || (esp_timer_get_time() / 1000 - start) < time) {
            uint8_t temp_buf[availableBytes];
            int len = uart_read_bytes(UART_NUM_2, temp_buf, sizeof(temp_buf), 100 / portTICK_RATE_MS);
            if (len > 0) {
                buffer = *(_HandleUARTBuffer(&buffer, len, temp_buf));
            }
            //anti watchdog?
            vTaskDelay(50 / portTICK_RATE_MS);
        } 
        else
            break;
    }

    return buffer.data;
}

int UART_SendData(const char* msg)
{
    const int len = strlen(msg);
    const int txBytes = uart_write_bytes(UART_PORT_NUM, msg , len);
    //ESP_LOGI(TAG_ETH, "wrote %d bytes", txBytes);
    return txBytes;
}
/**
 * @brief Get the Return Value object
 *  return -2 on busy, so send again
 * @param waitTime 
 * @param msgToFilterOn 
 * @return true 
 * @return -4 ERROR received, -2 busy received, -1 nothing, 0 success received,  
 */
int UART_GetReturnValue(unsigned long waitTime, const char* msgToFilterOn, const char* msgToFilterOn2)
{
    char* res = waitMsg(waitTime);
    int ret = -1;
    //ESP_LOGI(TAG_ETH, "rs: %s", res);
    char *pos;
    if(msgToFilterOn2 != NULL)
    {
        pos = strstr(res, msgToFilterOn2);
        if(pos != NULL)
        {
            return -4;
        }
    }
    pos = strstr(res, msgToFilterOn);
    if(pos != NULL)
    {
        //ESP_LOGI(TAG_ETH, "%s return", msgToFilterOn);
        ret = 0;
    }else{
        ESP_LOGW(TAG_ETH, "not found %s", msgToFilterOn);
        ret = -1;
    }
    free(res);
    return ret;
}
/**
 * @brief receive data from tcp conection
 * and memcpy it in data pointer
 *  
 * @param data cahr*(in)
 * @param amountMaxRead max chars to read, which is sizeof data
 * @return int -1 on failure, 0 on success
 */
int ReceiveTCPData(char* data, size_t amountMaxRead, int num)
{
    
    const char *msg1 = "AT+CIPRECVDATA=";//passive receive
    char buf[30];
    int ret = -1;
    snprintf(buf, 30, "%s%d,%d\r\n", msg1, num, amountMaxRead);
    //ESP_LOGI(TAG_ETH, "%s: buf: %s", __func__, buf);
    UART_SendData(buf);
    char* res = waitMsg(500);
    if(res != NULL)
    {
        //ESP_LOGI(TAG_ETH, "%s: res: %s", __func__, res);
        //check error
        char *pos;
        pos = strstr(res, "+CIPRECVDATA:");
        char bufRes[10];
        if(pos != NULL)
        {
            pos+=strlen("+CIPRECVDATA:");//14;
            int i = 0;
            while(*pos != ',')
            {
                if(i < 10)
                {
                    bufRes[i] = *(pos);
                    i++;
                    pos++;
                }else  
                    break;
            }
            bufRes[i] = '\0';
            
            size_t actual_read;
            sscanf(bufRes, "%d", &actual_read); 
            memcpy(data, ++pos, actual_read);
            //ESP_LOGI(TAG_ETH, "%s becomes: actual read size:%d", bufRes, actual_read);        
            ret = 0;
        }else{
            ret = -1;
        }
    }
    free(res);
    return ret;
}

/**
 * @brief send msg over ethernet to tcp client 'num'
 * 
 * @param buffer msg
 * @param size size of msg
 * @param num tcp link num(0 = broadcast 1-3 = servers)
 * @return * int 
 */
int SendTCPData(char* buffer, size_t size, int num) {
    const char* msg1 = "AT+CIPSEND=";
    char buf[20];
    snprintf(buf, 20, "%s%d,%d\r\n", msg1,num,size);
    //ESP_LOGI(TAG_ETH, "%s: buf: %s", __func__, buf);
    //TODO: semaphore 
    UART_SendData(buf);
    //sendCMD("AT+CIPSEND=" + char*(size) + "");
    vTaskDelay(500/portTICK_RATE_MS);
    //delay(500);
    UART_SendData(buffer);
    UART_SendData("\r\n");
    //receive data, if error gencurGeneralState = Err
    return UART_GetReturnValue(500, "SEND OK", "ERROR" );
    //start task 
   
}
/**
 * @brief 
 * 
 * @param num 0, broadcast, 1-3 server ip's
 * @return int 
 */
int ReceiveType(int num)
{
    int ret = -1;
    size_t amountMaxRead = 1000;
    char data[amountMaxRead];
    if(ReceiveTCPData(data, amountMaxRead, num) == 0)
    {
        //ESP_LOGI(TAG_ETH, "%s:received data:%s", __func__, data);
        mesh_data r =  HandleIncomingCMD(NULL, data);
        if(r.cmd == CMD_ERROR)
            ret = -1;
        else if(strcmp(mac_eth, r.mac) == 0)
            ret = ReceiveClient(r.cmd, data);
        else
            ret = -3;
    }
    else
    {
        memset(data, '\0', amountMaxRead);
        ret = -2;
    }
    return ret;
}
/**
 * @brief receive data over ethernet connection, and handle the
 * received data in 
 *  
 * @return int -1 on faillure, -2 no data, -3 not my data, 0 on success
 */
int ReceiveEth()
{
   
    int ret =-1, ret2 = -1;
    if( xSemaphoreTake( xSemaphoreEth, ( TickType_t ) 1000 ) == pdTRUE )
    {
        //ret = ReceiveType(0);
        ret = ReceiveType(currentServerIPCount_ETH+1);
        xSemaphoreGive(xSemaphoreEth);
    }else
    {
        ESP_LOGE(TAG_ETH, "%s, semaphore taken", __func__);
    }
    //return failure if there was a failure
    //if(ret != -1 || ret2 != -1)ret = -1;
    return ret;
}


//init function
void ResetModule()
{
    const char *msg = "AT+RST\r\n";
    if(UART_SendData(msg) <= 0)
    {
        ESP_LOGW(TAG_ETH, "WRITE <= 0 byets");
    }
}
int UARTInit()
{
    #define UART_PORT_NUM UART_NUM_2 // Use UART1
    #define UART_TX_PIN 14          // TX pin of UART1
    #define UART_RX_PIN 13          // RX pin of UART1
    #define UART_BAUD_RATE 9600   // Baud rate of UART1
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    if(!uart_is_driver_installed(UART_PORT_NUM))
        uart_driver_install(UART_PORT_NUM, 1024*2, 0, 0,NULL , 0); //&uart2_queue
    uart_param_config(UART_PORT_NUM, &uart_config);
    uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        //uart_driver_install(UART_PORT_NUM, 1024*2, 1024*2, 0, NULL, 0);
    //Create a task to handler UART event from ISR
    //uart_enable_pattern_det_baud_intr(UART_PORT_NUM, '+', PATTERN_CHR_NUM, 9, 0, 0);
    //Reset the pattern queue length to record at most 20 pattern positions.
    //uart_pattern_queue_reset(UART_PORT_NUM, 20);
    return 0;
}
/**
 * @brief init ethernet with uart commands, 
 * if amount try is too high switch to wifi
 * 
 * @return true 
 * @return false 
 */
int InitEthernetConnection()
{
    ResetModule();
    vTaskDelay(500 / portTICK_RATE_MS);
    
    while(CheckDeviceConnect() != 0)
    {
        //ESP_LOGE(TAG_ETH, "%s checkDeviceConnect failed", __func__);
        vTaskDelay(2000 / portTICK_RATE_MS);
    }
    vTaskDelay(500 / portTICK_RATE_MS);

    while(CheckETHConnect() != 0)
    {
        vTaskDelay(2000 / portTICK_RATE_MS);
        //ESP_LOGE(TAG_ETH, "%s checkETHConnect():-1", __func__ );
    }
    vTaskDelay(500 / portTICK_RATE_MS);

    ESP_LOGI(TAG_ETH, "%s checkETHConnect ip:%s", __func__, ip_eth);
   // gotIPAddress = true;
    while(GetMAC() != 0)
    {
        vTaskDelay(2000 / portTICK_RATE_MS);
        //ESP_LOGE(TAG_ETH, "GetMAC failed");
    }
    vTaskDelay(500 / portTICK_RATE_MS);

    ESP_LOGI(TAG_ETH, "%s, currentServerIPCount %d", __func__, currentServerIPCount_ETH);
    while(SetMultiConnection() != 0)
    {
        vTaskDelay(2000 / portTICK_RATE_MS);
        //ESP_LOGE(TAG_ETH, "SetMultiConnection failed");
    }
    /*while(CreateTCPClient("255.255.255.255", TCP_SOCKET_SERVER_PORT, 0) != 0)
    {
        vTaskDelay(2000 / portTICK_RATE_MS);
        static int amount;
        amount ++;
        if(amount > 3)
        {
            IncrementServerIpCount(false);
            amount = 0;
        }
        ESP_LOGE(TAG_ETH, "%s createTCPClient() error curr %d", __func__ , currentServerIPCount_ETH);
    }*/
    while(CreateTCPClient(SERVER_IP[currentServerIPCount_ETH], TCP_SOCKET_SERVER_PORT, currentServerIPCount_ETH+1) != 0)
    {
        vTaskDelay(2000 / portTICK_RATE_MS);
        static int amount;
        amount ++;
        if(amount > 3)
        {
            IncrementServerIpCount(false);
            amount = 0;
        }
        //ESP_LOGE(TAG_ETH, "%s createTCPClient(%s) error curr %d", __func__ ,SERVER_IP[currentServerIPCount_ETH], currentServerIPCount_ETH);
    }
    vTaskDelay(500 / portTICK_RATE_MS);
    ESP_LOGI(TAG_ETH, "createTCPClient: 0");
    while(SetTCPMode(true) != 0)
    {
        vTaskDelay(2000 / portTICK_RATE_MS);
    }
    is_eth_connected = true;
    return 0;
}

/*! @brief Detecting device connection status
    @return True if good connection, false otherwise. */
int CheckDeviceConnect() {
    //connection device
    const char* msg1 = "AT\r\n";
    //ESP_ERROR_CHECK(uart_flush_input(UART_PORT_NUM));
    if(UART_SendData(msg1) <= 0)
    {
        ESP_LOGW(TAG_ETH, "WRITE <= 0 byets");
        return -1;
    }
    //ESP_ERROR_CHECK(uart_wait_tx_done(UART_PORT_NUM, (portTickType)portMAX_DELAY));

    return UART_GetReturnValue(500, "OK", NULL);
}

/*! @brief Detecting device ETH connection status
    @return True if good connection, false otherwise. */
int CheckETHConnect() {
    const char* msg1 = "AT+CIPETH?\r\n";
    char *pos;
    int ret;
    while(UART_SendData(msg1) <= 0)
    {
        ESP_LOGW(TAG_ETH, "WRITE <= 0 bytes");
    }
    char* res = waitMsg(1000);
    //ESP_LOGI(TAG_ETH, "%s,res: %s",__func__, res);
    
    pos = strstr(res, "192");

    if(pos != NULL)
    {
        int i = 0;
        while(*pos != '\"' && *pos != '\r')
        {
            ip_eth[i] = *(pos);
            i++;
            pos++;
        }
        ip_eth[i] = '\0';
        ESP_LOGI(TAG_ETH, "ok return ip:%s", ip_eth);
        ret = 0;
    }else{
        ESP_LOGW(TAG_ETH, "neither 192");
        ret =  -1;
    }
    free(res);
    return ret;
}
int GetMAC()
{
    int ret;
    char *pos;
    char* res;
    const char* msg1 = "AT+CIPETHMAC?\r\n";
    
    while(UART_SendData(msg1) <= 0)
    {
        ESP_LOGW(TAG_ETH, "WRITE <= 0 bytes");
        vTaskDelay(50 / portTICK_RATE_MS);
    }
    res = waitMsg(1000);
    ESP_LOGI(TAG_ETH, "rs: %s", res);
        
    pos = strstr(res, "\"");
    if(pos != NULL)
    {
        int i = 0;
        while(*++pos != '\"')
        {
            mac_eth[i] = *(pos);
            i++;
        }
        mac_eth[i] = '\0';
        ESP_LOGI(TAG_ETH, "ok return mac:%s", mac_eth);
        ret =  0;
    }else{
        ESP_LOGW(TAG_ETH, "neither 192");
        ret =  -1;
    }
    free(res);
    return ret;

}
int SetMultiConnection()
{
    int res;
    const char* msg2 = "AT+CIPCLOSE\r\n";
    if(UART_SendData(msg2) <= 0)
    {
        ESP_LOGE(TAG_ETH, "UART_senddata <= 0");
    }
    vTaskDelay(1000 / portTICK_RATE_MS);

    res = UART_GetReturnValue(1000, "OK", NULL);

    vTaskDelay(500 / portTICK_RATE_MS);

    const char* msg1 = "AT+CIPMUX=1\r\n";
    if(UART_SendData(msg1) <= 0)
    {
        ESP_LOGE(TAG_ETH, "UART_senddata <= 0");
    }
    vTaskDelay(1000 / portTICK_RATE_MS);
    res = UART_GetReturnValue(1000, "OK", NULL);
    return res;
}
/*! @brief Create a TCP client
    @return True if create successfully, false otherwise. */
int CreateTCPClient(const char* ip, int port, int num) {
    int res;
    const char* msg1 = "AT+CIPCLOSE\r\n";
    if(UART_SendData(msg1) <= 0)
    {
        ESP_LOGE(TAG_ETH, "UART_senddata <= 0");
    }
    vTaskDelay(1000 / portTICK_RATE_MS);

    res = UART_GetReturnValue(1000, "OK", NULL);

    //delay(500);
    char msg2[100];
    snprintf(msg2, 100, "AT+CIPSTART=%d,\"TCP\",\"%s\",%d\r\n",num, ip, port);
    //sendCMD("AT+CIPSTART=\"TCP\",\"" + ip + "\"," + char*(port));
    if(UART_SendData(msg2) <= 0)
    {
        ESP_LOGE(TAG_ETH, "UART_senddata <= 0");
    }
    //receive data
    res = UART_GetReturnValue(1000, "CONNECT", NULL);
    return res;
}

/**
 * @brief mode 1 - passive, data keeps in buffer
 * 
 * @param mode 
 * @return true 
 * @return false 
 */
int SetTCPMode(bool mode)
{
    const char *msg1 = "AT+CIPRECVMODE=";
    char buf[38];
    snprintf(buf, 38, "%s%d\r\n", msg1, (char)mode);
    ESP_LOGI(TAG_ETH, "%s: buf=%s", __func__, buf);
    UART_SendData(buf);
    vTaskDelay(300/portTICK_RATE_MS);
    
    return UART_GetReturnValue(500, "OK", NULL);
}
