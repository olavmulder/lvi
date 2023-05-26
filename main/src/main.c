/* Mesh Internal Communication Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
/**
 * @brief errors...
 *  mesh: [mesh_schedule.c,3130] [WND-RX]max_wnd:1, 1200 ms timeout, 
 *          seqno:0, xseqno:24, no_wnd_count:0
    * 498<alloc>fail, recv ctx, heap:9340, from 30:c6::7:232>up(0, be:0), down(0, be:0), mgmt:0, xon(req:0, rsp:0), bcast:0, wnd(0, parent:00:00:00:00:00:00)
    I (474528) mesh: [RXQ]<max:32 = cfg:32 + extra:0>self:0, <max:32 = cfg:32 + extra:0>tods:0
    :32 = cfg:32 + extra:0>tods:0
    
    probleem:   current server ontvangt heartbeat, volgt toe aan strip
                current server gaat door, strip blijft werken,
                ontvangt geen data, want verbroken, timer loopt af,
                m.b.v sync gaat de status op zwart. Terwijl andere 
                server gewoon een normale heartbeat ontvangt, er is dus connectie
    oplossing:  alleen veranderen in changeData wanneer isAlive is false in        
                monitoring_head en closeState = -1.

    wat wel werkt en wat niet / functionaliteiten; 
    -- connectie weg bij 1 server, wordt niet gesynct met andere server. 
    -- connectie weg bij een node, wordt wel gesynct.   
    -- er wordt overgeschakeld wanneer een server zijn connectie met de 
        router verliest.\
    --indien connectie met verbroken server herstelt wordt -> 
        wordt er niet direct gesynct, maar er kan wel weer data heen er
        weer worden verstuurd, maar de kleur op de server die verbroken was
        wordt weer zwart, omdat er niet naar toe wordt gestuurd en dus wordt de 
        timer niet gerestart en dus wordt sync aangeroepen .
    
    --coap :root to server without; eth, client & sync
    --coap : root to multi server without: eth & leaf can handle 
            heartbeat, by setting gen state to black, but take a while to set
            black on server without any connection.
    -- coap:

    --after reconnection of wifi, sync doenst work imidetly
        and the reconnection server will set his own status to black,
        because the hearbeat message is not send to the reconnected one,
        so the timer will go off and sets the state to black

    --bij uitvallen van server, wordt er snel naar andere server geswitchted
        er kan ook weer terug worden geschakeld.
 * TODO:
    Zaterdag:
    1.  Unit test syncalg nog een keer? KAN THUIS!!
    2.  warnings gui fixen
    3.  
    Donderdag & (vrijdag)
    1.  Hearbeat eth fixen, monitoring list, id correct, 
        ip & mac ook correct? (ochtend)
    2.  Unit test hearbeat.(1.5 uur na pause)
    3.  heartbeat in verslag (na middag)
    
    Zondag & maandag wifi fixen?
    Dinsdag alles testen en test opstelling maken

    testing connection when eth is gone & back again
    testing hearbteat when eth is gone & when it is back again
    
    to fix
    V send cpy when re(connected)/(startup) on server side.
        downside: no detecion of not received msg is on otherside
    -- head_monitor will set state to black after reconnection.
    -- keep old value, when disconnect, so after reconnection old value can be
        set
    -- when node disconnected, not settable

 */
#include "unity.h"
#include "../inc/communication.h"
//#include "../inc/heartbeat.h"
#include "../inc/libs/M5Core2.h"
#include "../inc/libs/MLX90614_SMBus_Driver.h"
#include "../inc/libs/MLX90614_API.h"

#define MLX90614_DEFAULT_ADDRESS 0x5A // default chip address(slave address) of MLX90614
#define MLX90614_SDA_GPIO 32 // sda for MLX90614
#define MLX90614_SCL_GPIO 33 // scl for MLX90614

//states
volatile ClosingState curGeneralState = Err;
volatile VoltageFreeState curVoltageState;
double curMeasuredTemp;
double curReceivedTemp;
volatile CommunicationState comState = COMMUNICATION_NONE;

//wifi/eth connection variables
volatile bool initSuccessfull = false;
volatile bool is_mesh_connected = false;
volatile bool is_eth_connected = false;

