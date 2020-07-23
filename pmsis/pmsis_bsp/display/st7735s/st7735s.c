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
 * Authors: zhangzhijun@perfxlab.com
 */


#include "pmsis.h"
#include "pmsis/drivers/spi.h"
#include "pmsis/drivers/hyperbus.h"
#include "pmsis/drivers/gpio.h"
#include "bsp/display/st7735s.h"
#include "bsp/bsp.h"
#include "st7735s.h"


typedef struct
{
  struct pi_device spim;
  struct pi_device gpio_port;
  struct pi_device gpio_rst_port; 
  int width;
  uint8_t * current_data;
  uint32_t current_size;
  uint32_t current_line_len;
  pi_buffer_t *buffer;
  uint8_t temp_buffer[10];

  uint8_t * temp_buff;
  pi_task_t temp_copy_task;
  pi_task_t *current_task;
  int gpio;
  int gpio_rst;                 

  unsigned int _width;
  unsigned int _height;
  int16_t cursor_x;
  int16_t cursor_y;
  int16_t wrap;
  uint8_t textsize;
  uint16_t textcolor;
  uint16_t textbgcolor;
} st_t;

#if 1

static void __st_init(st_t *st);
static void __st_set_addr_window(st_t *st,uint16_t x, uint16_t y, uint16_t w, uint16_t h) ;
static void __st_write_8(st_t *st, uint8_t  d);
static void __st_write_command(st_t *st,uint8_t cmd);
static void __st_color_to_rgb565(uint8_t *input,uint16_t *output,int width, int height);


static uint8_t setWin_flag =1;


/*
51 * 80 = 8160  for one transfer 
51frame.

51*80*2 = 8160Byte

st->current_size = w*h   : total len . or remain len
st->current_line_len = 80 * 51 : the big number for one transfer.
size : the len for current transfer.
*/
static void __st7735s_write_buffer_iter(void *arg)
{
	st_t *st = (st_t *)arg;
	uint32_t size = st->current_size;

	if(size > st->current_line_len )
		size = st->current_line_len;

    pi_task_t *task = &st->temp_copy_task;
    pi_spi_flags_e flags = PI_SPI_CS_KEEP;	

	st->current_size -= size;			//remain len

    if (st->current_size == 0)
    {
        task = st->current_task;
        flags = PI_SPI_CS_AUTO;
    }
	st->temp_buff = st->current_data;

	st->current_data += size*2;
	
	pi_spi_send_async(&st->spim, st->temp_buff, size*2*8, flags, task);

}


/*
x: offset for x axis.
y: offset for y axis.
w: width for display on lcd.
h: hight for display on lcd.

buffer: the buffer for display.
*/
static void __st_write_async(struct pi_device *device, pi_buffer_t *buffer, uint16_t x, uint16_t y,uint16_t w, uint16_t h, pi_task_t *task)
{
  st_t *st = (st_t *)device->data;

    __st_set_addr_window(st, x, y, w, h); // Clipped area

  pi_task_callback(&st->temp_copy_task, __st7735s_write_buffer_iter, (void *)st);

  st->buffer = buffer;
  st->width = w;					
  st->current_task = task;			
  st->current_data = (uint8_t*)buffer->data;	//the buffer for display	
  st->current_size = w*h;						          //the total number for buffer
  st->current_line_len = w*(4096/w);				  //how many pixels for once. udma_SPI max size is 8192. stt7735s is RGB565. so 8192/2=4096

  __st7735s_write_buffer_iter(st);
}


static int __st_open(struct pi_device *device)
{
  struct pi_st7735s_conf *conf = (struct pi_st7735s_conf *)device->config;

  st_t *st = (st_t *)pmsis_l2_malloc(sizeof(st_t));
  if (st == NULL) return -1;

  if (bsp_st7735s_open(conf))
    goto error;

  struct pi_gpio_conf gpio_conf;
  pi_gpio_conf_init(&gpio_conf);

  pi_open_from_conf(&st->gpio_port, &gpio_conf);

  if (pi_gpio_open(&st->gpio_port))
    goto error;

  device->data = (void *)st;

  st->gpio = conf->gpio;
  st->gpio_rst = conf->gpio_rst; 

  struct pi_spi_conf spi_conf;
  pi_spi_conf_init(&spi_conf);
  spi_conf.itf = conf->spi_itf;
  spi_conf.cs = conf->spi_cs;

  spi_conf.wordsize = PI_SPI_WORDSIZE_8;
  spi_conf.big_endian = 1;
  //spi_conf.max_baudrate = 50000000;
  spi_conf.max_baudrate = 20000000;
  spi_conf.polarity = 0;
  spi_conf.phase = 0;

  pi_open_from_conf(&st->spim, &spi_conf);

  if (pi_spi_open(&st->spim))
    goto error;

  __st_init(st);
  st->_width = ST7735S_TFTWIDTH;
  st->_height = ST7735S_TFTHEIGHT;

  st->cursor_x = 0;
  st->cursor_y = 0;
  st->wrap = 0;
  st->textsize = 1;
  st->textcolor = ST7735S_GREEN;
  st->textbgcolor = ST7735S_WHITE;

  return 0;

error:
  pmsis_l2_malloc_free(st, sizeof(st_t));
  return -1;
}

