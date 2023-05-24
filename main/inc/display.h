#ifndef DISPLAY_H_
#define DISPLAY_H_

#include "sdkconfig.h"

#include "libs/disp_spi.h"
#include "libs/ft6x36.h"
#include "libs/helpers.h"
#include "libs/ili9341.h"
#include "libs/power.h"

#include "utils.h"
#include "config.h"
#include "lvgl.h"
#include "lvgl/lvgl.h"

#define DISPLAY_OPTION_2


#define BRIGHTNESS_HIGH 12
#define BRIGHTNESS_OFF 0
#define BRIGHTNESS_LOW 9

void InitDisplay();

void Btnmatrix();
void ClearBtnmatrix();

void SetMenu();
void ClearMenu();

void DrawInitStatus(); //call once
void UpdateInitStatus();

void DrawRectangle();   //call once
void UpdateRectangle();

void DrawTemp();        // call once
void UpdateTempText();

void DrawBattery();     //call once
void UpdateBatteryPercentageText();

void DrawWifi();        //call once
void UpdateConnection();

void DrawVoltageFree(); //call once
void UpdateVoltageFree(); 

void UpdateDisplayUtils();
void lv_tick_task(void *arg);

//void UpdateBrigthness(uint8_t lvl);
//static function
#endif