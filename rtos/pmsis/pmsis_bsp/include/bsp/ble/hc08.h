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

#ifndef __PI_BSP_BLE_HC08_H__
#define __PI_BSP_BLE_HC08_H__

#include "bsp/ble.h"

/**
 * @addtogroup BLE
 * @{
 */


typedef struct
{
    struct pi_device uart_device; /*!< UART interface used to communicate with BLE module.  */
    char *byte;                   /*!< Byte used to receive response. */
    char *buffer;                 /*!< Buffer used to receive response. */
    uint16_t buffer_len;
} hc08_t;

/**
 * @defgroup HC08 HC08
 *
 * The hc08 driver provides support for for data transfer using a BLE module,
 * here a NINA B112 BLE module.
 * This module is interfaced on GAPPOC through UART.
 *
 * @addtogroup HC08
 * @{
 */

#define PI_AT_RESP_ARRAY_LENGTH ( 256 ) /*!< RESP array length. */

struct hc08_at_cmd{
    int name;
    char* at_cmd;
};

/**
 * \struct pi_hc08_conf
 *
 * \brief HC08 configuration structure.
 *
 * This structure holds BLE configuration(interface used, baudrate,...).
 */
struct pi_hc08_conf
{
    uint8_t uart_itf;           /*!< UART interface used to connect BLE device. */
    //pi_device_api_t *api;       /*!< HC08 API binding to BLE API. */
    char local_name[30];        /*!< BLE device name(visible by others). */
    uint32_t baudrate;        /*!< UART baudrate. */
    uint8_t parity_bits;
    uint8_t stop_bits;
};

struct pi_hc08_id
{
    char manufacturer_id[20];
    char model_id[20];
    char sw_ver_id[20];
    char serial_num[20];
};

typedef enum
{
    PI__HC08_MODIFY_END_OPERATOR  = 0,
} pi_ble_ioctl_cmd_e;


/**
 * \brief Initialize HC08 configuration structure.
 *
 * \param device         Pointer to the BLE device structure.
 * \param conf           Pointer to HC08 configuration structure.
 */
void pi_ble_hc08_conf_init(struct pi_device *device, struct pi_hc08_conf *conf);


/*
* the end of operator for hc22 get()
*/
struct hc08_end_operator{
    char first_operator;
    char second_operator;
};

/******************************************
 *  hc-08 wifi AT cmd
 *  
 ******************************************/
typedef enum 
{
    HC08_AT_TEST_OK                 =0,
    HC08_AT_QUERY_MODULE_PARAMS,

    HC08_AT_RECOVER_DEFAULT,
    HC08_AT_MODULE_RESET,

    HC08_AT_QUERY_VERSION,

    HC08_AT_SET_MASTER,
    HC08_AT_SET_SLAVE,
    HC08_AT_QUERY_ROLE,

    HC08_AT_SET_NAME,
    HC08_AT_QUERY_NAME,

    HC08_AT_SET_MAC,
    HC08_AT_QUERY_MAC,

    HC08_AT_SET_RFPM_0,
    HC08_AT_SET_RFPM_1,
    HC08_AT_SET_RFPM_2,
    HC08_AT_SET_RFPM_3,
    HC08_AT_QUERY_RFPM,

    HC08_AT_SET_BAUD_RATE_9600,
    HC08_AT_QUERY_BAUD_RATE,

    HC08_AT_SET_CONNECT_STATUS,
    HC08_AT_QUERY_CONNECT_STATUS,

    HC08_AT_UPDATE_BROADCAST_DATA,

    HC08_AT_SET_POWER_MODE,
    HC08_AT_QUERY_POWER_MODE,

    HC08_AT_SET_CONNECT_INTERVAL,
    HC08_AT_QUERY_CONNECT_INTERVAL,

    HC08_AT_SET_CONNECT_TIME_OUT,
    HC08_AT_QUERY_CONNECT_TIME_OUT,

    HC08_AT_MASTER_CLEAR_RECORD,

    HC08_AT_SET_LED_ON,
    HC08_AT_SET_LED_OFF,
    HC08_AT_QUERY_LED_STATUS,

    HC08_AT_SEARCH_UUID,
    HC08_AT_QUERY_CONNECT_UUID,

    HC08_AT_SET_UUID,
    HC08_AT_QUERY_UUID,

    HC08_AT_SET_UNVERNISHED_UUID,
    HC08_AT_QUERY_UNVERNISHED_UUID,

    HC08_AT_SET_AUTO_SEELP_TIME,
    HC08_AT_QUERY_AUTO_SEELP_TIME,


    HC08_AT_END
}pi_ble_name_cmd_e;

struct hc08_at_cmd hc08_cmds[HC08_AT_END];

/**
 * @} addtogroup HC08
 */

/**
 * @} addtogroup BLE
 */

#endif  /* __PI_BSP_BLE_HC08_H__ */
