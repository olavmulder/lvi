#include "../inc/display.h"
/*Todo ADD wifi icon*/
#define ID_TEXT_SIZE 5 //=9999 id's
static lv_style_t style_color;
static lv_style_t textStyle;
static lv_obj_t *block;
static lv_obj_t *tempString_obj;
static lv_obj_t *tempValue_obj;
static lv_obj_t *batteryIcon_obj;
static lv_obj_t *batteryText_obj;
static lv_obj_t *wifi_obj;
static lv_obj_t *voltage_obj;
static lv_obj_t *idString_obj;
static lv_color_t lights[4]; 
static lv_obj_t * btnmatrix_obj;
static lv_obj_t * id_textfield_obj;
static lv_obj_t * menu_obj;

static lv_obj_t * statusIcon_obj;
static lv_style_t styleInitStus;

static char idNameTemp[ID_TEXT_SIZE];
volatile uint8_t idInputCounter = 0;

#ifdef CORE2
static const char * btnm_map[] = {"1", "2", "3", "4", "5", "\n",
                                  "6", "7", "8", "9", "0", "\n",
                                  "OK","BackSpace", ""};

static void key_event_handler(lv_obj_t * obj, lv_event_t event)
{
    if(event == LV_EVENT_VALUE_CHANGED) {
        const char * txt = lv_btnmatrix_get_active_btn_text(obj);
        //printf("%s was pressed\n", txt);
        if(strcmp(txt, "OK") == 0 )
        {
          //dont allow empty input
          if(idNameTemp[0] == '\n' || (int)idNameTemp[0] == 0 )
            return;
          strcpy(idName, idNameTemp);
          idInputCounter = 0;
          memset(idNameTemp,0,sizeof(idNameTemp));
          char t[20];
          strcpy(t,"id name: ");
          strcat(t, idName);
          lv_label_set_text(idString_obj, t);
          ClearBtnmatrix();
          SetMenu();
          initSuccessfull = false;
          curGeneralState = Err;
        }
        else if(strcmp(txt, "BackSpace") == 0 )
        {
          //if there was input
          if(idInputCounter >0)
          {
            idNameTemp[idInputCounter-1] = '\n'; //delete it
            idInputCounter--;
          }
        }
        else
        {
          idNameTemp[idInputCounter] = txt[0];
          idInputCounter++;
          if(idInputCounter > ID_TEXT_SIZE)idInputCounter--;
        }
        lv_label_set_text(id_textfield_obj, idNameTemp);
    }
}

