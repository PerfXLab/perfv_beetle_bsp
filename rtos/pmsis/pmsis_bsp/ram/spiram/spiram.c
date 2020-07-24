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

/* 
 * Authors: zhangzhijun@perfxlab.com
 */

#include "pmsis.h"
#include "bsp/bsp.h"
#include "bsp/ram/spiram.h"
#include "pmsis/drivers/spi.h"
#include "../extern_alloc.h"

typedef struct
{
    struct pi_device spi_device;
    struct pi_spi_conf qspi_conf;
    extern_alloc_t alloc;
} spiram_t;

static uint32_t ucode_buffer[12];
static uint8_t status_reg[52];


static inline void spiram_ly68l6400slit_readID(spiram_t * spiram)
{
    uint8_t *ucode = (uint8_t*)ucode_buffer;
    volatile uint32_t *ucode_u32 = (uint32_t*)ucode;
    
    ucode[0] = 0x9F;
    ucode[1] = 0x0;
    ucode[2] = 0x0;
    ucode[3] = 0x0;

    ucode[4] = 0x0;
    ucode[5] = 0x0;
    ucode[6] = 0x0;
    pi_spi_send(&spiram->spi_device, (void*)&ucode[0],1* 8 , PI_SPI_LINES_SINGLE | PI_SPI_CS_KEEP);
    pi_spi_send(&spiram->spi_device, (void*)&ucode[4],3* 8 , PI_SPI_LINES_SINGLE | PI_SPI_CS_KEEP);
    pi_spi_receive(&spiram->spi_device, (void*)status_reg, 50*8, PI_SPI_LINES_SINGLE | PI_SPI_CS_AUTO);

    printf("--spiram--MFID(%x)--KGD(%x)--\n",status_reg[0],status_reg[1]);
}

static inline void spiram_set_quad_enable(spiram_t * spiram)
{
    uint8_t *ucode = (uint8_t*)ucode_buffer;
    volatile uint32_t *ucode_u32 = (uint32_t*)ucode;
    
    ucode[0] = 0x35;
    pi_spi_send(&spiram->spi_device, (void*)ucode,1* 8 , PI_SPI_LINES_SINGLE | PI_SPI_CS_AUTO);
}

static int spiram_open(struct pi_device *device)
{  
    printf("Opening SPIRAM device (device: %p)\n", device);

    struct pi_spiram_conf *conf = (struct pi_spiram_conf *)device->config;

    spiram_t *spiram = (spiram_t *)pi_l2_malloc(sizeof(spiram_t));
    if (spiram == NULL)
    {
        printf("[SPIRAM] Error during driver opening: failed to allocate memory for internal structure\n");
        goto error0;
    }

    device->data = (void *)spiram;

    if (extern_alloc_init(&spiram->alloc, 0, conf->ram_size))
    {
        printf("[SPIRAM] Error during driver opening: failed to allocate memory for internal structure\n");
        goto error2;
    }

    if (bsp_spiram_open(conf))
    {
        printf("[SPIRAM] Error during driver opening: failed to init bsp for spiram\n");
        goto error3;
    }

    struct pi_spi_conf spi_conf;
    pi_spi_conf_init(&spi_conf);
    spi_conf.itf = conf->spi_itf;
    spi_conf.cs = conf->spi_cs;
    spi_conf.max_baudrate = 50000000;
    spi_conf.big_endian = 0;
    spi_conf.wordsize = PI_SPI_WORDSIZE_8;
    spi_conf.polarity = 0;
    spi_conf.phase = 0;
    
    pi_open_from_conf(&spiram->spi_device, &spi_conf);

    int32_t error = pi_spi_open(&spiram->spi_device);
    if (error)
    {
        printf("[SPIRAM] Error during driver opening: failed to open spi driver\n");
        goto error3;
    }

//    spiram_set_quad_enable(spiram);

    spiram_ly68l6400slit_readID(spiram);
    return 0;
error3:
    extern_alloc_deinit(&spiram->alloc);
error2:
    pi_l2_free(spiram, sizeof(spiram_t));
error0:
    return -1;
}