static pi_display_api_t st_api =
{
  .open           = &__st_open,
  .write_async    = &__st_write_async,
};

void pi_st7735s_conf_init(struct pi_st7735s_conf *conf)
{
  conf->display.api = &st_api;
  conf->spi_itf = 0;
  conf->skip_pads_config = 0;
  __display_conf_init(&conf->display);
  bsp_st7735s_conf_init(conf);
}

static void __st_write_8(st_t *st, uint8_t value)
{
  st->temp_buffer[0] = value;
  pi_spi_send(&st->spim, st->temp_buffer, 8, PI_SPI_CS_AUTO);
}

static void __st_write_16(st_t *st, uint16_t value)
{
  __st_write_8(st, value >> 8);
  __st_write_8(st, value);
}

static void __st_write_32(st_t *st, uint32_t value)
{
  __st_write_16(st, value >> 16);
  __st_write_16(st, value);
}

static void __st_write_command(st_t *st, uint8_t cmd)
{
  pi_gpio_pin_write(&st->gpio_port, st->gpio, 0);
  __st_write_8(st,cmd);
  pi_gpio_pin_write(&st->gpio_port, st->gpio, 1);
}


static void __st_init(st_t *st)
{
	pi_gpio_pin_configure(&st->gpio_rst_port, st->gpio_rst, PI_GPIO_OUTPUT);
	pi_gpio_pin_configure(&st->gpio_port, st->gpio, PI_GPIO_OUTPUT);

	pi_gpio_pin_write(&st->gpio_rst_port, st->gpio_rst, 1);
	pi_time_wait_us(200000);
	pi_gpio_pin_write(&st->gpio_rst_port, st->gpio_rst, 0);
	pi_time_wait_us(800000);
	pi_gpio_pin_write(&st->gpio_rst_port, st->gpio_rst, 1);
	pi_time_wait_us(800000);

	pi_gpio_pin_write(&st->gpio_port, st->gpio, 0);

	__st_write_command(st,0xCF);
	__st_write_8(st,0x00);
	__st_write_8(st,0xC1);
	__st_write_8(st,0x30);

	__st_write_command(st,0xED);
	__st_write_8(st,0x64);
	__st_write_8(st,0x03);
	__st_write_8(st,0x12);
	__st_write_8(st,0x81);

	__st_write_command(st,0xE8);
	__st_write_8(st,0x85);
	__st_write_8(st,0x00);
	__st_write_8(st,0x7A);

	__st_write_command(st,0xCB);
	__st_write_8(st,0x39);
	__st_write_8(st,0x2C);
	__st_write_8(st,0x00);
	__st_write_8(st,0x34);
	__st_write_8(st,0x02);

	__st_write_command(st,0xF7);
	__st_write_8(st,0x20);

	__st_write_command(st,0xEA);
	__st_write_8(st,0x00);
	__st_write_8(st,0x00);

	__st_write_command(st,0xc0);
	__st_write_8(st,0x21);

	__st_write_command(st,0xc1);
	__st_write_8(st,0x11);

	__st_write_command(st,0xc5);
	__st_write_8(st,0x25);
	__st_write_8(st,0x32);

	__st_write_command(st,0xc7);
	__st_write_8(st,0xaa);

	__st_write_command(st,0x36);
	__st_write_8(st,0x00);


	__st_write_command(st,0xb6);
	__st_write_8(st,0x0a);
	__st_write_8(st,0xA2);

	__st_write_command(st,0xb1);
	__st_write_8(st,0x00);
	__st_write_8(st,0x1B);

	__st_write_command(st,0xf2);
	__st_write_8(st,0x00);

	__st_write_command(st,0x26);
	__st_write_8(st,0x01);

	__st_write_command(st,0x3a);
	__st_write_8(st,0x55);

	__st_write_command(st,0xE0);
	__st_write_8(st,0x0f);
	__st_write_8(st,0x2D);
	__st_write_8(st,0x0e);
	__st_write_8(st,0x08);
	__st_write_8(st,0x12);
	__st_write_8(st,0x0a);
	__st_write_8(st,0x3d);
	__st_write_8(st,0x95);
	__st_write_8(st,0x31);
	__st_write_8(st,0x04);
	__st_write_8(st,0x10);
	__st_write_8(st,0x09);
	__st_write_8(st,0x09);
	__st_write_8(st,0x0d);
	__st_write_8(st,0x00);

	__st_write_command(st,0xE1);
	__st_write_8(st,0x00);
	__st_write_8(st,0x12);
	__st_write_8(st,0x17);
	__st_write_8(st,0x03);
	__st_write_8(st,0x0d);
	__st_write_8(st,0x05);
	__st_write_8(st,0x2c);
	__st_write_8(st,0x44);
	__st_write_8(st,0x41);
	__st_write_8(st,0x05);
	__st_write_8(st,0x0f);
	__st_write_8(st,0x0a);
	__st_write_8(st,0x30);
	__st_write_8(st,0x32);
	__st_write_8(st,0x0F);

	__st_write_command(st,0x11);
	pi_time_wait_us(120000);
	__st_write_command(st,0x29);

}

