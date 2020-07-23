/*
 * Copyright (C) 2018 Perfxlab Technologies
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
 * zhangzhijun@perfxlab.com
 */


/*
 * TODO 1: Mutexes on all functions using shared vars
 * TODO 2: re uniformize api
 * TODO 3: Port to new spi API (depend on implem new api for spi)
 */
//#include "stdio.h"
#include "pmsis.h"
#include "bsp/bsp.h"
#include "bsp/flash/spiflash.h"
#include "pmsis/drivers/spi.h"


typedef struct {
  struct pi_device qspi_dev;
  struct pi_spi_conf qspi_conf;
  size_t sector_size;
  uint8_t cmd_buf[4];
  uint8_t status_reg[4];

} spi_flash_t;


#define IS_BUSY(status_reg) ((status_reg[0] >> 0) & 0x1)

#define QSPIF_PAGE_SIZE        0x100

#define W25Q_R_DEVID_CMD	((uint8_t)0x90)
#define W25Q_R_QE_CMD		((uint8_t)0X35)
#define W25Q_W_QE_CMD		((uint8_t)0X31)
#define W25Q_W_ENABLE_CMD	((uint8_t)0X06)
#define W25Q_W_DISABLE_CMD	((uint8_t)0X04)
#define W25Q_R_STATUS1_CMD	((uint8_t)0X05)
#define W25Q_W_STATUS1_CMD	((uint8_t)0X01)
#define W25Q_R_DATA_CMD		((uint8_t)0X03)
#define W25Q_R_FAST_QIO_CMD	((uint8_t)0xEB)
#define W25Q_R_FAST_QOUT_CMD ((uint8_t)0x6B)
#define W25Q_W_QPAGE_PROG_CMD ((uint8_t)0x32)
#define W25Q_CHIP_ERASE_CMD ((uint8_t)0xC7)
#define W25Q_SECTOR_ERASE_CMD ((uint8_t)0X20)

PI_L2 static const uint8_t w25q_chip_erase[] = {W25Q_CHIP_ERASE_CMD};
PI_L2 static const uint8_t w25q_write_enable[] = {W25Q_W_ENABLE_CMD};
PI_L2 static const uint8_t w25q_write_disable[] = {W25Q_W_DISABLE_CMD};



static void wait_wip(spi_flash_t *flash_dev);
static uint32_t ucode_buffer[9];


static int get_wip(spi_flash_t *flash_dev)
{
    uint8_t *status_reg = flash_dev->status_reg;
    uint8_t *cmd_buf = flash_dev->cmd_buf;
    pi_device_t *qspi_dev = &flash_dev->qspi_dev;

    cmd_buf[0] = 0X05;   //W25Q_R_STATUS1_CMD; // read status reg
    pi_spi_send(qspi_dev, (void*)&cmd_buf[0], 1*8,PI_SPI_LINES_SINGLE | PI_SPI_CS_KEEP);
    pi_spi_receive(qspi_dev, (void*)&status_reg[0], 1*8, PI_SPI_LINES_SINGLE | PI_SPI_CS_AUTO);
            
    return IS_BUSY(status_reg);
}

static void wait_wip(spi_flash_t *flash_dev)
{
    do
    {
        pi_time_wait_us(5000);
    } while(get_wip(flash_dev));
}

int spiflash_erase(struct pi_device *bsp_flash_dev, uint32_t addr, int size)
{
    spi_flash_t *flash_dev  =(spi_flash_t*) bsp_flash_dev->data;
    uint32_t flash_addr = addr;
    uint32_t sector_size = flash_dev->sector_size;
    pi_device_t *qspi_dev = &flash_dev->qspi_dev;

    uint8_t *ucode = (uint8_t*)ucode_buffer; 
    volatile uint32_t *ucode_u32 = (uint32_t*)ucode;
    wait_wip(flash_dev);
    for(uint32_t c_size = 0; c_size < (unsigned int)size; c_size += sector_size)
    {
        uint32_t curr_flash_addr = flash_addr+c_size;
        ucode[0] = 0x20;  //W25Q_SECTOR_ERASE_CMD;
        ucode[1] = 0x0;
        ucode[2] = 0x0;
        ucode[3] = 0x0;


        ucode[4] = (curr_flash_addr & 0x00FF0000)>>16;
        ucode[5] = (curr_flash_addr & 0x0000FF00)>>8;
        ucode[6] = (curr_flash_addr & 0x000000FF)>>0;
        ucode[7] = 0x00;

        pi_spi_send(qspi_dev, (void*)w25q_write_enable, 8,PI_SPI_LINES_SINGLE | PI_SPI_CS_AUTO);
        pi_spi_send(qspi_dev, (void*)ucode, 8*1,PI_SPI_LINES_SINGLE | PI_SPI_CS_KEEP);

        pi_spi_send(qspi_dev, (void*)&ucode[4], 8*3,PI_SPI_LINES_SINGLE | PI_SPI_CS_AUTO);

        wait_wip(flash_dev);
        pi_spi_send(qspi_dev, (void*)w25q_write_disable, 8,PI_SPI_LINES_SINGLE | PI_SPI_CS_AUTO);
    }
    return 0;
}

