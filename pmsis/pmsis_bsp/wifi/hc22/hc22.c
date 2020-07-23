/*
 * Copyright (C) 2019 Perfxlab Technologies
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicawifi law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "bsp/bsp.h"
#include "bsp/wifi/hc22.h"

#define PI_HC22_UART_BAUDRATE   ( 9600 ) /*!< Baudrate used by HC22 module(default value). */
#define PI_HC22_DATA_BITS       ( 8 ) /*!< Data bits used by HC22 module(default value). */

/*******************************************************************************
 * type define
 ******************************************************************************/
typedef enum
{
    PI_AT_RESP_NOT_STARTED,       /*!< Response not received yet from WIFI module.  */
    PI_AT_RESP_IN_PROGRESS,       /*!< Response transmission has started. */
    PI_AT_RESP_DONE               /*!< Response transmission received, with end of resp. */
} at_resp_state_e;

typedef enum
{
    CMD_RES_OK    = 0,             /*!< OK reponse. */
    CMD_RES_ERR   = -1,            /*!< ERROR response. */
    CMD_RES_UNSOL = -2,            /*!< UNSOLICITED_RESPONSE response. */
    CMD_RES_NA    = -3             /*!< Non authorized response. */
} cmd_res_e;

/**
 * 
 */
typedef struct
{
    hc22_t *hc22;
    struct pi_task *task;
} cb_args_t;

struct hc_at_cmd hc22_cmds[HC22_AT_END] = 
{
    {HC22_AT_ENTRY_AT_MODE,              "+++" },
    {HC22_AT_EXIT_AT_MODE,              "AT+ENTM" },

    {HC22_AT_G_UART_INFO,              "AT+UART" },
    {HC22_AT_S_UART_INFO,              "UART=9600,1,NONE" },

    {HC22_AT_PRINT_AT_CMD_LIST,         "AT+H" },
    {HC22_AT_CMD_ECHO,                  "AT+E" },
    {HC22_AT_QUERY_VERSION,              "AT+E" },
    {HC22_AT_MODULE_RESET,              "AT+RESET" },
    {HC22_AT_RECOVER_DEFAULT,              "AT+DEFAULT" },
    {HC22_AT_QUERY_MAC,              "AT+MAC" },
    {HC22_AT_QUERY_MODULE_ID,              "AT+MID" },
    {HC22_AT_SEARCH_AP,              "AT+WSCAN" },
    {HC22_AT_UDP_BROADCAST,              "AT+SEARCH" },

    {HC22_AT_QUERY_WIFI_MODEL,              "AT+WMODE" },
    {HC22_AT_SET_WIFI_MODEL,              "AT+WMODE=AP+STA" },          //must MODE =1;
    {HC22_AT_SET_WIFI_MODEL_STA,              "AT+WMODE=STA" },          //must MODE =1;

    {HC22_AT_QUERY_CORRELATION_AP,              "AT+WSTA" },
    {HC22_AT_SET_CORRELATION_AP,              "AT+WSTA=PerfXLab,perfxlab2017" },        //"AT+WSTA=CHKCPT,cuihukechuangpingtai"

    {HC22_AT_QUERY_STA_PARAMS,              "AT+WANN" },
    {HC22_AT_SET_STA_PARAMS,              "AT+WANN=DHCP" },         //AT+WANN=DHCP

    {HC22_AT_QUERY_WIFI_CONNECTION_STATUS,              "AT+WSLK" },

    {HC22_AT_QUERY_AP_CONFIG_PARAMS,              "AT+WAP" },
    {HC22_AT_SET_AP_CONFIG_PARAMS,              "AT+WAP=Perf-V Beetle,1234567890" },

    {HC22_AT_AP_QUERY_IP,              "AT+LANN" },
    {HC22_AT_AP_SET_IP,              "AT+LANN=192.168.2.1" },

    {HC22_AT_G_SOCKET_STATUS,              "AT+SOCKSTATUS" },
    {HC22_AT_S_SOCKET_STATUS,              "AT+SOCKSTATUS=ON" },

    {HC22_AT_G_SOCKET_INFO,              "AT+SOCK" },
    {HC22_AT_S_SOCKET_INFO_TCPC,              "AT+SOCK=TCPC,10.30.101.202,8080" },       //AT+SOCK=TCPC,10.30.101.202,8080
    {HC22_AT_S_SOCKET_INFO_TCPS,              "AT+SOCK=TCPS,192.168.1.1,8080" }, 

    {HC22_AT_QUERY_SOCKET_TCP_STATUS,              "AT+SOCKLK" },

