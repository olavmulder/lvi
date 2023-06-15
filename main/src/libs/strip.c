#include "../inc/libs/strip.h"
const char* TAG_STRIP = "STRIP:";

/**
 * @brief find id in monitoring linked list
 * 
 * @param id 
 * @return int 
 */
int FindID(int id)
{
    int max = monitoring_head->lenChildArr;
    int index = 0;
    
    while(index < max)
    {
      if(monitoring_head->childArr[index]->id == id)
        return 0;
      index++;
    }
   return -1;
}
/**
 * @brief add node to node strip when it isn't there already
 * , and init timer
 * 
 * @param strip 
 * @param n 
 * @return strip_t* 
 */
strip_t* AddNodeToStrip(strip_t* strip, Node* n)
{
    uint8_t inID = n->id;
    //only add new node if id of incoming node is not in monitoring_head
    int res = FindID(inID);
    if(res != 0)
    {
        strip->childArr = realloc(strip->childArr, sizeof(Node*) * ++strip->lenChildArr);
        strip->childArr[strip->lenChildArr-1] = (Node*)malloc(sizeof(Node));
        memcpy(strip->childArr[strip->lenChildArr-1],n, sizeof(Node)); 
        InitTimer(strip->childArr[strip->lenChildArr-1]);
    }
    return strip;
}
/**
 * @brief show what is in monitoring_head
 * 
 */
void DisplayMonitoringString()
{
    ESP_LOGI(TAG_STRIP, "%s",__func__);
   
    for(uint8_t i = 0; i < monitoring_head->lenChildArr;i++)
    {
        ESP_LOGI(TAG_STRIP, "id:%d, isAlive: %d, ip:%s\n", monitoring_head->childArr[i]->id,
                monitoring_head->childArr[i]->isAlive, monitoring_head->childArr[i]->ip_wifi);
    }
}