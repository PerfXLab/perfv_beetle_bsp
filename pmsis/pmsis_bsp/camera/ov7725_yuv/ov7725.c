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

/*
 * Authors: zhangzhijun@perfxlab.com.
 */

#include "pmsis.h"
#include "pmsis/rtos/os_frontend_api/pmsis_time.h"
#include "bsp/bsp.h"
#include "bsp/camera/ov7725.h"
#include "ov7725.h"

typedef struct {
  uint8_t addr;
  uint8_t value;
} i2c_req_t;

typedef struct {
  struct pi_device cpi_device;
  struct pi_device i2c_device;

  struct pi_device ov7725_reset; 
  struct pi_device ov7725_pwdn; 
  
  i2c_req_t i2c_req;
  uint32_t i2c_read_value;
  
  int reset_gpio;
  int pwdn_gpio;
  
  int is_awake;
} ov7725_t;

typedef struct {
    uint8_t addr;
    uint8_t data;
} ov7725_reg_init_t;

static ov7725_reg_init_t __ov7725_reg_init[] =
{
	{0x32,0x00},
	{0x2a,0x00},
	{0x11,0x03},
	{0x12,0x40},//QVGA YUV
	
	{0x42,0x7f},
	{0x4d,0x00},
	{0x63,0xf0},
	{0x64,0x1f},
	{0x65,0x20},
	{0x66,0x00},    //COM3[4] == 0b0; and DSP_CTRL3[7] == 0b0 ; yuv output order:Y0U0,Y1V1,Y2U2,Y3V3...
	{0x67,0x00},
	{0x69,0x50},  
	
	
	{0x13,0xff},
	{0x0d,0x41},
	{0x0f,0x01},
	{0x14,0x06},
	
	{0x24,0x75},
	{0x25,0x63},
	{0x26,0xd1},
	{0x2b,0xff},
	{0x6b,0xaa},
	
	{0x8e,0x10},
	{0x8f,0x00},
	{0x90,0x00},
	{0x91,0x00},
	{0x92,0x00},
	{0x93,0x00},
	
	{0x94,0x2c},
	{0x95,0x24},
	{0x96,0x08},
	{0x97,0x14},
	{0x98,0x24},
	{0x99,0x38},
	{0x9a,0x9e},
	{0x15,0x00}, 
	{0x9b,0x00},
	{0x9c,0x20},
	{0xa7,0x40},    //yuv
	{0xa8,0x40},    //yuv
	{0xa9,0x80},    //yuv
	{0xaa,0x80},    //yuv
	
	{0x9e,0x81},    //yuv
	{0xa6,0x06},
	
	{0x7e,0x0c},
	{0x7f,0x16},
	{0x80,0x2a},
	{0x81,0x4e},
	{0x82,0x61},
	{0x83,0x6f},
	{0x84,0x7b},
	{0x85,0x86},
	{0x86,0x8e},
	{0x87,0x97},
	{0x88,0xa4},
	{0x89,0xaf},
	{0x8a,0xc5},
	{0x8b,0xd7},
	{0x8c,0xe8},
	{0x8d,0x20},
	
	{0x33,0x40},
	{0x34,0x00},
	{0x22,0xaf},
	{0x23,0x01},
	
	{0x49,0x10},
	{0x4a,0x10},
	{0x4b,0x14},
	{0x4c,0x17},
	{0x46,0x05},
	
	{0x47,0x08},
	{0x0e,0x01},
	{0x0c,0x40},      //YUV 0x10 / 0x00 ,they are ok,I think.   bit[4] swap Y/UV
	{0x09,0x03},
	
	{0x29,0x50},
	{0x2C,0x78},
	
	{0x00,0x00},
	{0x00,0x00},
	{0x00,0x60},

};

static inline int is_i2c_active()
{
#if defined(ARCHI_PLATFORM_RTL)

  // I2C driver is not yet working on some chips, at least this works on gvsoc.
  // Also there is noI2C connection to camera model on RTL
#if PULP_CHIP == CHIP_GAP9 || PULP_CHIP == CHIP_VEGA || PULP_CHIP == CHIP_ARNOLD || PULP_CHIP == CHIP_PULPISSIMO || PULP_CHIP == CHIP_PULPISSIMO_V1
  return 0;
#else
  return rt_platform() != ARCHI_PLATFORM_RTL;
#endif

#else

  return 1;

#endif
}

static void __ov7725_reg_write(ov7725_t *ov7725, unsigned int addr, uint8_t value)
{
  if (is_i2c_active())
  {
    ov7725->i2c_req.value = value;
    ov7725->i2c_req.addr = (uint8_t)addr;
    pi_i2c_write(&ov7725->i2c_device, (uint8_t *)&ov7725->i2c_req, 2, PI_I2C_XFER_STOP);
  }
}

static uint8_t __ov7725_reg_read(ov7725_t *ov7725, unsigned int addr)
{
  if (is_i2c_active())
  {
    ov7725->i2c_req.addr = (addr & 0xFF);
    pi_i2c_write(&ov7725->i2c_device, (uint8_t *)&ov7725->i2c_req, 1, PI_I2C_XFER_STOP);
    pi_i2c_read(&ov7725->i2c_device, (uint8_t *)&ov7725->i2c_read_value, 1, PI_I2C_XFER_STOP);
    return *(volatile uint8_t *)&ov7725->i2c_read_value;
  }
  else
  {
    return 0;
  }
}