    {HC22_AT_G_TCP_AUTO_CONNECT_STATUS,              "AT+SOCKAUTO" },
    {HC22_AT_S_TCP_AUTO_CONNECT_STATUS,              "AT+SOCKAUTO=ON" },

    {HC22_AT_G_HTTP_URL,              "AT+HTTPCURL" },
    {HC22_AT_S_HTTP_URL,              "AT+HTTPCURL=PHP?" },

    {HC22_AT_G_HTTP_REQUEST_MODE,              "AT+HTTPCMODE" },
    {HC22_AT_S_HTTP_REQUEST_MODE,              "AT+HTTPCMODE =GET" },

    {HC22_AT_G_HTTP_REQUEST_HEAD,              "AT+HTTPCHEAD" },
    {HC22_AT_S_HTTP_REQUEST_HEAD,              "AT+HTTPCHEAD=Accept:text/html;Accept-Language:zh-CN;User-Agent:M    \
ozilla/5.0;Connection:close" },

    {HC22_AT_G_AT_MODE,              "AT+MODE" },
    {HC22_AT_S_AT_MODE,              "AT+MODE=1" },
    
};

/*******************************************************************************
 * Driver data
 ******************************************************************************/

static cb_args_t g_param = {0};

static volatile at_resp_state_e g_at_resp_state;

static pi_device_api_t g_hc22_api = {0};
static pi_wifi_api_t g_hc22_wifi_api = {0};

static uint32_t AT_index = 0;

static struct end_operator end_op = {.first_operator = '\r',.second_operator = '\n'};

/*******************************************************************************
 * Function declaration
 ******************************************************************************/

static int __pi_hc22_open(struct pi_device *device);

static int __pi_hc22_close(struct pi_device *device);

static ssize_t __pi_hc22_data_get(struct pi_device *device, uint32_t ext_addr,
                                       void *buffer, uint32_t size, pi_task_t *task);
                                
static ssize_t __pi_hc22_data_send(struct pi_device *device, uint32_t ext_addr,
                                    const void *buffer, uint32_t size, pi_task_t *task);

/*******************************************************************************
 * Internal functions
 ******************************************************************************/

static void __pi_hc22_at_cmd_send(hc22_t *hc22,const char *cmd)
{
    uint32_t cmd_length = strlen(cmd);
    char *cmd_string = (char *) pi_l2_malloc(sizeof(char) * cmd_length);
    if (cmd_string == NULL)
    {
        return;
    }
    strcpy((char*) cmd_string, (char*) cmd);
    pi_uart_write(&(hc22->uart_device), cmd_string, strlen(cmd_string));
}

/*
receive at data
*/
static void __pi_hc22_at_data_received(void *arg)
{
    struct pi_task *param_task = g_param.task;

    g_at_resp_state = PI_AT_RESP_IN_PROGRESS;
    g_param.hc22->buffer[AT_index++] = *(g_param.hc22->byte);
    g_param.hc22->buffer[AT_index] = '\0';                     //EOF

    pi_task_callback(g_param.task, __pi_hc22_at_data_received, &g_param);
    pi_uart_read_async(&(g_param.hc22->uart_device),
                        (void *) g_param.hc22->byte,
                        sizeof(uint8_t),
                        g_param.task);

}

static void timer_handler()
{
    g_at_resp_state = PI_AT_RESP_DONE;
    printf("Timer IRQ: timer is over, val=%d\n",  pi_timer_value_read(FC_TIMER_1));
}

HANDLER_WRAPPER_LIGHT(timer_handler);

/*
send AT cmd and received  
*/
static int32_t __pi_hc22_at_cmd(struct pi_device *device, const char *cmd,
                                     char *resp, uint32_t size)
{
    hc22_t *hc22 = (hc22_t *) device->data;
    g_at_resp_state = PI_AT_RESP_NOT_STARTED;
    pi_task_t rx_cb = {0};

    /* start timer */
    pi_timer_start(FC_TIMER_1);

    /* clean up the last legacy */
    pi_uart_ioctl(&(hc22->uart_device), PI_UART_IOCTL_ABORT_RX, NULL );
    pi_uart_ioctl(&(hc22->uart_device), PI_UART_IOCTL_ENABLE_RX, NULL );

    g_param.hc22 = hc22;
    g_param.task = &rx_cb;
    pi_task_callback(&rx_cb, __pi_hc22_at_data_received, &g_param);
    pi_uart_read_async(&(hc22->uart_device), (void *) hc22->byte, sizeof(uint8_t), &rx_cb);
    __pi_hc22_at_cmd_send(hc22, cmd);

    while (g_at_resp_state != PI_AT_RESP_DONE)
    {
        pi_yield();
    }
    
    /* stop timer */
    pi_timer_stop(FC_TIMER_1);

    /* the count for receive data  to zero */
    AT_index = 0;

    //printf("Got response: %s\n", hc22->buffer);

    if (size)
    {
        strcpy((void *) resp, (void *) hc22->buffer);
    }
    return 0;
}

