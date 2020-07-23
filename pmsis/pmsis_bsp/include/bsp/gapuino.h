/*
 * Copyright (C) 2019 GreenWaves Technologies
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

#ifndef __BSP__GAPUINO_H__
#define __BSP__GAPUINO_H__

#define CONFIG_OV7725
//#define CONFIG_HIMAX

//#define CONFIG_NINA_W10
#define CONFIG_HC08

#define CONFIG_HC22

#define CONFIG_Z350
//#define CONFIG_YT280
//#define CONFIG_ST7735S
//#define CONFIG_ILI9341

//#define CONFIG_HYPERFLASH
//#define CONFIG_HYPERRAM
#define CONFIG_SPIRAM
#define CONFIG_SPIFLASH

#if defined(CONFIG_HIMAX)
#define CONFIG_HIMAX_CPI_ITF 0
#define CONFIG_HIMAX_I2C_ITF 1
#elif defined(CONFIG_OV7725)
#define CONFIG_OV7725_CPI_ITF 0
#define CONFIG_OV7725_I2C_ITF 1
#endif


#define CONFIG_NINA_W10_SPI_ITF            1
#define CONFIG_NINA_W10_SPI_CS             0
#define CONFIG_NINA_GPIO_NINA_ACK          18
#define CONFIG_NINA_GPIO_NINA_ACK_PAD      PI_PAD_32_A13_TIMER0_CH1
#define CONFIG_NINA_GPIO_NINA_ACK_PAD_FUNC PI_PAD_32_A13_GPIO_A18_FUNC1
#define CONFIG_NINA_GPIO_NINA_NOTIF          3
#define CONFIG_NINA_GPIO_NINA_NOTIF_PAD      PI_PAD_15_B1_RF_PACTRL3
#define CONFIG_NINA_GPIO_NINA_NOTIF_PAD_FUNC PI_PAD_15_B1_GPIO_A3_FUNC1

#if defined(CONFIG_ILI9341)

#define CONFIG_ILI9341_SPI_ITF       1
#define CONFIG_ILI9341_SPI_CS        0
//#define CONFIG_ILI9341_GPIO          19
//#define CONFIG_ILI9341_GPIO_PAD      PI_PAD_33_B12_TIMER0_CH2
//#define CONFIG_ILI9341_GPIO_PAD_FUNC PI_PAD_33_B12_GPIO_A19_FUNC1

#define CONFIG_ILI9341_GPIO          0
#define CONFIG_ILI9341_GPIO_PAD      PI_PAD_12_A3_RF_PACTRL0
#define CONFIG_ILI9341_GPIO_PAD_FUNC PI_PAD_12_A3_GPIO_A0_FUNC1

#define CONFIG_ILI9341_GPIO_RST          1
#define CONFIG_ILI9341_GPIO_RST_PAD      PI_PAD_13_B2_RF_PACTRL1
#define CONFIG_ILI9341_GPIO_RST_PAD_FUNC PI_PAD_13_B2_GPIO_A1_FUNC1

#elif defined(CONFIG_ST7735S)

#define CONFIG_ST7735S_SPI_ITF       1
#define CONFIG_ST7735S_SPI_CS        0
#define CONFIG_ST7735S_GPIO          0
#define CONFIG_ST7735S_GPIO_PAD      PI_PAD_12_A3_RF_PACTRL0
#define CONFIG_ST7735S_GPIO_PAD_FUNC PI_PAD_12_A3_GPIO_A0_FUNC1

#define CONFIG_ST7735S_GPIO_RST          1
#define CONFIG_ST7735S_GPIO_RST_PAD      PI_PAD_13_B2_RF_PACTRL1
#define CONFIG_ST7735S_GPIO_RST_PAD_FUNC PI_PAD_13_B2_GPIO_A1_FUNC1

#elif defined(CONFIG_YT280)

#define CONFIG_YT280_SPI_ITF       1
#define CONFIG_YT280_SPI_CS        0
#define CONFIG_YT280_GPIO          0
#define CONFIG_YT280_GPIO_PAD      PI_PAD_12_A3_RF_PACTRL0
#define CONFIG_YT280_GPIO_PAD_FUNC PI_PAD_12_A3_GPIO_A0_FUNC1

#define CONFIG_YT280_GPIO_RST          1
#define CONFIG_YT280_GPIO_RST_PAD      PI_PAD_13_B2_RF_PACTRL1
#define CONFIG_YT280_GPIO_RST_PAD_FUNC PI_PAD_13_B2_GPIO_A1_FUNC1

#elif defined(CONFIG_Z350)

#define CONFIG_Z350_SPI_ITF       1
#define CONFIG_Z350_SPI_CS        0
#define CONFIG_Z350_GPIO          0
#define CONFIG_Z350_GPIO_PAD      PI_PAD_12_A3_RF_PACTRL0
#define CONFIG_Z350_GPIO_PAD_FUNC PI_PAD_12_A3_GPIO_A0_FUNC1

#define CONFIG_Z350_GPIO_RST          1
#define CONFIG_Z350_GPIO_RST_PAD      PI_PAD_13_B2_RF_PACTRL1
#define CONFIG_Z350_GPIO_RST_PAD_FUNC PI_PAD_13_B2_GPIO_A1_FUNC1

#endif


#if defined(CONFIG_OV7725)
#define CONFIG_OV7725_GPIO_RST          3
#define CONFIG_OV7725_GPIO_RST_PAD      PI_PAD_15_B1_RF_PACTRL3
#define CONFIG_OV7725_GPIO_RST_PAD_FUNC PI_PAD_15_B1_GPIO_A3_FUNC1

#define CONFIG_OV7725_GPIO_PWDN          2
#define CONFIG_OV7725_GPIO_PWDN_PAD      PI_PAD_14_A2_RF_PACTRL2
#define CONFIG_OV7725_GPIO_PWDN_PAD_FUNC PI_PAD_14_A2_GPIO_A2_FUNC1
#endif

#if defined(CONFIG_HYPERFLASH)
#define CONFIG_HYPERFLASH_HYPER_ITF 0
#define CONFIG_HYPERFLASH_HYPER_CS  1
#endif

#if defined(CONFIG_HYPERRAM)
#define CONFIG_HYPERRAM_HYPER_ITF 0
#define CONFIG_HYPERRAM_HYPER_CS  0
#define CONFIG_HYPERRAM_START     0
#define CONFIG_HYPERRAM_SIZE     (8<<20)
#endif

#define CONFIG_SPIRAM_SPI_ITF   0
#define CONFIG_SPIRAM_SPI_CS    1
#define CONFIG_SPIRAM_START     0
#define CONFIG_SPIRAM_SIZE     (8<<20)

#define CONFIG_SPIFLASH_SPI_ITF     0
#define CONFIG_SPIFLASH_SPI_CS      0
#define CONFIG_SPIFLASH_START       0
#define CONFIG_SPIFLASH_SIZE        (1<<24)
#define CONFIG_SPIFLASH_SECTOR_SIZE (1<<12)

#if defined(CHIP_VERSION)
#if (CHIP_VERSION == 2)
#define GPIO_USER_LED                        ( PI_GPIO_A3_PAD_15_B1 )
#endif  /* CHIP_VERSION */
#endif  /* CHIP_VERSION */

#define PI_BSP_PROFILE_GAPUINO_0 0   // Default profile
#define PI_BSP_PROFILE_GAPUINO_1 1   // I2S0 and I2S1 with different clock

#define CONFIG_HC08_UART_ID       ( 0 )

#define CONFIG_HC22_UART_ID       ( 0 )

#endif
