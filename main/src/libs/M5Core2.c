#include "../inc/libs/M5Core2.h"
#include "../inc/libs/lvgl_spi_conf.h"
void m5core2_init() {
  // i2c and spi setup
  lv_init();

  lvgl_i2c_driver_init(I2C_NUM_0, CONFIG_I2C_SDA, CONFIG_I2C_SCL,
                       TOUCH_I2C_SPEED_HZ);
 
  #ifdef CORE2
    begin();
  #endif
  lvgl_spi_driver_init(HSPI_HOST, DISP_SPI_MISO, DISP_SPI_MOSI, DISP_SPI_CLK,
                       SPI_BUS_MAX_TRANSFER_SZ, 1, -1, -1);

  disp_spi_add_device(HSPI_HOST);
  ili9341_init();
  
}