int spiflash_erase_sector(struct pi_device *bsp_flash_dev, uint32_t addr)
{
    spi_flash_t *flash_dev  = (spi_flash_t*)bsp_flash_dev->data;
    uint32_t flash_addr = addr;
    uint32_t sector_size = flash_dev->sector_size;

    return spiflash_erase(bsp_flash_dev, addr, sector_size);
}


static void spiflash_erase_async(struct pi_device *device, uint32_t addr,
        int size, pi_task_t *task)
{
    spiflash_erase(device,addr,size);
}

static void spiflash_erase_sector_async(struct pi_device *device, uint32_t addr,
        pi_task_t *task)
{
    spiflash_erase_sector(device,addr);
}

int spiflash_erase_chip(struct pi_device *bsp_flash_dev)
{
    spi_flash_t *flash_dev = (spi_flash_t*) bsp_flash_dev->data;

    pi_device_t *qspi_dev = &flash_dev->qspi_dev;
    wait_wip(flash_dev);
    // flash chip erase (optional!)
    pi_spi_send(qspi_dev, (void*)w25q_write_enable, 8, PI_SPI_LINES_SINGLE | PI_SPI_CS_AUTO);

    pi_spi_send(qspi_dev, (void*)w25q_chip_erase, 8,PI_SPI_LINES_SINGLE | PI_SPI_CS_AUTO);

    wait_wip(flash_dev);
    pi_spi_send(qspi_dev, (void*)w25q_write_disable, 8,PI_SPI_LINES_SINGLE | PI_SPI_CS_AUTO);
    return 0;
}

static void spiflash_erase_chip_async(pi_device_t *device, pi_task_t *task)
{
    spiflash_erase_chip(device);
}