static void menu_event_handler(lv_obj_t *obj, lv_event_t e)
{
  

    if(e == LV_EVENT_CLICKED) {
        //printf("button event clicked\n");
        ClearMenu();
        Btnmatrix();
    }
}
void SetMenu()
{
    menu_obj = lv_btn_create(lv_scr_act(), NULL);
    lv_obj_set_event_cb(menu_obj, menu_event_handler);
    lv_obj_align(menu_obj, NULL, LV_ALIGN_IN_BOTTOM_RIGHT, 0, 0);

    static lv_style_t style_but;
    lv_style_init(&style_but);
    lv_style_set_size(&style_but, LV_BTN_PART_MAIN, 5);
    lv_obj_add_style(menu_obj, LV_BTN_PART_MAIN, &style_but);   
    lv_obj_t* label = lv_label_create(menu_obj, NULL);
    lv_label_set_text(label, "menu");

}
void ClearMenu()
{
    lv_obj_del(menu_obj);
    
}
void Btnmatrix(void)
{
    btnmatrix_obj = lv_btnmatrix_create(lv_scr_act(), NULL);
    lv_btnmatrix_set_map(btnmatrix_obj, btnm_map);
    //lv_btnmatrix_set_btn_width(btnmatrix_obj, 10, 2);        /*Make "Action1" twice as wide as "Action2"*/
    lv_btnmatrix_set_btn_ctrl(btnmatrix_obj, 10, LV_BTNMATRIX_CTRL_NO_REPEAT);
    lv_obj_align(btnmatrix_obj, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_event_cb(btnmatrix_obj, key_event_handler);

    id_textfield_obj = lv_label_create(lv_scr_act(), NULL);    
    lv_label_set_text(id_textfield_obj, "");
    lv_label_set_align(id_textfield_obj, LV_LABEL_ALIGN_LEFT);
    lv_obj_align(id_textfield_obj,btnmatrix_obj, LV_ALIGN_IN_TOP_LEFT, 0,-15);

    lv_obj_move_foreground(btnmatrix_obj);
    lv_obj_move_foreground(id_textfield_obj);
}
void ClearBtnmatrix()
{
    lv_obj_del(btnmatrix_obj);
}
#endif
void InitDisplay()
{
    // create bufffers
    lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(
        DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1 != NULL);
    
    lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(
      DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf2 != NULL);

    static lv_disp_buf_t disp_buf;
    lv_disp_buf_init(&disp_buf, buf1, buf2, DISP_BUF_SIZE);

    // display driver setup
    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 320;
    disp_drv.ver_res = 240;
    disp_drv.flush_cb = ili9341_flush;
    disp_drv.buffer = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    // touch setup
    #ifdef CORE2
    ft6x06_init(FT6236_I2C_SLAVE_ADDR);
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.read_cb = ft6x36_read;
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    lv_indev_drv_register(&indev_drv);
    #endif
    // tick timer
    // TODO: replace with callback?
  const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "periodic_gui",
    };
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(
        esp_timer_start_periodic(periodic_timer, 5 * 30));
    //draw all function first time;
    lights[0] = LV_COLOR_BLACK;
    lights[1] = LV_COLOR_RED;
    lights[2] = LV_COLOR_GREEN;
    lights[3] = LV_COLOR_YELLOW;

    //text big and black
    lv_style_set_text_font(&textStyle, LV_STATE_DEFAULT, &lv_font_montserrat_48); 
    lv_style_set_text_color(&textStyle,LV_STATE_DEFAULT, LV_COLOR_BLACK);

    
    //draw utils
    
    DrawBattery();
    DrawRectangle();
    DrawTemp();
    DrawWifi();
    DrawVoltageFree();
    #ifdef CORE2
    SetMenu();
    #endif
    DrawInitStatus();
    #ifdef DISPLAY_OPTION_2
      //draw ID
      //lv_style_set_text_font(&textStyle, LV_STATE_DEFAULT, &lv_font_montserrat_48); 
      idString_obj = lv_label_create(lv_scr_act(), NULL);
      char idString[15];
      #ifndef CORE2
        #ifdef ITEM2
          strcpy(idName, "6");
        #else
          strcpy(idName, "5");
        #endif
      #endif
      snprintf(idString, 15, "id name: %s", idName);
      lv_label_set_text(idString_obj, idString);
      lv_label_set_align(idString_obj, LV_LABEL_ALIGN_LEFT);
      lv_obj_set_pos(idString_obj, LV_VER_RES_MAX/2, 0);
    #endif
    #ifdef DISPLAY_OPTION_3
      idString_obj = lv_label_create(lv_scr_act(), NULL);
      char idString[15];
      snprintf(idString, 15, "id name: %s", idName);
      lv_label_set_text(idString_obj, idName);
      lv_label_set_align(idString_obj, LV_LABEL_ALIGN_LEFT);
      lv_style_set_text_color(&textStyleSmall,LV_STATE_DEFAULT, LV_COLOR_BLACK);
      lv_obj_add_style(idString_obj, LV_LED_PART_MAIN, &textStyleSmall);
      lv_obj_set_pos(idString_obj, 2,LV_VER_RES-20);
    #endif
   
}


