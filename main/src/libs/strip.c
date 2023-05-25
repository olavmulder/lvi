#include "../inc/libs/strip.h"
const char* TAG_STRIP = "STRIP:";

/**
 * @brief make new node, init it with data and return it
 * 
 * @param id 
 * @param ip 
 * @param mac 
 * @param port 
 * @param isAlive 
 * @return Node* 
 */
//not needed anymore...
/*Node* NewNode(int id, char* ip, char *mac, uint16_t port, bool isAlive)//data
{
   Node *temp = (Node*)malloc(sizeof(Node));
   temp->id = id;
   strcpy(temp->ip_wifi,ip);
   strcpy(temp->mac_wifi,mac);
   temp->port = port;
   temp->isAlive = isAlive;
   InitTimer(temp);
   return temp;
}*/
/**
 * @brief find id in monitoring linked list
 * 
 * @param id 
 * @return int 
 */
//PSD done
int FindID(int id)
{
    //ESP_LOGI(TAG_STRIP, "%s, id = %d", __func__, id);
    int max = monitoring_head->lenChildArr;
    int index = 0;
    //ESP_LOGI(TAG_STRIP, "%s, id = %d, max = %d, arr id = %d"
        //, __func__, id, max, monitoring_head->childArr[0]->id);
   
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
//PSD done
strip_t* AddNodeToStrip(strip_t* strip, Node* n)
{
   //alloc a new child in Node r
   // ESP_LOGI(TAG_STRIP, "%s, id = %d", __func__, n->id);
    uint8_t inID = n->id;
    //only add new node if id of incoming node is not in monitoring_head
    int res = FindID(inID);
    if(res != 0)
    {
        strip->childArr = realloc(strip->childArr, sizeof(Node*) * ++strip->lenChildArr);
        strip->childArr[strip->lenChildArr-1] = (Node*)malloc(sizeof(Node));
        memcpy(strip->childArr[strip->lenChildArr-1],n, sizeof(Node)); 
        //strip->lenChildArr++;
        InitTimer(strip->childArr[strip->lenChildArr-1]);
        ESP_LOGI(TAG_STRIP, "%s, strip->lenChildAddr = %d", __func__, strip->lenChildArr);

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
/*void FindAllChildren(Node *node) {
    //printf("Children of node %d:\n", node->id);
    for (int i = 0; i < node->num_childs; i++) {
        if (node->childs != NULL){
            //add all nodes to idArr
            if(node->idArr == NULL)
            {
                node->idArr = (uint8_t*)malloc(sizeof(uint8_t)); 
            }else{
                node->idArr = (uint8_t*)realloc(node->idArr, sizeof(uint8_t) * (node->lenArr+1) );
            }
            node->idArr[node->lenArr] = node->childs[i]->id;
            node->lenArr++;
            //printf("%p=%d len %d\n", &node->idArr, node->idArr[node->lenArr-1], node->lenArr);
        }
    }
    //printf("\n");
    for (int i = 0; i < node->num_childs; i++) {
        if (node->childs[i] != NULL) {
            findAllChildren(node->childs[i]);
        }
    }
}

int MaxDepth(Node *root) 
{
    if (root == NULL) {
        return 0;
    }
    int maxChildDepth = 0;
    for (int i = 0; i < root->num_childs; i++) {
        int childDepth = maxDepth(root->childs[i]);
        if (childDepth > maxChildDepth) {
            maxChildDepth = childDepth;
        }
    }
    return 1 + maxChildDepth;
}

//for all nodes update dependency
//dependency = nodes lower in tree = childs of childs of child for maxDept
void UpdateDependancy(Node** root, int totalNodes)
{
    int max_depth = maxDepth(root[0]);  
    printf("max depth %d\n", max_depth);
    //for all levels in tree check for every node if it has childs
    for(uint8_t n = 0; n < max_depth; n++){
        for(uint8_t i = 0;i < totalNodes ;i++)
        {                
            for(uint8_t j = 0; j < root[i]->num_childs;j++)
            {
                //if it has childs-> copy for every child his child to idArr 
                //resize idArr to size of own + size of idArr of child[j]
                root[i]->idArr = realloc(root[i]->idArr, sizeof(uint8_t) * (root[i]->lenArr+root[i]->childs[j]->lenArr));
                uint8_t indexToAdd = root[i]->lenArr;
                printf("index to Add:%d for node %d\n", indexToAdd, i);
                //copy every id of the child[j]->idArr of root[i] to the idArr of root[i]
                for(uint8_t m = 0; m < root[i]->childs[j]->lenArr; m++)
                {
                    uint8_t idToFind = root[i]->childs[j]->idArr[m];
                    uint8_t tempIndex = 0;
                    bool found = false;
                    //search for idArr
                     while(tempIndex < root[i]->lenArr)
                     {
                        if(root[i]->idArr[tempIndex] == idToFind){
                            found = true;
                            printf("m%d->found %d\n", m, idToFind);
                            break;
                        }
                        tempIndex++;
                     }
                     if(!found)
                     {
                        root[i]->idArr[indexToAdd] = idToFind;
                        indexToAdd++;
                        root[i]->lenArr++;
                        printf("add: %d to node %d\n", idToFind, i);
                     }
                    
                }
                root[i]->idArr = realloc(root[i]->idArr, sizeof(uint8_t) * (root[i]->lenArr));
            }
        }
    }
    
}
*/