static int spiflash_program(struct pi_device *bsp_flash_dev, uint32_t flash_addr,
        const void *data, uint32_t size)
{
    spi_flash_t *flash_dev  = (spi_flash_t*) bsp_flash_dev->data;
    pi_device_t *qspi_dev = &flash_dev->qspi_dev;

    uint8_t *ucode = (uint8_t*)ucode_buffer; 
    volatile uint32_t *ucode_u32 = (uint32_t*)ucode;

    uint8_t *l2_buff = pi_l2_malloc(QSPIF_PAGE_SIZE);
    if(!l2_buff)
    {
        //printf("Malloc failed!\n");
        return -1;
    }
    uint32_t size_left = size;
    uint32_t curr_size = 0;
    uint32_t curr_pos = 0;
    wait_wip(flash_dev);

    if((flash_addr & 0xFF) && (((flash_addr & 0xFF)+size_left) > 0x100))
    {
        curr_pos  = 0;
        curr_size = 0x100 - (flash_addr & 0xFF);
        size_left -= curr_size;

        memcpy(l2_buff, data+curr_pos, curr_size);

        ucode[0] = 0x32; //W25Q_W_QPAGE_PROG_CMD;	//QUAD input page program 0x32.
        ucode[1] = 0X00;
        ucode[2] = 0X00;
        ucode[3] = 0X00;

        ucode[4] = ((flash_addr+curr_pos) & 0x00FF0000)>>16;
        ucode[5] = ((flash_addr+curr_pos) & 0x0000FF00)>>8;
        ucode[6] = ((flash_addr+curr_pos) & 0x000000FF)>>0;
        ucode[7] = 0X00;

        pi_spi_send(qspi_dev, (void*)w25q_write_enable, 8,PI_SPI_LINES_SINGLE | PI_SPI_CS_AUTO);

        pi_spi_send(qspi_dev, (void*)ucode, 1*8,PI_SPI_LINES_SINGLE | PI_SPI_CS_KEEP);

        pi_spi_send(qspi_dev, (void*)&ucode[4], 3*8,PI_SPI_LINES_SINGLE | PI_SPI_CS_KEEP);

        pi_spi_send(qspi_dev, (void*)l2_buff, curr_size*8,PI_SPI_LINES_QUAD | PI_SPI_CS_AUTO);

        wait_wip(flash_dev);

        pi_spi_send(qspi_dev, (void*)w25q_write_disable, 8,PI_SPI_LINES_SINGLE | PI_SPI_CS_AUTO);
        curr_pos += curr_size;

    }

    while(size_left)
    {
        if(size_left >= QSPIF_PAGE_SIZE)
        {
            curr_size = QSPIF_PAGE_SIZE;
            size_left -= QSPIF_PAGE_SIZE;
        }
        else
        {
            curr_size = size_left;
            size_left = 0;
        }
        memcpy(l2_buff, data+curr_pos, curr_size);

        ucode[0] = 0x32;	//W25Q_W_QPAGE_PROG_CMD;  QUAD input page program 0x32.
        ucode[1] = 0X00;
        ucode[2] = 0X00;
        ucode[3] = 0X00;

        ucode[4] = ((flash_addr+curr_pos) & 0x00FF0000)>>16;
        ucode[5] = ((flash_addr+curr_pos) & 0x0000FF00)>>8;
        ucode[6] = ((flash_addr+curr_pos) & 0x000000FF)>>0;
        ucode[7] = 0X00;

        pi_spi_send(qspi_dev, (void*)w25q_write_enable, 8,PI_SPI_LINES_SINGLE | PI_SPI_CS_AUTO);

        pi_spi_send(qspi_dev, (void*)ucode, 1*8, PI_SPI_LINES_SINGLE | PI_SPI_CS_KEEP);

        pi_spi_send(qspi_dev, (void*)&ucode[4], 3*8,PI_SPI_LINES_SINGLE | PI_SPI_CS_KEEP);

        pi_spi_send(qspi_dev, (void*)l2_buff, (curr_size)*8,PI_SPI_LINES_QUAD | PI_SPI_CS_AUTO);

        wait_wip(flash_dev);

        pi_spi_send(qspi_dev, (void*)w25q_write_disable, 8,PI_SPI_LINES_SINGLE | PI_SPI_CS_AUTO);

        curr_pos += curr_size;
    }

    pi_l2_free(l2_buff, QSPIF_PAGE_SIZE);
    return 0;
}


static void spiflash_program_async(struct pi_device *device, uint32_t addr,
        const void *data, uint32_t size, pi_task_t *task)
{
    spiflash_program(device,addr,data,size);
}

static int spiflash_read(struct pi_device *bsp_flash_dev, uint32_t addr, void *data,
        uint32_t size)
{
    spi_flash_t *flash_dev  =(spi_flash_t*) bsp_flash_dev->data;
    pi_device_t *qspi_dev = &flash_dev->qspi_dev;

    uint8_t *ucode = (uint8_t*)ucode_buffer; 
    volatile uint32_t *ucode_u32 = (uint32_t*)ucode;

    uint32_t flash_addr = addr;
    uint32_t sector_size = flash_dev->sector_size;

    uint32_t *l2_buff = pi_l2_malloc(QSPIF_PAGE_SIZE);

    if(!l2_buff)
    {
        //printf("MALLOC FAILED\n");
        return -1;
    }
    uint32_t size_left = size;
    uint32_t curr_size = 0;
    uint32_t curr_pos = 0;

    wait_wip(flash_dev);

    while(size_left)
    {
        if(size_left >= QSPIF_PAGE_SIZE)
        {
            curr_size = QSPIF_PAGE_SIZE;
            size_left -= QSPIF_PAGE_SIZE;
        }
        else
        {
            curr_size = size_left;
            size_left = 0;
        }
        uint32_t curr_addr = flash_addr+curr_pos;

        //set fast read quad io 0xEB
        ucode[0] = 0xEB;    //W25Q_R_FAST_QIO_CMD;
        ucode[1] = 0x00;
        ucode[2] = 0x00;
        ucode[3] = 0x00;

        ucode[4] = (curr_addr >> 16 )& 0xFFUL;
        ucode[5] = (curr_addr >> 8) & 0xFFUL;
        ucode[6] = curr_addr & 0xFFUL;
        ucode[7] = 0xff;	//bit[5,4] ==2b`10

        ucode[8] = 0x00;	//dummy,2byte
        ucode[9] = 0x00;

        pi_spi_send(qspi_dev, (void*)ucode,1*8 , PI_SPI_LINES_SINGLE | PI_SPI_CS_KEEP);

        pi_spi_send(qspi_dev, (void*)&ucode[4],6*8 , PI_SPI_LINES_QUAD | PI_SPI_CS_KEEP);

        pi_spi_receive(qspi_dev, (void*)l2_buff, (curr_size)*8,PI_SPI_LINES_QUAD | PI_SPI_CS_AUTO);

        memcpy(data+curr_pos, l2_buff, curr_size);
        curr_pos += curr_size;
    }
    pi_l2_free(l2_buff, QSPIF_PAGE_SIZE);
    wait_wip(flash_dev);
    return 0;

}