void UpdateDisplayUtils()
{
    UpdateBatteryPercentageText();
    UpdateTempText();
    UpdateRectangle();
    UpdateConnection();
    UpdateVoltageFree();
    UpdateInitStatus();
}
void DrawInitStatus()
{
  statusIcon_obj = lv_label_create(lv_scr_act(), NULL);
  lv_label_set_text(statusIcon_obj, LV_SYMBOL_CLOSE);
  lv_style_set_text_color(&styleInitStus, LV_STATE_DEFAULT, LV_COLOR_RED);
  lv_style_set_text_font(&styleInitStus, LV_STATE_DEFAULT, &lv_font_montserrat_36);  /*Set a larger font*/
  lv_obj_add_style(statusIcon_obj, LV_BTN_PART_MAIN, &styleInitStus);
  lv_obj_set_pos(statusIcon_obj, 220,0);
  lv_label_set_align(statusIcon_obj, LV_LABEL_ALIGN_RIGHT);
}
void UpdateInitStatus()
{
  if(!initSuccessfull)
  {
    curGeneralState = Err;
    lv_style_set_text_color(&styleInitStus, LV_STATE_DEFAULT, LV_COLOR_RED);
    lv_label_set_text(statusIcon_obj, LV_SYMBOL_CLOSE);
  }else{
    lv_label_set_text(statusIcon_obj, LV_SYMBOL_OK);
    lv_style_set_text_color(&styleInitStus, LV_STATE_DEFAULT, LV_COLOR_GREEN);

  }
}
void DrawBattery()
{
    batteryIcon_obj = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_text(batteryIcon_obj, LV_SYMBOL_BATTERY_3);
    lv_obj_set_pos(batteryIcon_obj, 255,0);
    lv_label_set_align(batteryIcon_obj, LV_LABEL_ALIGN_RIGHT);

    batteryText_obj = lv_label_create(lv_scr_act(), NULL);
    
    UpdateBatteryPercentageText();
    
    lv_obj_set_pos(batteryText_obj, 280,0);
    lv_label_set_align(batteryText_obj, LV_LABEL_ALIGN_RIGHT);
}
void UpdateBatteryPercentageText()
{
  char buf[6];
  float bat = 0;
  #ifdef CORE2
  bat = GetBatteryLevel();
  #endif
  snprintf(buf, sizeof(buf), "%.0f%%", bat);
  lv_label_set_text(batteryText_obj, buf);  
}

void UpdateTempText()
{
  char tempValueString[8];
  snprintf(tempValueString, sizeof(tempValueString), "%.2f°C", curReceivedTemp);
  lv_label_set_text(tempValue_obj, tempValueString);
}
void UpdateConnection()
{
  //wifi connection  -> set wifi icon
  if(is_mesh_connected || is_eth_connected ){

    lv_label_set_text(wifi_obj, LV_SYMBOL_WIFI);

  }
  //no connection -> remove
  else
  {
    lv_label_set_text(wifi_obj, "");
  }
}
/**
 * @brief set screen brighness depending on general state
 * and change color
 */
void UpdateRectangle()
{
  #ifdef CORE2
  switch(curGeneralState)
  {
    case R:
      ScreenBreath(100);
      break;
    case Y:
      ScreenBreath(80);
      break;
    case G: 
      ScreenBreath(30);
      break;
    case Err:
      ScreenBreath(0);
      break;
    default:
      ScreenBreath(50);
      break;
  }
  #endif
  lv_style_set_bg_color(&style_color, LV_STATE_DEFAULT, lights[curGeneralState+1]);
  lv_obj_refresh_style(block, LV_OBJ_PART_ALL, LV_STYLE_PROP_ALL);
}
/**
 * @brief update voltage free indication
 * if curVoltageState -> room = voltage free
 * 
 */
void UpdateVoltageFree()
{
  if((bool)curVoltageState){

    lv_label_set_text(voltage_obj, LV_SYMBOL_CHARGE);

  }
  else if(!(bool)curVoltageState)
  {
    lv_label_set_text(voltage_obj, "");
  }
}
#ifdef DISPLAY_OPTION_1


void DrawTemp()
{
  tempString_obj = lv_label_create(lv_scr_act(), NULL);
  lv_label_set_text(tempString_obj, "temperatuur:");
  lv_label_set_align(tempString_obj, LV_LABEL_ALIGN_LEFT);
  lv_obj_set_pos(tempString_obj, 0,0);

  tempValue_obj = lv_label_create(lv_scr_act(), NULL);
  lv_obj_set_pos(tempValue_obj, 120,0);
  lv_label_set_align(tempValue_obj, LV_LABEL_ALIGN_LEFT);
}

void DrawWifi()
{
  wifi_obj = lv_label_create(lv_scr_act(), NULL);
  lv_label_set_text(wifi_obj, LV_SYMBOL_WIFI);
  lv_obj_set_pos(wifi_obj, 160,0);
}