volatile bool gotIPAddress = false;
volatile bool heartbeatEnable = false;

//hearbeat / error checking variables
uint8_t ethSendingFailureCounter;
int8_t currentServerIPCount_ETH = -1;
uint8_t currentServerIPCount_WIFI;

uint8_t serverTimeout;
int8_t amountMeshClients = 0;
//ip stirngs
esp_ip4_addr_t s_current_ip;
                                        //   eth network       wifi network     wifi home 
const char *SERVER_IP[AMOUNT_SERVER_ADDRS] = {"192.168.2.103","192.168.2.106"};//, "192.168.178.25", }; 
char ip_eth[LENGTH_IP];
char mac_eth[LENGTH_IP];
char ip_wifi[LENGTH_IP];
char mac_wifi[LENGTH_IP];

//rest values
char idName[5];

//semaphores
SemaphoreHandle_t xSemaphoreWiFiMesh;
SemaphoreHandle_t xSemaphoreCOAP;
SemaphoreHandle_t xSemaphoreI2C;
SemaphoreHandle_t xSemaphoreEth;

const char *TAG_MAIN = "main";

void InitEthernet();
void TemperatureTask();
void DisplayTask();
void SendDataToServerTask();
void ReceiveDataTask();
void CoAP_Client_InitTask();
//core 0
void app_main()
{
    strcpy(idName, ID);


    //init display
    m5core2_init();
    InitDisplay();
    //setup semaphores
    xSemaphoreCOAP = xSemaphoreCreateMutex();
    xSemaphoreWiFiMesh = xSemaphoreCreateMutex();
    xSemaphoreI2C = xSemaphoreCreateMutex();
    xSemaphoreEth  = xSemaphoreCreateMutex();
    xGuiSemaphore = xSemaphoreCreateMutex();

    if( xSemaphoreWiFiMesh != NULL && xSemaphoreCOAP != NULL 
        && xSemaphoreI2C != NULL && xSemaphoreEth != NULL)
    {
        //init uart & wifi
        while(UARTInit() != 0)vTaskDelay(50/portTICK_RATE_MS);
        ESP_LOGI(TAG_MAIN, "uart init success");
        WiFiInit();

        xTaskCreatePinnedToCore(InitEthernet, "init eth", 4024, NULL, 6, 
                           NULL, 0);
        xTaskCreatePinnedToCore(CoAP_Client_InitTask, "init_coap", 2024, NULL, 6, 
                           NULL, 0);
        xTaskCreatePinnedToCore(DisplayTask, "gui_task", 4096, NULL, 3,
                           NULL, 1);
    
        xTaskCreatePinnedToCore(TemperatureTask, "temp_task", 1024, NULL, 2,
                           NULL, 1);
        xTaskCreatePinnedToCore(SendDataToServerTask, "send_task", 6000 , NULL, 7,
                           NULL, 1);
        xTaskCreatePinnedToCore(ReceiveDataTask, "receive_task", 6000, NULL, 8,
                           NULL, 0);
        
        //starts heartbeat timer
        StartHeartbeat();

    }      
    while (1)
    {
        vTaskDelay(10000/portTICK_RATE_MS);
    }
                 
}
/**
 * @brief init ethernet task function
 * start init internet with for everything kind of init a while loop
 * after that every 10 secs, there will be checked if there isn't 
 * ethernet, if so init it again...
 */
void InitEthernet()
{
    
    InitEthernetConnection();    
    //ESP_LOGI(TAG_MAIN, "& ethernet init success");
    while(1)
    {
        //check every 10 is eth_connect variable is true
        //if not init again
        vTaskDelay(10000 / portTICK_RATE_MS);
        if(!is_eth_connected)
            InitEthernetConnection();
    }
    vTaskDelete(NULL);
}

void CoAP_Client_InitTask()
{
    CoAP_Client_Init();    
    while(1)
    {
        //check every 10 is eth_connect variable is true
        //if not init again
        vTaskDelay(10000 / portTICK_RATE_MS);
        if(!is_mesh_connected)
        {
            if(coapInitDone)
                CoAP_Client_Clear();
            CoAP_Client_Init();
        }
    }
    vTaskDelete(NULL);
}