static void spiflash_read_async(struct pi_device *device, uint32_t addr, void *data,
        uint32_t size, pi_task_t *task)
{
    spiflash_read(device,addr,data,size);
}


int w25q128_readID(struct pi_device *bsp_flash_dev)
{
    spi_flash_t *flash_dev  =(spi_flash_t*) bsp_flash_dev->data;
    pi_device_t *qspi_dev = &flash_dev->qspi_dev;

    uint8_t* cmd_buf = flash_dev->cmd_buf;
    uint8_t* status_reg = flash_dev->status_reg;
    uint8_t *ucode = (uint8_t*)ucode_buffer; 
    volatile uint32_t *ucode_u32 = (uint32_t*)ucode;

    cmd_buf[0] = 0x90;      //W25Q_R_DEVID_CMD;   //0X90
    cmd_buf[1] = 0X00;   //0X00
    cmd_buf[2] = 0X00;   //0X00
    cmd_buf[3] = 0X00;   //0X00
    pi_spi_send(qspi_dev, (void*)cmd_buf,4 * 8 , PI_SPI_LINES_SINGLE | PI_SPI_CS_KEEP);
    pi_spi_receive(qspi_dev, (void*)status_reg, 2*8, PI_SPI_LINES_SINGLE | PI_SPI_CS_AUTO);

    //printf("line(%d):CMD(90),(MF7-MF0) :(%x);(ID7-ID0) :(%x)\n",__LINE__,status_reg[0],status_reg[1]);

    cmd_buf[0] = 0xAB;      //W25Q_R_DEVID_CMD;   
    cmd_buf[1] = 0X00;   //addr,3byte
    cmd_buf[2] = 0X00;   //addr,3byte
    cmd_buf[3] = 0X00;   //addr,3byte
    pi_spi_send(qspi_dev, (void*)cmd_buf,4 * 8 , PI_SPI_LINES_SINGLE | PI_SPI_CS_KEEP);
    pi_spi_receive(qspi_dev, (void*)status_reg, 1*8, PI_SPI_LINES_SINGLE | PI_SPI_CS_AUTO);

    //printf("line(%d):CMD(AB),ID7-ID0 :(%x)\n",__LINE__,status_reg[0]);


    cmd_buf[0] = 0x9F;      //W25Q_R_DEVID_CMD;   
    pi_spi_send(qspi_dev, (void*)cmd_buf,1 * 8 , PI_SPI_LINES_SINGLE | PI_SPI_CS_KEEP);
    pi_spi_receive(qspi_dev, (void*)status_reg, 3*8, PI_SPI_LINES_SINGLE | PI_SPI_CS_AUTO);
    //printf("line(%d):CMD(9F),(MF7-MF0) :(%x);(ID15-ID8) :(%x);(ID7-ID0) :(%x)\n",__LINE__,status_reg[0],status_reg[1],status_reg[2]);

    ucode[0] = 0x94;   //W25Q_R_DEVID_CMD;  
    ucode[1] = 0X00;   
    ucode[2] = 0X00;   
    ucode[3] = 0X00;   

    ucode[4] = 0x00;	//addr,3byte
    ucode[5] = 0X00;  
    ucode[6] = 0X00;  
    ucode[7] = 0xef;	//ef .
    ucode[8] = 0X00;	//dummy ,2byte
    ucode[9] = 0X00;   

    pi_spi_send(qspi_dev, (void*)ucode,1 * 8 , PI_SPI_LINES_SINGLE | PI_SPI_CS_KEEP);
    pi_spi_send(qspi_dev, (void*)&ucode[4],6 * 8 , PI_SPI_LINES_QUAD | PI_SPI_CS_KEEP);
    pi_spi_receive(qspi_dev, (void*)status_reg, 2*8, PI_SPI_LINES_QUAD | PI_SPI_CS_AUTO);
    //printf("line(%d):CMD(94),(MF7-MF0) :(%x);(ID7-ID0) :(%x)\n",__LINE__,status_reg[0],status_reg[1]);

    return 0;
}