void DrawVoltageFree()
{
  static lv_style_t voltageStyle;
  voltage_obj = lv_label_create(lv_scr_act(), NULL);
  lv_label_set_text(wifi_obj, LV_SYMBOL_CHARGE);
  lv_style_set_outline_width(&voltageStyle, LV_STATE_DEFAULT, 8);
  lv_style_set_outline_color(&voltageStyle, LV_STATE_DEFAULT, LV_COLOR_RED);
  lv_style_set_outline_pad(&voltageStyle, LV_STATE_DEFAULT, 8);  
  lv_obj_add_style(voltage_obj, LV_OBJ_PART_MAIN, &voltageStyle);
  lv_obj_set_pos(voltage_obj, 220,0);
}


void DrawRectangle()
{
  
  block = lv_obj_create(lv_scr_act(), NULL);
  lv_obj_set_size(block, 320, 200);

  lv_style_init(&style_color);
  lv_style_set_bg_color(&style_color, LV_STATE_DEFAULT, lights[0]);
  lv_obj_add_style(block, LV_LED_PART_MAIN, &style_color);
  lv_obj_align(block, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, 0);

}
#endif

/*background color*/
/*temperature lager*/
#ifdef DISPLAY_OPTION_2
void DrawTemp()
{
    lv_style_set_text_font(&textStyle, LV_STATE_DEFAULT, &lv_font_montserrat_48); 
    
    tempString_obj = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_text(tempString_obj, "");// "temperatuur"
    lv_label_set_align(tempString_obj, LV_LABEL_ALIGN_LEFT);
    lv_obj_set_pos(tempString_obj, 0,LV_VER_RES_MAX/2);
    lv_obj_add_style(tempString_obj, LV_LED_PART_MAIN, &textStyle);

    tempValue_obj = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_recolor(tempValue_obj, true);
    lv_obj_set_pos(tempValue_obj, LV_HOR_RES/2-20,LV_VER_RES/2);
    lv_label_set_align(tempValue_obj, LV_LABEL_ALIGN_CENTER);
    lv_obj_add_style(tempValue_obj, LV_LED_PART_MAIN, &textStyle);
}
void DrawRectangle()
{
  
    block = lv_obj_create(lv_scr_act(), NULL);
    lv_obj_set_size(block, 320, 200);

    lv_style_init(&style_color);
    lv_style_set_bg_color(&style_color, LV_STATE_DEFAULT, lights[0]);
    lv_obj_add_style(block, LV_LED_PART_MAIN, &style_color);
    lv_obj_align(block, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, 0);

}
void DrawWifi()
{
  wifi_obj = lv_label_create(lv_scr_act(), NULL);
  lv_label_set_text(wifi_obj, LV_SYMBOL_WIFI);
  lv_obj_add_style(wifi_obj, LV_LED_PART_MAIN, &textStyle);
  lv_obj_set_pos(wifi_obj, 0,0);
}

void DrawVoltageFree()
{
  static lv_style_t voltageStyle;
  voltage_obj = lv_label_create(lv_scr_act(), NULL);
  lv_label_set_text(wifi_obj, LV_SYMBOL_CHARGE);

  lv_style_set_outline_width(&voltageStyle, LV_STATE_DEFAULT, 8);
  lv_style_set_outline_color(&voltageStyle, LV_STATE_DEFAULT, LV_COLOR_BLUE);
  lv_style_set_outline_pad(&voltageStyle, LV_STATE_DEFAULT, 8);  
  lv_style_set_text_font(&voltageStyle, LV_STATE_DEFAULT, &lv_font_montserrat_48); 
  lv_style_set_text_color(&voltageStyle,LV_STATE_DEFAULT, LV_COLOR_BLACK);

  lv_obj_add_style(voltage_obj, LV_OBJ_PART_MAIN, &voltageStyle);
  lv_obj_set_pos(voltage_obj,20,LV_VER_RES/2);
}
#endif
/*background color*/
/*temperature smaller*/
/*bigger voltage state*/
#ifdef DISPLAY_OPTION_3
void DrawTemp()
{
    lv_style_set_text_font(&textStyle, LV_STATE_DEFAULT, &lv_font_montserrat_48); 
    
    tempString_obj = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_text(tempString_obj, "");// "temperatuur"
    lv_label_set_align(tempString_obj, LV_LABEL_ALIGN_LEFT);
    lv_obj_set_pos(tempString_obj, 0,LV_VER_RES_MAX/2);
    lv_obj_add_style(tempString_obj, LV_LED_PART_MAIN, &textStyle);

    tempValue_obj = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_recolor(tempValue_obj, true);
    lv_obj_set_pos(tempValue_obj, 60,0);
    lv_label_set_align(tempValue_obj, LV_LABEL_ALIGN_LEFT);
    lv_obj_add_style(tempValue_obj, LV_LED_PART_MAIN, &textStyle);
}
void DrawRectangle()
{
  
    block = lv_obj_create(lv_scr_act(), NULL);
    lv_obj_set_size(block, 320, 200);

    lv_style_init(&style_color);
    lv_style_set_bg_color(&style_color, LV_STATE_DEFAULT, lights[0]);
    lv_obj_add_style(block, LV_LED_PART_MAIN, &style_color);
    lv_obj_align(block, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, 0);

}
void DrawWifi()
{
  wifi_obj = lv_label_create(lv_scr_act(), NULL);
  lv_label_set_text(wifi_obj, LV_SYMBOL_WIFI);
  lv_obj_add_style(wifi_obj, LV_LED_PART_MAIN, &textStyle);
  lv_obj_set_pos(wifi_obj, 0,0);
}

