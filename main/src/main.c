#include "unity.h"
#include "../inc/communication.h"
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
int8_t currentServerIPCount_ETH = 0;
int8_t currentServerIPCount_WIFI = 0;

uint8_t serverTimeout;
int8_t amountMeshClients = 0;
//ip stirngs
esp_ip4_addr_t s_current_ip;
                                        //   eth network       wifi network     wifi home 
const char *SERVER_IP[AMOUNT_SERVER_ADDRS] = {"192.168.2.109","192.168.2.106"};//, "192.168.178.25", }; 
char ip_eth[LENGTH_IP];
char mac_eth[LENGTH_IP];
char ip_wifi[LENGTH_IP];
char mac_wifi[LENGTH_IP];

//rest values
char idName[5];

//semaphores
SemaphoreHandle_t xSemaphoreWiFiMesh;
SemaphoreHandle_t xSemaphoreCOAP;
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
    xSemaphoreEth  = xSemaphoreCreateMutex();
    xGuiSemaphore = xSemaphoreCreateMutex();

    if( xSemaphoreWiFiMesh != NULL && xSemaphoreCOAP != NULL 
        && xSemaphoreEth != NULL)
    {
        //init uart & wifi
        while(UARTInit() != 0)vTaskDelay(50/portTICK_RATE_MS);
        ESP_LOGI(TAG_MAIN, "uart init success");
        //WiFiInit();

        xTaskCreatePinnedToCore(InitEthernet, "init eth", 4024, NULL, 6, 
                         NULL, 1);
        //xTaskCreatePinnedToCore(CoAP_Client_InitTask, "init_coap", 2024, NULL, 5, 
        //                   NULL, 0);
        xTaskCreatePinnedToCore(DisplayTask, "gui_task", 6096, NULL, 3,
                           NULL, 1);
    
        xTaskCreatePinnedToCore(TemperatureTask, "temp_task", 1024, NULL, 2,
                           NULL, 1);
        xTaskCreatePinnedToCore(SendDataToServerTask, "send_task", 6000 , NULL, 7,
                           NULL, 1);
        xTaskCreatePinnedToCore(ReceiveDataTask, "receive_task", 6000, NULL, 8,
                           NULL, 0);
        
        //starts heartbeat timer
        monitoring_head = StartHeartbeat(monitoring_head);
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
        //curGeneralState = Err;
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
        int64_t begin = esp_timer_get_time() / 1000; // Convert to milliseconds
        SetComState();
        int res = SendToServer();
        if(res < 0){
            ESP_LOGW(TAG_MAIN, "SEND TO SERVER; %d", res);
            //curGeneralState = Err;
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
 */
void DisplayTask()
{
    while (1) {
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
        MLX90614_GetTa(MLX90614_DEFAULT_ADDRESS, &curMeasuredTemp);
        vTaskDelay(5000/portTICK_RATE_MS);
    }
    vTaskDelete(NULL);

}