static void __ov7725_init_regs(ov7725_t *ov7725)
{
    unsigned int i;
    for(i=0; i<(sizeof(__ov7725_reg_init)/sizeof(ov7725_reg_init_t)); i++)
    {
        __ov7725_reg_write(ov7725, __ov7725_reg_init[i].addr, __ov7725_reg_init[i].data);
    }
}

static void __ov7725_reset(ov7725_t *ov7725)
{
  pi_gpio_pin_configure(&ov7725->ov7725_pwdn, ov7725->pwdn_gpio, PI_GPIO_OUTPUT);
  pi_gpio_pin_configure(&ov7725->ov7725_reset, ov7725->reset_gpio, PI_GPIO_OUTPUT);
  pi_gpio_pin_write(&ov7725->ov7725_pwdn, ov7725->pwdn_gpio, 0);
  pi_gpio_pin_write(&ov7725->ov7725_reset, ov7725->reset_gpio, 1);
}

int32_t __ov7725_open(struct pi_device *device)
{
    struct pi_ov7725_conf *conf = (struct pi_ov7725_conf *)device->config;

    ov7725_t *ov7725 = (ov7725_t *)pmsis_l2_malloc(sizeof(ov7725_t));
    if (ov7725 == NULL) return -1;

    device->data = (void *)ov7725;

    if (bsp_ov7725_open(conf))
        goto error;

    struct pi_gpio_conf gpio_reset_conf;
    pi_gpio_conf_init(&gpio_reset_conf);
    pi_open_from_conf(&ov7725->ov7725_reset, &gpio_reset_conf);
    if (pi_gpio_open(&ov7725->ov7725_reset))
        goto error;
    
    struct pi_gpio_conf gpio_pwdn_conf;
    pi_gpio_conf_init(&gpio_pwdn_conf);
    pi_open_from_conf(&ov7725->ov7725_reset, &gpio_pwdn_conf);
    if (pi_gpio_open(&ov7725->ov7725_reset))
        goto error;
    
    ov7725->reset_gpio = conf->reset_gpio;
    ov7725->pwdn_gpio = conf->pwdn_gpio;

    struct pi_cpi_conf cpi_conf;
    pi_cpi_conf_init(&cpi_conf);
    cpi_conf.itf = conf->cpi_itf;
    pi_open_from_conf(&ov7725->cpi_device, &cpi_conf);

    if (pi_cpi_open(&ov7725->cpi_device))
        goto error;

    struct pi_i2c_conf i2c_conf;
    pi_i2c_conf_init(&i2c_conf);
    i2c_conf.cs = 0x42;
    i2c_conf.itf = conf->i2c_itf;
    i2c_conf.max_baudrate = 200000;
    pi_open_from_conf(&ov7725->i2c_device, &i2c_conf);

    if (pi_i2c_open(&ov7725->i2c_device))
        goto error2;

    pi_cpi_set_format(&ov7725->cpi_device, PI_CPI_FORMAT_BYPASS_BIGEND);

    __ov7725_reset(ov7725);

    //printf("---%x-%x---\n", __ov7725_reg_read(ov7725,0x1c),__ov7725_reg_read(ov7725,0x1D));

    __ov7725_init_regs(ov7725);

    printf("ov7725(yuv) init over \n");

    return 0;

error2:
    pi_cpi_close(&ov7725->cpi_device);
error:
    pmsis_l2_malloc_free(ov7725, sizeof(ov7725_t));
    return -1;
}



static void __ov7725_close(struct pi_device *device)
{
    ov7725_t *ov7725 = (ov7725_t *)device->data;
    pi_cpi_close(&ov7725->cpi_device);
    pmsis_l2_malloc_free(ov7725, sizeof(ov7725_t));
}

static int32_t __ov7725_control(struct pi_device *device, pi_camera_cmd_e cmd, void *arg)
{
    int irq = disable_irq();

    ov7725_t *ov7725 = (ov7725_t *)device->data;

    switch (cmd)
    {
        case PI_CAMERA_CMD_START:
            pi_cpi_control_start(&ov7725->cpi_device);
            break;

        case PI_CAMERA_CMD_STOP:
            pi_cpi_control_stop(&ov7725->cpi_device);
            break;

        default:
            break;
    }

    restore_irq(irq);

    return 0;
}

void __ov7725_capture_async(struct pi_device *device, void *buffer, uint32_t bufferlen, pi_task_t *task)
{
    ov7725_t *ov7725 = (ov7725_t *)device->data;

	pi_cpi_capture_async(&ov7725->cpi_device, buffer,bufferlen , task);
}

int32_t __ov7725_reg_set(struct pi_device *device, uint32_t addr, uint8_t *value)
{
    ov7725_t *ov7725 = (ov7725_t *)device->data;
    __ov7725_reg_write(ov7725, addr, *value);
    return 0;
}

int32_t __ov7725_reg_get(struct pi_device *device, uint32_t addr, uint8_t *value)
{
    ov7725_t *ov7725 = (ov7725_t *)device->data;
    *value = __ov7725_reg_read(ov7725, addr);
    return 0;
}

static pi_camera_api_t ov7725_api =
{
    .open           = &__ov7725_open,
    .close          = &__ov7725_close,
    .control        = &__ov7725_control,
    .capture_async  = &__ov7725_capture_async,
    .reg_set        = &__ov7725_reg_set,
    .reg_get        = &__ov7725_reg_get
};



void pi_ov7725_conf_init(struct pi_ov7725_conf *conf)
{
    conf->camera.api = &ov7725_api;
    conf->skip_pads_config = 0;
    bsp_ov7725_conf_init(conf);
    __camera_conf_init(&conf->camera);
}
