#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "lvgl.h"
#include "helpers.h"
#include "../config.h"

#define ILI9341_DC CONFIG_LV_DISP_PIN_DC
#define ILI9341_RST CONFIG_LV_DISP_PIN_RST
#define ILI9341_BCKL CONFIG_LV_DISP_PIN_BCKL
#define ILI9341_ENABLE_BACKLIGHT_CONTROL CONFIG_LV_ENABLE_BACKLIGHT_CONTROL
#if CONFIG_LV_BACKLIGHT_ACTIVE_LVL
  #define ILI9341_BCKL_ACTIVE_LVL 1
#else
  #define ILI9341_BCKL_ACTIVE_LVL 0
#endif

#define ILI9341_INVERT_COLORS CONFIG_LV_INVERT_COLORS
void ili9341_init(void);
void ili9341_flush(lv_disp_drv_t *drv, const lv_area_t *area,
                   lv_color_t *color_map);
void ili9341_enable_backlight(bool backlight);
void ili9341_sleep_in(void);
void ili9341_sleep_out(void);

#ifdef __cplusplus
} /* extern "C" */
#endif
