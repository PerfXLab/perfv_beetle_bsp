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

#ifndef __PI_BSP_WIFI_HC22_H__
#define __PI_BSP_WIFI_HC22_H__

#include "bsp/wifi.h"

struct hc_at_cmd{
    int name;
    char* at_cmd;
};

typedef struct
{
    struct pi_device uart_device; /*!< UART interface used to communicate with WIFI module.  */
    char *byte;                   /*!< Byte used to receive response. */
    char *buffer;                 /*!< Buffer used to receive response. */
    uint16_t buffer_len;
} hc22_t;

#define PI_AT_RESP_ARRAY_LENGTH ( 256 ) /*!< RESP array length. */

/**
 * \struct pi_hc22_conf
 *
 * \brief HC22 configuration structure.
 *
 * This structure holds WIFI configuration(interface used, baudrate,...).
 */
struct pi_hc22_conf
{
    uint8_t uart_itf;           /*!< UART interface used to connect WIFI device. */
    //pi_device_api_t *api;       /*!< HC22 API binding to WIFI API. */
    char local_name[30];        /*!< WIFI device name(visible by others). */
    uint32_t baudrate;        /*!< UART baudrate. */
    uint8_t parity_bits;
    uint8_t stop_bits;
};

struct pi_hc22_id
{
    char manufacturer_id[20];
    char model_id[20];
    char sw_ver_id[20];
    char serial_num[20];
};

typedef enum
{
    PI__HC22_MODIFY_END_OPERATOR = 0,
} pi_wifi_ioctl_cmd_e;


/**
 * \brief Initialize HC22 configuration structure.
 *
 * \param device         Pointer to the WIFI device structure.
 * \param conf           Pointer to HC22 configuration structure.
 */
void pi_wifi_hc22_conf_init(struct pi_device *device, struct pi_hc22_conf *conf);


/*
* the end of operator for hc22 get()
*/
struct end_operator{
    char first_operator;
    char second_operator;
};


/******************************************
 *  hc-22 wifi AT cmd
 *  
 ******************************************/
typedef enum 
{
    HC22_AT_ENTRY_AT_MODE                 =0,
    HC22_AT_EXIT_AT_MODE,

    HC22_AT_G_UART_INFO,
    HC22_AT_S_UART_INFO,

    HC22_AT_PRINT_AT_CMD_LIST,
    HC22_AT_CMD_ECHO,
    HC22_AT_QUERY_VERSION,
    HC22_AT_MODULE_RESET,
    HC22_AT_RECOVER_DEFAULT,
    HC22_AT_QUERY_MAC,
    HC22_AT_QUERY_MODULE_ID,
    HC22_AT_SEARCH_AP,
    HC22_AT_UDP_BROADCAST,

    HC22_AT_QUERY_WIFI_MODEL,
    HC22_AT_SET_WIFI_MODEL,
    HC22_AT_SET_WIFI_MODEL_STA,

    HC22_AT_QUERY_CORRELATION_AP,
    HC22_AT_SET_CORRELATION_AP,

    HC22_AT_QUERY_STA_PARAMS,
    HC22_AT_SET_STA_PARAMS,

    HC22_AT_QUERY_WIFI_CONNECTION_STATUS,

    HC22_AT_QUERY_AP_CONFIG_PARAMS,
    HC22_AT_SET_AP_CONFIG_PARAMS,

    HC22_AT_AP_QUERY_IP,
    HC22_AT_AP_SET_IP,

    HC22_AT_G_SOCKET_STATUS,
    HC22_AT_S_SOCKET_STATUS,

    HC22_AT_G_SOCKET_INFO,
    HC22_AT_S_SOCKET_INFO_TCPC,
    HC22_AT_S_SOCKET_INFO_TCPS,

    HC22_AT_QUERY_SOCKET_TCP_STATUS,

    HC22_AT_G_TCP_AUTO_CONNECT_STATUS,
    HC22_AT_S_TCP_AUTO_CONNECT_STATUS,

    HC22_AT_G_HTTP_URL,
    HC22_AT_S_HTTP_URL,

    HC22_AT_G_HTTP_REQUEST_MODE,
    HC22_AT_S_HTTP_REQUEST_MODE,

    HC22_AT_G_HTTP_REQUEST_HEAD,
    HC22_AT_S_HTTP_REQUEST_HEAD,

    HC22_AT_G_AT_MODE,
    HC22_AT_S_AT_MODE,

    HC22_AT_END
}pi_wifi_name_cmd_e;

struct hc_at_cmd hc22_cmds[HC22_AT_END];

#endif  /* __PI_BSP_WIFI_HC22_H__ */