void DrawVoltageFree()
{
  static lv_style_t voltageStyle;
  voltage_obj = lv_label_create(lv_scr_act(), NULL);
  lv_label_set_text(wifi_obj, LV_SYMBOL_CHARGE);

  /*lv_style_set_outline_width(&voltageStyle, LV_STATE_DEFAULT, 8);
  lv_style_set_outline_color(&voltageStyle, LV_STATE_DEFAULT, LV_COLOR_BLUE);
  lv_style_set_outline_pad(&voltageStyle, LV_STATE_DEFAULT, 8);  */
  lv_style_set_text_font(&voltageStyle, LV_STATE_DEFAULT, &lv_font_montserrat_48); 
  lv_style_set_text_color(&voltageStyle,LV_STATE_DEFAULT, LV_COLOR_BLACK);

  lv_obj_add_style(voltage_obj, LV_OBJ_PART_MAIN, &voltageStyle);
  lv_obj_set_pos(voltage_obj,LV_HOR_RES/2-10,LV_VER_RES/2);
}
#endif

void lv_tick_task(void *arg) 
{
  (void)arg;
  lv_tick_inc(LV_TICK_PERIOD_MS);
}

/*
//function from:
//https://forum.m5stack.com/topic/3510/extended-core2-lcd-brightness-function-demo
// &
//https://github.com/Sarah-C/M5Stack_Core2_ScreenBrightness/blob/main/ScreenBrightnessNoComments/ScreenBrightnessNoComments.ino
void Display::UpdateBrigthness(uint8_t lvl)
{
  //max level = 40?
  int v = lvl * 25 + 2300;
  if (v < 2300) v = 2300;
  if (v > 3300) v = 3300;
  if (v == 2300) {
    M5.Axp.SetDCDC3(false);
    return;
  } else {
    M5.Axp.SetDCDC3(true);
  }
  M5.Axp.SetDCVoltage(2, v);
}

void Display::DrawVoltageImage()
{
  GoVoltage.header.cf = LV_IMG_CF_TRUE_COLOR;
  GoVoltage.header.always_zero = 0;
  GoVoltage.header.reserved = 0;
  GoVoltage.header.w = 47;
  GoVoltage.header.h = 63;
  GoVoltage.data_size = 2961* LV_COLOR_SIZE / 8;// * LV_COLOR_SIZE / 8;
  GoVoltage.data = GoVoltage_map;

 //LV_IMG_DECLARE(GoVoltage);  
  goImg_obj = lv_img_create(lv_scr_act());
  lv_obj_set_pos(goImg_obj, 155,0);
  lv_img_set_src(goImg_obj, &GoVoltage);  
}


void Display::UpdateVoltage(VoltageFreeState state)
{
  if(state != curVoltageState){
    if(state == Go)
    {
      //remove img
      free(goImg_obj);
    }
    else if(state == NoGo)
    {
      //show img
      DrawVoltageImage();      
    }
    curVoltageState = state;
    //lv_task_handler(); //let the GUI do its work 
  }else{
    //no change so do nothing
  }

}*/