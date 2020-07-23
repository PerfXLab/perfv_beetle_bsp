/*
 * Copyright (C) 2019 perfxlab Technologies
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

#include "bsp/bsp.h"
#include "bsp/ble/hc08.h"

#define PI_HC08_UART_BAUDRATE   ( 9600 ) /*!< Baudrate used by HC08 module(default value). */
#define PI_HC08_DATA_BITS       ( 8 ) /*!< Data bits used by HC08 module(default value). */

/*******************************************************************************
 * type define
 ******************************************************************************/
typedef enum
{
    PI_AT_RESP_NOT_STARTED,       /*!< Response not received yet from BLE module.  */
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

typedef struct
{
    hc08_t *hc08;
    struct pi_task *task;
} cb_args_t;

/*******************************************************************************
 * AT cmd
 ******************************************************************************/
struct hc08_at_cmd hc08_cmds[HC08_AT_END] = 
{
    {HC08_AT_TEST_OK,              "AT" },
    {HC08_AT_QUERY_MODULE_PARAMS,              "AT+RX" },

    {HC08_AT_RECOVER_DEFAULT,              "AT+DEFAULT" },
    {HC08_AT_MODULE_RESET,              "AT+RESET" },

    {HC08_AT_QUERY_VERSION,         "AT+VERSION" },

    {HC08_AT_SET_MASTER,                  "AT+ROLE=M" },
    {HC08_AT_SET_SLAVE,                  "AT+ROLE=S" },
    {HC08_AT_QUERY_ROLE,                  "AT+ROLE=?" },

    {HC08_AT_SET_NAME,                  "AT+NAME=Beetle" },
    {HC08_AT_QUERY_NAME,                  "AT+NAME=?" },

    {HC08_AT_SET_MAC,                  "AT+ADDR=AABBCCDDEEFF" },
    {HC08_AT_QUERY_MAC,                  "AT+ADDR=? " },

    {HC08_AT_SET_RFPM_0,                  "AT+RFPM=0" },        //4dBm（出厂默认值）
    {HC08_AT_SET_RFPM_1,                  "AT+RFPM=1" },        // 0dBm
    {HC08_AT_SET_RFPM_2,                  "AT+RFPM=2" },        //-6dBm
    {HC08_AT_SET_RFPM_3,                  "AT+RFPM=3" },        //-23dBm
    {HC08_AT_QUERY_RFPM,                  "AT+RFPM=?" },        //查看当前射频功率

    {HC08_AT_SET_BAUD_RATE_9600,                  "AT+BAUD=9600,N" },   // N 无校验 NONE ;  E 偶校验 EVEN ;  O 奇校验 ODD;
    {HC08_AT_QUERY_BAUD_RATE,                  "AT+BAUD=?" },

    {HC08_AT_SET_CONNECT_STATUS,                  "AT+CONT=1" },        //0: can be connected ;  1: can`t be connected.
    {HC08_AT_QUERY_CONNECT_STATUS,                  "AT+CONT=?" },

    {HC08_AT_UPDATE_BROADCAST_DATA,                  "AT+AVDA=1234567890AB" },

    {HC08_AT_SET_POWER_MODE,                  "AT+MODE=0" },        //0:全速功耗模式（出厂默认）; 1:一级节能模式。连接前电流由 AT+AINT 的设置决定 ; 2:二级节能模式（睡眠模式）。睡眠时电流 0.4μA。
    {HC08_AT_QUERY_POWER_MODE,                  "AT+MODE=?" },

    {HC08_AT_SET_CONNECT_INTERVAL,                  "AT+CINT=16,32 " },     //设置连接间隔为 20ms~40ms
    {HC08_AT_QUERY_CONNECT_INTERVAL,                  "AT+CINT=? " },

    {HC08_AT_SET_CONNECT_TIME_OUT,                  "AT+CTOUT=100" },
    {HC08_AT_QUERY_CONNECT_TIME_OUT,                  "AT+CTOUT=?" },

    {HC08_AT_MASTER_CLEAR_RECORD,                  "AT+CLEAR" },

    {HC08_AT_SET_LED_ON,                  "AT+LED=1" },
    {HC08_AT_SET_LED_OFF,                  "AT+LED=0" },
    {HC08_AT_QUERY_LED_STATUS,                  "AT+LED=?" },

    {HC08_AT_SEARCH_UUID,                  "AT+LUUID=1234" },
    {HC08_AT_QUERY_CONNECT_UUID,                  "AT+LUUID=?" },

    {HC08_AT_SET_UUID,                  "AT+SUUID=1234" },
    {HC08_AT_QUERY_UUID,                  "AT+SUUID=? " },

    {HC08_AT_SET_UNVERNISHED_UUID,                  "AT+TUUID=1234" },
    {HC08_AT_QUERY_UNVERNISHED_UUID,                  "AT+TUUID=?" },

    {HC08_AT_SET_AUTO_SEELP_TIME,                  "AT+AUST=60" },      //设置自动进入睡眠时间为 60S
    {HC08_AT_QUERY_AUTO_SEELP_TIME,                  "AT+AUST=?" },

};


/*******************************************************************************
 * Driver data
 ******************************************************************************/

static cb_args_t g_param = {0};

static volatile at_resp_state_e g_at_resp_state;

static pi_device_api_t g_hc08_api = {0};
static pi_ble_api_t g_hc08_ble_api = {0};

static uint32_t AT_index = 0;

static struct hc08_end_operator end_op = {.first_operator = '\r',.second_operator = '\n'};

/*******************************************************************************
 * Function declaration
 ******************************************************************************/

static int __pi_hc08_open(struct pi_device *device);

static int __pi_hc08_close(struct pi_device *device);

static ssize_t __pi_hc08_data_get(struct pi_device *device, uint32_t ext_addr,
                                       void *buffer, uint32_t size, pi_task_t *task);
                                
static ssize_t __pi_hc08_data_send(struct pi_device *device, uint32_t ext_addr,
                                    const void *buffer, uint32_t size, pi_task_t *task);

/*******************************************************************************
 * Internal functions
 ******************************************************************************/

static void __pi_hc08_at_cmd_send(hc08_t *hc08,const char *cmd)
{
    uint32_t cmd_length = strlen(cmd);
    char *cmd_string = (char *) pi_l2_malloc(sizeof(char) * cmd_length);
    if (cmd_string == NULL)
    {
        return;
    }
    strcpy((char*) cmd_string, (char*) cmd);
    pi_uart_write(&(hc08->uart_device), cmd_string, strlen(cmd_string));
}

/*
receive at data
*/
static void __pi_hc08_at_data_received(void *arg)
{
    struct pi_task *param_task = g_param.task;

    g_at_resp_state = PI_AT_RESP_IN_PROGRESS;
    g_param.hc08->buffer[AT_index++] = *(g_param.hc08->byte);
    g_param.hc08->buffer[AT_index] = '\0';                     //EOF

    pi_task_callback(g_param.task, __pi_hc08_at_data_received, &g_param);
    pi_uart_read_async(&(g_param.hc08->uart_device),
                        (void *) g_param.hc08->byte,
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
static int32_t __pi_hc08_at_cmd(struct pi_device *device, const char *cmd,
                                     char *resp, uint32_t size)
{
    hc08_t *hc08 = (hc08_t *) device->data;
    g_at_resp_state = PI_AT_RESP_NOT_STARTED;
    pi_task_t rx_cb = {0};

    /* start timer */
    pi_timer_start(FC_TIMER_1);

    /* clean up the last legacy */
    pi_uart_ioctl(&(hc08->uart_device), PI_UART_IOCTL_ABORT_RX, NULL );
    pi_uart_ioctl(&(hc08->uart_device), PI_UART_IOCTL_ENABLE_RX, NULL );

    g_param.hc08 = hc08;
    g_param.task = &rx_cb;
    pi_task_callback(&rx_cb, __pi_hc08_at_data_received, &g_param);
    pi_uart_read_async(&(hc08->uart_device), (void *) hc08->byte, sizeof(uint8_t), &rx_cb);
    __pi_hc08_at_cmd_send(hc08, cmd);

    while (g_at_resp_state != PI_AT_RESP_DONE)
    {
        pi_yield();
    }
    
    /* stop timer */
    pi_timer_stop(FC_TIMER_1);

    /* the count for receive data  to zero */
    AT_index = 0;

    //printf("Got response: %s\n", hc08->buffer);

    if (size)
    {
        strcpy((void *) resp, (void *) hc08->buffer);
    }
    return 0;
}

/*
    send data
*/
static ssize_t __pi_hc08_data_send(struct pi_device *device, uint32_t ext_addr,
                                    const void *buffer, uint32_t size, pi_task_t *task)
{
    hc08_t *hc08 = (hc08_t *) device->data;
    pi_uart_write_async(&(hc08->uart_device), (void *) buffer, size, task);
    return 0;
}

/*
   we send data which the data manually add '\r'and'\n'.
   so ,we should process the '\r'and'\n'.  
   note : 
        '\r'and'\n' are hex decimal. not "\r" and "\n"
*/
static void __pi_hc08_data_received(void *arg)
{
    struct pi_task *param_task = g_param.task;
    static uint32_t index = 0;              //should move to outside.
    static unsigned char prev_byte = 0;

    if ((g_at_resp_state == PI_AT_RESP_IN_PROGRESS) &&
        (prev_byte == end_op.first_operator) && (*(g_param.hc08->byte) == end_op.second_operator))
    {
        param_task->id = PI_TASK_NONE_ID;
        g_param.hc08->buffer[--index] = '\0';
        g_param.hc08->buffer_len = index;
        
        index = 0;

        pi_uart_read_async(&(g_param.hc08->uart_device),
                            (void *) g_param.hc08->byte,
                            sizeof(uint8_t),
                            g_param.task);

        pi_uart_read_async(&(g_param.hc08->uart_device),
                            (void *) g_param.hc08->byte,
                            sizeof(uint8_t),
                            g_param.task); 

        g_at_resp_state = PI_AT_RESP_DONE;

    }
    else
    {
        g_at_resp_state = PI_AT_RESP_IN_PROGRESS;
        prev_byte = *(g_param.hc08->byte);
        g_param.hc08->buffer[index++] = *(g_param.hc08->byte);

        pi_task_callback(g_param.task, __pi_hc08_data_received, &g_param);
            pi_uart_read_async(&(g_param.hc08->uart_device),
                            (void *) g_param.hc08->byte,
                            sizeof(uint8_t),
                            g_param.task);
    }
    
}

/*
device->api->read(device, 0, (void *) buffer, size, task);
*/
static ssize_t __pi_hc08_data_get(struct pi_device *device, uint32_t ext_addr,
                                       void *buffer, uint32_t size, pi_task_t *task)
{
    hc08_t *hc08 = (hc08_t *) device->data;
    g_at_resp_state = PI_AT_RESP_NOT_STARTED;
    g_param.hc08 = hc08;
    g_param.task = task;

    /* clean up the last legacy */
    pi_uart_ioctl(&(hc08->uart_device), PI_UART_IOCTL_ABORT_RX, NULL );
    pi_uart_ioctl(&(hc08->uart_device), PI_UART_IOCTL_ENABLE_RX, NULL );

    pi_task_callback(task, __pi_hc08_data_received, &g_param);
    pi_uart_read_async(&(hc08->uart_device), (void *) hc08->byte, sizeof(uint8_t), task);

    while (g_at_resp_state != PI_AT_RESP_DONE)
    {
        pi_yield();
    }

    memcpy(buffer,g_param.hc08->buffer,g_param.hc08->buffer_len);

    printf("--code:(%s)\n",g_param.hc08->buffer);
    
    memset(g_param.hc08->buffer,0,PI_AT_RESP_ARRAY_LENGTH);

    return 0;
}

/*******************************************************************************
 * API implementation
 ******************************************************************************/

static int __pi_hc08_open(struct pi_device *device)
{

    struct pi_hc08_conf *conf = (struct pi_hc08_conf *) device->config;
    /* hc08 device. */
    hc08_t *hc08 = (hc08_t *) pi_l2_malloc(sizeof(hc08_t));
    if (hc08 == NULL)
    {
        return -1;
    }
    hc08->byte = (char *) pi_l2_malloc(sizeof(char) * 1);
    if (hc08->byte == NULL)
    {
        pi_l2_free(hc08, sizeof(hc08_t));
        return -2;
    }
    hc08->buffer = (char *) pi_l2_malloc(sizeof(char) * (uint32_t) PI_AT_RESP_ARRAY_LENGTH);
    if (hc08->byte == NULL)
    {
        pi_l2_free(hc08->byte, sizeof(sizeof(char) * 1));
        pi_l2_free(hc08, sizeof(hc08_t));
        return -3;
    }
    device->data = (void *) hc08;
    
    if(bsp_hc08_open()) 
    {
        pi_l2_free(hc08->byte, sizeof(sizeof(char) * 1));
        pi_l2_free(hc08, sizeof(hc08_t));
        printf("[hc08] Error during driver opening: failed to init bsp for hc08\n");
        return -3;
    }

    /* Init and open UART device. */
    struct pi_uart_conf uart_conf = {0};
    pi_uart_conf_init(&uart_conf);
    uart_conf.uart_id = conf->uart_itf;
    uart_conf.baudrate_bps = (uint32_t) PI_HC08_UART_BAUDRATE;
    uart_conf.enable_rx = 1;
    uart_conf.enable_tx = 1;
    pi_open_from_conf(&(hc08->uart_device), &uart_conf);
    if (pi_uart_open(&(hc08->uart_device)))
    {
        pi_l2_free(hc08->buffer, sizeof(sizeof(char) * (uint32_t) PI_AT_RESP_ARRAY_LENGTH));
        pi_l2_free(hc08->byte, sizeof(sizeof(char) * 1));
        pi_l2_free(hc08, sizeof(hc08_t));
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
pi_ble_ioctl(&ble, PI_NINA_B112_CLIENT_CONFIGURE, NULL);

struct hc08_end_operator{
    char first_operator;
    char second_operator;
}
*/
static int __pi_hc08_ioctl(struct pi_device *device, uint32_t cmd, void *arg)
{
    hc22_t *hc22 = (hc22_t *) device->data;
    pi_ble_ioctl_cmd_e func_id = (pi_ble_ioctl_cmd_e) cmd;
    switch(func_id)
    {
        case PI__HC08_MODIFY_END_OPERATOR:
        {
            end_op.first_operator = ((struct hc08_end_operator *)arg)->first_operator;
            end_op.second_operator = ((struct hc08_end_operator *)arg)->second_operator;
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

static int __pi_hc08_close(struct pi_device *device)
{
    hc08_t *hc08 = (hc08_t *) device->data;
    pi_uart_close(&(hc08->uart_device));
    pi_l2_free(hc08->buffer, sizeof(sizeof(char) * (uint32_t) PI_AT_RESP_ARRAY_LENGTH));
    pi_l2_free(hc08->byte, sizeof(sizeof(char) * 1));
    pi_l2_free(hc08, sizeof(hc08_t));
    return 0;
}

void pi_ble_hc08_conf_init(struct pi_device *device, struct pi_hc08_conf *conf)
{
    bsp_hc08_conf_init(conf);
    /* Bind HC08 API to BLE API. */
    g_hc08_api.open = &__pi_hc08_open;
    g_hc08_api.close = &__pi_hc08_close;
    g_hc08_api.read = &__pi_hc08_data_get;
    g_hc08_api.write = &__pi_hc08_data_send;
    g_hc08_api.ioctl = &__pi_hc08_ioctl;
    /* BLE specific API. */
    g_hc08_ble_api.at_cmd = &__pi_hc08_at_cmd;
    g_hc08_api.specific_api = (void *) &g_hc08_ble_api;
    char *str = "Perfxlab Perf-V Beetle";
    strcpy(conf->local_name, str);
    conf->baudrate = (uint32_t) PI_HC08_UART_BAUDRATE; /* Default baudrate. */
    conf->parity_bits = 1;           /* No parity. */
    conf->stop_bits = 1;             /* 1 stop bit. */
    device->api = &g_hc08_api;
}