static inline void qpi_flash_set_quad_enable(spi_flash_t *flash_dev)
{
    volatile uint8_t* cmd_buf = flash_dev->cmd_buf;
    uint8_t* status_reg = flash_dev->status_reg;
    pi_device_t *qspi_dev = &flash_dev->qspi_dev;
    // read status register, and then write it back with QE bit = 1
    cmd_buf[0] = W25Q_R_QE_CMD; // read status reg
    pi_spi_send(qspi_dev, (void*)&cmd_buf[0], 1*8,PI_SPI_LINES_SINGLE | PI_SPI_CS_KEEP);
    pi_spi_receive(qspi_dev, (void*)&status_reg[0], 1*8,PI_SPI_LINES_SINGLE | PI_SPI_CS_AUTO);
    //printf("%s,%d:(%x)\n",__func__,__LINE__,status_reg[0]);
    if(!((status_reg[0] >> 1)&1))
    {
        status_reg[0] |= (1 << 1);
        // WREN before WRSR
        pi_spi_send(qspi_dev, (void*)w25q_write_enable, 1*8,PI_SPI_LINES_SINGLE | PI_SPI_CS_AUTO);
        cmd_buf[0] = W25Q_W_QE_CMD; // write status reg (holds QE bit)
        cmd_buf[1] = status_reg[0];
        pi_spi_send(qspi_dev, (void*)cmd_buf, 2*8,PI_SPI_LINES_SINGLE | PI_SPI_CS_AUTO);
        //printf("%s,%d:(%x)\n",__func__,__LINE__,status_reg[0]);
        // flash takes some time to recover from QE bit/status reg write
        pi_time_wait_us(100000);
    }
}

static inline void pi_qpi_flash_conf_spi(struct pi_spi_conf *conf,
        struct pi_spiflash_conf *flash_conf)
{
    memset(conf, 0, sizeof(struct pi_spi_conf));
    pi_spi_conf_init(conf);
    conf->max_baudrate = flash_conf->baudrate;
    conf->wordsize = PI_SPI_WORDSIZE_8;
    conf->polarity = 0;   
    conf->phase = conf->polarity;
    conf->cs = flash_conf->spi_cs;
    conf->itf = flash_conf->spi_itf;
    conf->big_endian = 1;
}

static int spiflash_open(struct pi_device *bsp_flash_dev)
{
    struct pi_spiflash_conf *flash_conf = bsp_flash_dev->config;
    spi_flash_t *flash_dev = pi_l2_malloc(sizeof(spi_flash_t));
    if(flash_dev == NULL)
    {
        return -1;
    }
    memset(flash_dev, 0, sizeof(spi_flash_t));

    if(bsp_spiflash_open(flash_conf)){
        goto error0;
    }

    pi_qpi_flash_conf_spi(&flash_dev->qspi_conf, flash_conf);
    pi_open_from_conf(&flash_dev->qspi_dev,&flash_dev->qspi_conf);
    int ret = pi_spi_open(&flash_dev->qspi_dev);
    if(ret)
    {
        goto error0;
    }

    flash_dev->sector_size = flash_conf->sector_size;
    bsp_flash_dev->data = (void*)flash_dev;

    qpi_flash_set_quad_enable(flash_dev);

    w25q128_readID(bsp_flash_dev);

    wait_wip(flash_dev);
    return 0;

error0:
    pi_l2_free(flash_dev, sizeof(spi_flash_t));

    return -1;
}

void spiflash_close(struct pi_device *bsp_flash_dev)
{
    spi_flash_t *flash_dev = (spi_flash_t*)bsp_flash_dev->data;
    wait_wip(flash_dev);
    pi_spi_close(&flash_dev->qspi_dev);
    pi_l2_free(flash_dev, sizeof(spi_flash_t));
}




static pi_flash_api_t spiflash_api = {
  .open                 = &spiflash_open,
  .close                = &spiflash_close,
  .read_async           = &spiflash_read_async,
  .program_async        = &spiflash_program_async,
  .erase_chip_async     = &spiflash_erase_chip_async,
  .erase_sector_async   = &spiflash_erase_sector_async,
  .erase_async          = &spiflash_erase_async,
  .read                 = &spiflash_read,
  .program              = &spiflash_program,
  .erase_chip           = &spiflash_erase_chip,
  .erase_sector         = &spiflash_erase_sector,
  .erase                = &spiflash_erase,
};

void pi_spiflash_conf_init(struct pi_spiflash_conf *conf)
{
    conf->flash.api = &spiflash_api;
    bsp_spiflash_conf_init(conf);
    __flash_conf_init(&conf->flash);
}