/**
 * @brief Set the comState variable,
 * with ethernet preferd
 * 
 */
void SetComState()
{
    /**
     * is_eth_connected is updated by the sending over communication wireed
     * is_mesh_connected is updated by disconnection of mesh parent
    */
    static CommunicationState comStateOld;
    if(is_eth_connected)
        comState = COMMUNICATION_WIRED;
    else if(is_mesh_connected)
        comState = COMMUNICATION_WIRELESS;
    else
    {
        comState = COMMUNICATION_NONE;
        curGeneralState = Err;
    }
    if(comStateOld != comState)
    {
        comStateOld = comState;
        ESP_LOGI(TAG_MAIN, "%s: comState %d", __func__, comState);
    }
}

/**
 * @brief send every 5 seconds a standaard message with current 
 * temperature to server, over the current communication type
 * if failed handle send failure is called and the green will be
 * black soon.
 * also send heatbeat message
 */
void SendDataToServerTask()
{
    while(1){
        //heap_caps_check_integrity_all(true);
        int64_t begin = esp_timer_get_time() / 1000; // Convert to milliseconds
        SetComState();
        int res = SendToServer();
        if(res < 0){
            ESP_LOGW(TAG_MAIN, "SEND TO SERVER; %d", res);

            /*if(comState == COMMUNICATION_WIRED)
                HandleSendFailure(true);
            else if(comState == COMMUNICATION_WIRED)
                HandleSendFailure(false);
            else{
                if(coapInitDone)
                {
                    is_mesh_connected = true;
                }
            }*/
            curGeneralState = Err;
        }

        int64_t end = esp_timer_get_time() / 1000; // Convert to milliseconds
        int64_t time = 2500-(end-begin);
        if(time > 0)
            vTaskDelay( time/ portTICK_RATE_MS);
        begin = esp_timer_get_time() / 1000;
        
        SendHeartbeat();
        
        end = esp_timer_get_time() / 1000;
        time = 2500 - (end-time);
        if(time > 0)
            vTaskDelay(time / portTICK_RATE_MS);
        
        //heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);

    }
    vTaskDelete(NULL);
    
}
/**
 * @brief task which will check if there is anything to read
 * 
 */
void ReceiveDataTask()
{
    int res;
    while(1)
    {
        SetComState();
        res = ReceiveData();
        vTaskDelay(50 / portTICK_RATE_MS);
    }   
    vTaskDelete(NULL);
}
/**
 * @brief updated all display utils, callss lv_task handler
 * and wait
 * 
 */
void DisplayTask()
{
    //ESP_ERROR_CHECK( heap_trace_init_standalone(trace_record2, NUM_RECORDS) );
    while (1) {
        //ESP_ERROR_CHECK( heap_trace_start(HEAP_TRACE_LEAKS) );
        vTaskDelay(LV_TICK_PERIOD_MS*5);
        UpdateDisplayUtils();
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
            UpdateDisplayUtils();
            lv_task_handler();
            xSemaphoreGive(xGuiSemaphore);
        }
    }
    vTaskDelete(NULL);

}
/**
 * @brief measure temperature from sensor over i2c every 5 seconds
 * 
 */
void TemperatureTask()
{

    MLX90614_SMBusInit(MLX90614_SDA_GPIO, MLX90614_SCL_GPIO, 50000); // sda scl and 50kHz
    vTaskDelay(1000/portTICK_RATE_MS);
    while (1)
    {
        /*if( xSemaphoreTake( xSemaphoreI2C, ( TickType_t ) 10 ) == pdTRUE )
        {*/
            //MLX90614_GetTo(MLX90614_DEFAULT_ADDRESS, &to);
            MLX90614_GetTa(MLX90614_DEFAULT_ADDRESS, &curMeasuredTemp);
            //MLX90614_GetTa(MLX90614_DEFAULT_ADDRESS, &dumpInfo);
            //ESP_LOGI(TAG_MAIN, "TEMP: %f , %d\n", temperature, dumpInfo);
            //xSemaphoreGive(xSemaphoreI2C);
        //}
            vTaskDelay(5000/portTICK_RATE_MS);
    }
    vTaskDelete(NULL);

}