static void __st_set_addr_window(st_t *st,uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
#ifdef ST7735S_80X160
  uint32_t xa = ((uint32_t)(x+24) << 16) | (x+24+w-1);
  uint32_t ya = ((uint32_t)y << 16) | (y+h-1);
#else       /* ST7735S_128x160 */
  uint32_t xa = ((uint32_t)(x+2) << 16) | (x+2+w-1);
  uint32_t ya = ((uint32_t)(y+1) << 16) | (y+1+h-1);
#endif
  __st_write_command(st,ST7735S_CASET); // Column addr set
  __st_write_32(st,xa);
  __st_write_command(st,ST7735S_PASET); // Row addr set
  __st_write_32(st,ya);
  __st_write_command(st,ST7735S_RAMWR); // write to RAM
}


void writeColor(struct pi_device *device, uint16_t color, unsigned int len)
{
  st_t *st = (st_t *)device->data;
  for (uint32_t t=0; t<len; t++)
  {
    __st_write_16(st, color);
  }
  return;
}

void writeFillRect(struct pi_device *device, unsigned short x, unsigned short y, unsigned short w, unsigned short h, unsigned short color)
{
  st_t *st = (st_t *)device->data;

  if((x >= st->_width) || (y >= st->_height)) return;
  unsigned short x2 = x + w - 1, y2 = y + h - 1;

  // Clip right/bottom
  if(x2 >= st->_width)  w = st->_width  - x;
  if(y2 >= st->_height) h = st->_height - y;
  unsigned int len = (int32_t)w * h;

  //setAddrWindow(spim,x, y, w, h);
  __st_set_addr_window(st, x, y, w, h); // Clipped area

  writeColor(device, color, len);
}


void setTextWrap(struct pi_device *device,signed short w)
{
  st_t *st = (st_t *)device->data;
  st->wrap = w;
}

void setCursor(struct pi_device *device,signed short x, signed short y)
{
  st_t *st = (st_t *)device->data;
  st->cursor_x = x;
  st->cursor_y = y;
}

void setTextColor(struct pi_device *device,uint16_t c)
{
  st_t *st = (st_t *)device->data;
  // For 'transparent' background, we'll set the bg
  // to the same as fg instead of using a flag
  st->textcolor = c;
}

static void __st_writePixelAtPos(struct pi_device *device,int16_t x, int16_t y, uint16_t color)
{
  st_t *st = (st_t *)device->data;

  if((x < 0) ||(x >= (int)st->_width) || (y < 0) || (y >= (int)st->_height)) return;

  __st_set_addr_window(st, x, y, 1, 1); // Clipped area
  __st_write_16(st,color);
}

static void drawChar(struct pi_device *device,int16_t x, int16_t y, unsigned char c,
  uint16_t color, uint16_t bg, uint8_t size)
{
  st_t *st = (st_t *)device->data;

  if((x >= (int)st->_width)  || // Clip right
     (y >= (int)st->_height) || // Clip bottom
     ((x + 6 * size - 1) < 0) || // Clip left
     ((y + 8 * size - 1) < 0))   // Clip top
    return;

  //if(!_cp437 && (c >= 176)) c++; // Handle 'classic' charset behavior

  for(int8_t i=0; i<5; i++ )
  { // Char bitmap = 5 columns
    uint8_t line = font[c * 5 + i];
    for(int8_t j=0; j<8; j++, line >>= 1)
    {
      if(line & 1)
      {
        if(size == 1)
          __st_writePixelAtPos(device,x+i, y+j, color);
        else
          writeFillRect(device,x+i*size, y+j*size, size, size, color);
      }
      else if(bg != color)
      {
        if(size == 1)
          __st_writePixelAtPos(device,x+i, y+j, bg);
        else
          writeFillRect(device,x+i*size, y+j*size, size, size, bg);
      }
    }
  }
}

static void writeChar(struct pi_device *device,uint8_t c)
{
  st_t *st = (st_t *)device->data;

  if(c == '\n')
  {                        // Newline?
    st->cursor_x  = 0;                     // Reset x to zero,
    st->cursor_y += st->textsize * 8;          // advance y one line
  }
  else if (c != '\r')
  {                 // Ignore carriage returns
    if (st->wrap && ((st->cursor_x + st->textsize * 6) > (int)st->_width))
    { // Off right?
      st->cursor_x  = 0;                 // Reset x to zero,
      st->cursor_y += st->textsize * 8;      // advance y one line
    }
    drawChar(device, st->cursor_x, st->cursor_y, c, st->textcolor, st->textbgcolor, st->textsize);
    st->cursor_x += st->textsize * 6;          // Advance x one char
  }
}

void writeText(struct pi_device *device,char* str,int fontsize)
{
  st_t *st = (st_t *)device->data;
  st->textsize = fontsize;
  int i=0;
  while(str[i] != '\0')
    writeChar(device,str[i++]);
}

#endif