/*
    send data
*/
static ssize_t __pi_hc22_data_send(struct pi_device *device, uint32_t ext_addr,
                                    const void *buffer, uint32_t size, pi_task_t *task)
{
    hc22_t *hc22 = (hc22_t *) device->data;
    pi_uart_write_async(&(hc22->uart_device), (void *) buffer, size, task);
    return 0;
}

/*
   we send data which the data manually add '\r'and'\n'.
   so ,we should process the '\r'and'\n'.  
   note : 
        '\r'and'\n' are hex decimal. not "\r" and "\n"
*/
static void __pi_hc22_data_received(void *arg)
{
    struct pi_task *param_task = g_param.task;
    static uint32_t index = 0;              //should move to outside.
    static unsigned char prev_byte = 0;

    if ((g_at_resp_state == PI_AT_RESP_IN_PROGRESS) &&
        (prev_byte == end_op.first_operator) && (*(g_param.hc22->byte) == end_op.second_operator))
    {
        param_task->id = PI_TASK_NONE_ID;
        g_param.hc22->buffer[--index] = '\0';
        g_param.hc22->buffer_len = index;
        
        index = 0;

        pi_uart_read_async(&(g_param.hc22->uart_device),
                            (void *) g_param.hc22->byte,
                            sizeof(uint8_t),
                            g_param.task);

        pi_uart_read_async(&(g_param.hc22->uart_device),
                            (void *) g_param.hc22->byte,
                            sizeof(uint8_t),
                            g_param.task); 

        g_at_resp_state = PI_AT_RESP_DONE;

    }
    else
    {
        g_at_resp_state = PI_AT_RESP_IN_PROGRESS;
        prev_byte = *(g_param.hc22->byte);
        g_param.hc22->buffer[index++] = *(g_param.hc22->byte);

        pi_task_callback(g_param.task, __pi_hc22_data_received, &g_param);
            pi_uart_read_async(&(g_param.hc22->uart_device),
                            (void *) g_param.hc22->byte,
                            sizeof(uint8_t),
                            g_param.task);
    }
    
}

/*
device->api->read(device, 0, (void *) buffer, size, task);
*/
static ssize_t __pi_hc22_data_get(struct pi_device *device, uint32_t ext_addr,
                                       void *buffer, uint32_t size, pi_task_t *task)
{
    hc22_t *hc22 = (hc22_t *) device->data;
    g_at_resp_state = PI_AT_RESP_NOT_STARTED;
    g_param.hc22 = hc22;
    g_param.task = task;

    /* clean up the last legacy */
    pi_uart_ioctl(&(hc22->uart_device), PI_UART_IOCTL_ABORT_RX, NULL );
    pi_uart_ioctl(&(hc22->uart_device), PI_UART_IOCTL_ENABLE_RX, NULL );

    pi_task_callback(task, __pi_hc22_data_received, &g_param);
    pi_uart_read_async(&(hc22->uart_device), (void *) hc22->byte, sizeof(uint8_t), task);

    while (g_at_resp_state != PI_AT_RESP_DONE)
    {
        pi_yield();
    }

    memcpy(buffer,g_param.hc22->buffer,g_param.hc22->buffer_len);

    printf("--code:(%s)\n",g_param.hc22->buffer);
    
    memset(g_param.hc22->buffer,0,PI_AT_RESP_ARRAY_LENGTH);

    return 0;
}

/*******************************************************************************
 * API implementation
 ******************************************************************************/