static void spiram_close(struct pi_device *device)
{
    spiram_t *spiram = (spiram_t *)device->data;
    printf("Closing SPIRAM device (device: %p)\n",device);
    pi_spi_close(&spiram->spi_device);
    pi_l2_free(spiram, sizeof(spiram_t));
}
static int  spiram_write_async(struct pi_device *device, uint32_t addr, void *data, uint32_t size)
{

    uint8_t *ucode = (uint8_t*)ucode_buffer; 
    volatile uint32_t *ucode_u32 = (uint32_t*)ucode;
    uint32_t ram_addr = addr;

    ucode[0] = 0x38;
    ucode[1] = 0X00;
    ucode[2] = 0X00;
    ucode[3] = 0X00;
#if 0
    ucode[4] = 
    ucode[5] = (ram_addr & 0x0000FF00)>>8;
    ucode[6] = (ram_addr & 0x000000FF)>>0;
    ucode[7] = 0X00;
#endif
    ucode[4] = (ram_addr & 0x000000FF)>>0;
    ucode[5] = (ram_addr & 0x0000FF00)>>8;
    ucode[6] = (ram_addr & 0x00FF0000)>>16;
    ucode[7] = 0X00;

    pi_spi_send(device, (void*)ucode, 1*8,PI_SPI_LINES_SINGLE | PI_SPI_CS_KEEP);

    pi_spi_send(device, (void*)&ucode[4], 3*8,PI_SPI_LINES_QUAD | PI_SPI_CS_KEEP);

    pi_spi_send(device, (void*)data, size*8,PI_SPI_LINES_QUAD | PI_SPI_CS_AUTO);

    return 0;
}

static int  spiram_read_async(struct pi_device *device, uint32_t addr, void *data, uint32_t size)
{

    uint8_t *ucode = (uint8_t*)ucode_buffer; 
    volatile uint32_t *ucode_u32 = (uint32_t*)ucode;
    uint32_t curr_addr = addr;

    ucode[0] = 0xEB;    //W25Q_R_FAST_QIO_CMD;
    ucode[1] = 0x00;
    ucode[2] = 0x00;
    ucode[3] = 0x00;
#if 0
    ucode[4] = 
    ucode[5] = (curr_addr >> 8) & 0xFFUL;
    ucode[6] = curr_addr & 0xFFUL;
    ucode[7] = 0x00;	//bit[5,4] ==2b`10
#endif

    ucode[4] = curr_addr & 0xFFUL;
    ucode[5] = (curr_addr >> 8) & 0xFFUL;
    ucode[6] = (curr_addr >> 16 )& 0xFFUL;
    ucode[7] = 0x00;	//bit[5,4] ==2b`10

    ucode[8] = 0x00;	//dummy,2byte
    ucode[9] = 0x00;
    ucode[10] = 0x00;	//dummy,2byte
    ucode[11] = 0x00;	//dummy,2byte

    pi_spi_send(device, (void*)ucode,1*8 , PI_SPI_LINES_SINGLE | PI_SPI_CS_KEEP);

    pi_spi_send(device, (void*)&ucode[4],3*8 , PI_SPI_LINES_QUAD | PI_SPI_CS_KEEP);

    pi_spi_send(device, (void*)&ucode[8],3*8 , PI_SPI_LINES_QUAD | PI_SPI_CS_KEEP);

    pi_spi_receive(device, (void*)data, (size)*8,PI_SPI_LINES_QUAD | PI_SPI_CS_AUTO);

    return 0;
}

static void spiram_copy_async(struct pi_device *device, uint32_t addr, void *data, uint32_t size, int ext2loc, pi_task_t *task)
{
    spiram_t *spiram = (spiram_t *)device->data;

    if (ext2loc)
        spiram_read_async(&spiram->spi_device, addr, data, size);
    else
        spiram_write_async(&spiram->spi_device, addr, data, size);

}


int spiram_alloc(struct pi_device *device, uint32_t *addr, uint32_t size)
{
    void *chunk;
    spiram_t *spiram = (spiram_t *)device->data;

    int err = extern_alloc(&spiram->alloc, size, &chunk);
    *addr = (uint32_t)chunk;
    return err;
}


int spiram_free(struct pi_device *device, uint32_t addr, uint32_t size)
{
    spiram_t *spiram = (spiram_t *)device->data;
    return extern_free(&spiram->alloc, size, (void *)addr);
}

static pi_ram_api_t spiram_api = {
    .open                 = &spiram_open,
    .close                = &spiram_close,
    .copy_async           = &spiram_copy_async,
    .alloc                = &spiram_alloc,
    .free                 = &spiram_free,
};


void pi_spiram_conf_init(struct pi_spiram_conf *conf)
{
    conf->ram.api = &spiram_api;
    conf->baudrate = 50000000;
    bsp_spiram_conf_init(conf);
}

