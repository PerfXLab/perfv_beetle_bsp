/*
 * Copyright (C) 2019 Perfxlab Technologies
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "pmsis.h"

#ifndef __BSP__DISPLAY__ST7735S_H__
#define __BSP__DISPLAY__ST7735S_H__

#include "bsp/display.h"

/**
 * @addtogroup Display
 * @{
 */

/**
 * @defgroup st7735s st7735s
 *
 * Display st7735s.
 * This module is interfaced through SPIM1.
 */

/**
 * @addtogroup st7735s
 * @{
 */

/* @brief Struct holding st7735s display config. */
struct pi_st7735s_conf
{
  struct pi_display_conf display; /*!< Display conf. */
  int spi_itf;                    /*!< Interface used to communicate with SPI.  */
  int spi_cs;                     /*!< Chip select on the interface.  */
  int gpio;                       /*!< GPIO pin.  */
  int gpio_rst;                       /*!< GPIO pin for reset.   zzj add */
  char skip_pads_config;          /*!< Buffer used to send  */
};

/* @brief Display orientation. */
typedef enum
{
  PI_ST_ORIENTATION_0 = 0,     /*!< Orientation at 0 degrees. */
  PI_ST_ORIENTATION_90 = 1,    /*!< Orientation at 90 degrees. */
  PI_ST_ORIENTATION_180 = 2,   /*!< Orientation at 180 degrees. */
  PI_ST_ORIENTATION_270 = 3,   /*!< Orientation at 270 degrees. */
} pi_st_orientation_e;

/* @brief st7735s ioctl commands. */
typedef enum
{
  PI_ST_IOCTL_ORIENTATION = PI_DISPLAY_IOCTL_CUSTOM /*!< Display orientation
    command. The argument to this command must be a value of type
    pi_st_orientation_e. */
} pi_st_ioctl_cmd_e;

/**
 * @brief Initialize a camera configuration with default values.
 *
 * The structure containing the configuration must be kept alive until
 * the camera device is opened.
 * It can only be called from fabric-controller side.
 *
 * @param conf           Pointer to the device configuration.
 */
void pi_st7735s_conf_init(struct pi_st7735s_conf *conf);


void writeFillRect(struct pi_device *device, unsigned short x, unsigned short y, unsigned short w, unsigned short h, unsigned short color);
void setTextWrap(struct pi_device *device,signed short w);
void setCursor(struct pi_device *device,signed short x, signed short y);
void setTextColor(struct pi_device *device,uint16_t c);
static void __st_writePixelAtPos(struct pi_device *device,int16_t x, int16_t y, uint16_t color);
static void drawChar(struct pi_device *device,int16_t x, int16_t y, unsigned char c,uint16_t color, uint16_t bg, uint8_t size);
static void writeChar(struct pi_device *device,uint8_t c);
void writeText(struct pi_device *device,char* str,int fontsize);
/**
 * @}
 */

/**
 * @}
 */

#endif  /* __BSP__DISPLAY__ST7735S_H__ */