static int __pi_hc22_open(struct pi_device *device)
{

    struct pi_hc22_conf *conf = (struct pi_hc22_conf *) device->config;
    /* hc22 device. */
    hc22_t *hc22 = (hc22_t *) pi_l2_malloc(sizeof(hc22_t));
    if (hc22 == NULL)
    {
        return -1;
    }
    hc22->byte = (char *) pi_l2_malloc(sizeof(char) * 1);
    if (hc22->byte == NULL)
    {
        pi_l2_free(hc22, sizeof(hc22_t));
        return -2;
    }
    hc22->buffer = (char *) pi_l2_malloc(sizeof(char) * (uint32_t) PI_AT_RESP_ARRAY_LENGTH);
    if (hc22->byte == NULL)
    {
        pi_l2_free(hc22->byte, sizeof(sizeof(char) * 1));
        pi_l2_free(hc22, sizeof(hc22_t));
        return -3;
    }
    device->data = (void *) hc22;
    
    if(bsp_hc22_open()) 
    {
        pi_l2_free(hc22->byte, sizeof(sizeof(char) * 1));
        pi_l2_free(hc22, sizeof(hc22_t));
        printf("[hc22] Error during driver opening: failed to init bsp for hc22\n");
        return -3;
    }

    /* Init and open UART device. */
    struct pi_uart_conf uart_conf = {0};
    pi_uart_conf_init(&uart_conf);
    uart_conf.uart_id = conf->uart_itf;
    uart_conf.baudrate_bps = (uint32_t) PI_HC22_UART_BAUDRATE;
    uart_conf.enable_rx = 1;
    uart_conf.enable_tx = 1;
    pi_open_from_conf(&(hc22->uart_device), &uart_conf);
    if (pi_uart_open(&(hc22->uart_device)))
    {
        pi_l2_free(hc22->buffer, sizeof(sizeof(char) * (uint32_t) PI_AT_RESP_ARRAY_LENGTH));
        pi_l2_free(hc22->byte, sizeof(sizeof(char) * 1));
        pi_l2_free(hc22, sizeof(hc22_t));
        return -4;
    }

    /* the FC basic timer */
    uint32_t time_us = 2000000;
    NVIC_SetVector(FC_EVENT_TIMER1, (uint32_t) __handler_wrapper_light_timer_handler);
    NVIC_EnableIRQ(FC_EVENT_TIMER1);
    printf("Timer IRQ set every %dus.\n", time_us);
    pi_timer_irq_set(FC_TIMER_1, time_us, 0);
    pi_timer_stop(FC_TIMER_1);              //when in use ,use pi_timer_start() to turn it on

    return 0;
}

/*
struct end_operator{
    char first_operator;
    char second_operator;
}
*/
static int __pi_hc22_ioctl(struct pi_device *device, uint32_t cmd, void *arg)
{
    hc22_t *hc22 = (hc22_t *) device->data;
    pi_wifi_ioctl_cmd_e func_id = (pi_wifi_ioctl_cmd_e) cmd;
    switch(func_id)
    {
        case PI__HC22_MODIFY_END_OPERATOR:
        {
            end_op.first_operator = ((struct end_operator *)arg)->first_operator;
            end_op.second_operator = ((struct end_operator *)arg)->second_operator;
        }
            break;
        default:
        {
            printf("Error: you must use the cmd (PI__HC22_MODIFY_END_OPERATOR)\n");
            return -1;
        }
    }
    return 0;
}

static int __pi_hc22_close(struct pi_device *device)
{
    hc22_t *hc22 = (hc22_t *) device->data;
    pi_uart_close(&(hc22->uart_device));
    pi_l2_free(hc22->buffer, sizeof(sizeof(char) * (uint32_t) PI_AT_RESP_ARRAY_LENGTH));
    pi_l2_free(hc22->byte, sizeof(sizeof(char) * 1));
    pi_l2_free(hc22, sizeof(hc22_t));
    return 0;
}

void pi_wifi_hc22_conf_init(struct pi_device *device, struct pi_hc22_conf *conf)
{
    bsp_hc22_conf_init(conf);
    /* Bind HC22 API to WIFI API. */
    g_hc22_api.open = &__pi_hc22_open;
    g_hc22_api.close = &__pi_hc22_close;
    g_hc22_api.read = &__pi_hc22_data_get;
    g_hc22_api.write = &__pi_hc22_data_send;
    g_hc22_api.ioctl = &__pi_hc22_ioctl;
    /* WIFI specific API. */
    g_hc22_wifi_api.at_cmd = &__pi_hc22_at_cmd;
    g_hc22_api.specific_api = (void *) &g_hc22_wifi_api;
    char *str = "Perfxlab Perf-V Beetle";
    strcpy(conf->local_name, str);
    conf->baudrate = (uint32_t) PI_HC22_UART_BAUDRATE; /* Default baudrate. */
    conf->parity_bits = 1;           /* No parity. */
    conf->stop_bits = 1;             /* 1 stop bit. */
    device->api = &g_hc22_api;
}
