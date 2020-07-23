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
#include "bsp/display/yt280L030.h"
#include "bsp/bsp.h"
#include "yt280L030.h"


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
} yt280_t;

#if 1

static void __yt280_init(yt280_t *yt280);
static void __yt280_set_rotation(yt280_t *yt280, uint8_t m);
static void __yt280_set_addr_window(yt280_t *yt280,uint16_t x, uint16_t y, uint16_t w, uint16_t h) ;
static void __yt280_write_8(yt280_t *yt280, uint8_t  d);
static void __yt280_write_command(yt280_t *yt280,uint8_t cmd);
static void __yt280_color_to_rgb565(uint8_t *input,uint16_t *output,int width, int height);


static uint8_t setWin_flag =1;


static void __yt280_write_buffer_iter(void *arg)
{
	yt280_t *yt280 = (yt280_t *)arg;
	uint32_t size = yt280->current_size;

	if(size > yt280->current_line_len )
		size = yt280->current_line_len;

    pi_task_t *task = &yt280->temp_copy_task;
    pi_spi_flags_e flags = PI_SPI_CS_KEEP;	

	yt280->current_size -= size;			//remain len

    if (yt280->current_size == 0)
    {
        task = yt280->current_task;
        flags = PI_SPI_CS_AUTO;
    }
	yt280->temp_buff = yt280->current_data;

	yt280->current_data += size*2;
	
	pi_spi_send_async(&yt280->spim, yt280->temp_buff, size*2*8, flags, task);

}


/*
x: offset for x axis.
y: offset for y axis.
w: width for display on lcd.
h: hight for display on lcd.

buffer: the buffer for display.
*/
static void __yt280_write_async(struct pi_device *device, pi_buffer_t *buffer, uint16_t x, uint16_t y,uint16_t w, uint16_t h, pi_task_t *task)
{
  yt280_t *yt280 = (yt280_t *)device->data;

    __yt280_set_addr_window(yt280, x, y, w, h); // Clipped area

  pi_task_callback(&yt280->temp_copy_task, __yt280_write_buffer_iter, (void *)yt280);

  yt280->buffer = buffer;
  yt280->width = w;								
  yt280->current_task = task;			
  yt280->current_data = (uint8_t*)buffer->data;	//the buffer for display	
  yt280->current_size = w*h;						          //the total number for buffer
  yt280->current_line_len = w*(4096/w);				  //how many pixels for once. udma_SPI max size is 8192. yt280t7735s is RGB565. so 8192/2=4096

  __yt280_write_buffer_iter(yt280);
}

static int __yt280_open(struct pi_device *device)
{
  struct pi_yt280_conf *conf = (struct pi_yt280_conf *)device->config;

  yt280_t *yt280 = (yt280_t *)pmsis_l2_malloc(sizeof(yt280_t));
  if (yt280 == NULL) return -1;

  if (bsp_yt280_open(conf))
    goto error;

  struct pi_gpio_conf gpio_conf;
  pi_gpio_conf_init(&gpio_conf);

  pi_open_from_conf(&yt280->gpio_port, &gpio_conf);

  if (pi_gpio_open(&yt280->gpio_port))
    goto error;

  device->data = (void *)yt280;

  yt280->gpio = conf->gpio;
  yt280->gpio_rst = conf->gpio_rst;

  struct pi_spi_conf spi_conf;
  pi_spi_conf_init(&spi_conf);
  spi_conf.itf = conf->spi_itf;
  spi_conf.cs = conf->spi_cs;

  spi_conf.wordsize = PI_SPI_WORDSIZE_8;
  spi_conf.big_endian = 1;
  //spi_conf.max_baudrate = 50000000;
  spi_conf.max_baudrate = 50000000;
  spi_conf.polarity = 0;
  spi_conf.phase = 0;

  pi_open_from_conf(&yt280->spim, &spi_conf);

  if (pi_spi_open(&yt280->spim))
    goto error;

  __yt280_init(yt280);
  yt280->_width = YT280_TFTWIDTH;         //may be to swap width and high
  yt280->_height = YT280_TFTHEIGHT;

  yt280->cursor_x = 0;
  yt280->cursor_y = 0;
  yt280->wrap = 0;
  yt280->textsize = 1;
  yt280->textcolor = YT280_GREEN;
  yt280->textbgcolor = YT280_WHITE;

  return 0;

error:
  pmsis_l2_malloc_free(yt280, sizeof(yt280_t));
  return -1;
}

static int32_t __yt280_ioctl(struct pi_device *device, uint32_t cmd, void *arg)
{
  yt280_t *yt280 = (yt280_t *)device->data;

  switch (cmd)
  {
    case PI_YT280_IOCTL_ORIENTATION:
        __yt280_set_rotation(yt280, (uint8_t)(long)arg);
    return 0;
  }
  return -1;
}

static pi_display_api_t yt280_api =
{
  .open           = &__yt280_open,
  .write_async    = &__yt280_write_async,
  .ioctl          = &__yt280_ioctl
};

void pi_yt280_conf_init(struct pi_yt280_conf *conf)
{
  conf->display.api = &yt280_api;
  conf->spi_itf = 0;
  conf->skip_pads_config = 0;
  __display_conf_init(&conf->display);
  bsp_yt280_conf_init(conf);
}

static void __yt280_write_8(yt280_t *yt280, uint8_t value)
{
  yt280->temp_buffer[0] = value;
  pi_spi_send(&yt280->spim, yt280->temp_buffer, 8, PI_SPI_CS_AUTO);
}

static void __yt280_write_16(yt280_t *yt280, uint16_t value)
{
  __yt280_write_8(yt280, value >> 8);
  __yt280_write_8(yt280, value);
}

static void __yt280_write_32(yt280_t *yt280, uint32_t value)
{
  __yt280_write_16(yt280, value >> 16);
  __yt280_write_16(yt280, value);
}

static void __yt280_write_command(yt280_t *yt280, uint8_t cmd)
{
  pi_gpio_pin_write(&yt280->gpio_port, yt280->gpio, 0);
  __yt280_write_8(yt280,cmd);
  pi_gpio_pin_write(&yt280->gpio_port, yt280->gpio, 1);
}


static void __yt280_init(yt280_t *yt280)
{
	pi_gpio_pin_configure(&yt280->gpio_rst_port, yt280->gpio_rst, PI_GPIO_OUTPUT);
	pi_gpio_pin_configure(&yt280->gpio_port, yt280->gpio, PI_GPIO_OUTPUT);

	pi_gpio_pin_write(&yt280->gpio_rst_port, yt280->gpio_rst, 1);
	pi_time_wait_us(200000);
	pi_gpio_pin_write(&yt280->gpio_rst_port, yt280->gpio_rst, 0);
	pi_time_wait_us(800000);
	pi_gpio_pin_write(&yt280->gpio_rst_port, yt280->gpio_rst, 1);
	pi_time_wait_us(800000);

	pi_gpio_pin_write(&yt280->gpio_port, yt280->gpio, 0);

	__yt280_write_command(yt280,0xCF);
	__yt280_write_8(yt280,0x00);
	__yt280_write_8(yt280,0xD9);
	__yt280_write_8(yt280,0x30);

	__yt280_write_command(yt280,0xED);
	__yt280_write_8(yt280,0x64);
	__yt280_write_8(yt280,0x03);
	__yt280_write_8(yt280,0x12);
	__yt280_write_8(yt280,0x81);

	__yt280_write_command(yt280,0xE8);
	__yt280_write_8(yt280,0x85);
	__yt280_write_8(yt280,0x10);
	__yt280_write_8(yt280,0x78);

	__yt280_write_command(yt280,0xCB);
	__yt280_write_8(yt280,0x39);
	__yt280_write_8(yt280,0x2C);
	__yt280_write_8(yt280,0x00);
	__yt280_write_8(yt280,0x34);
	__yt280_write_8(yt280,0x02);

	__yt280_write_command(yt280,0xF7);
	__yt280_write_8(yt280,0x20);

	__yt280_write_command(yt280,0xEA);
	__yt280_write_8(yt280,0x00);
	__yt280_write_8(yt280,0x00);

	__yt280_write_command(yt280,0xc0);
	__yt280_write_8(yt280,0x21);

	__yt280_write_command(yt280,0xc1);
	__yt280_write_8(yt280,0x12);

	__yt280_write_command(yt280,0xc5);
	__yt280_write_8(yt280,0x32);
	__yt280_write_8(yt280,0x3c);

	__yt280_write_command(yt280,0xc7);
	__yt280_write_8(yt280,0xc1);

	__yt280_write_command(yt280,0x36);
	__yt280_write_8(yt280,0x08);

	__yt280_write_command(yt280,0x3A);
	__yt280_write_8(yt280,0x55);

	__yt280_write_command(yt280,0xb1);
	__yt280_write_8(yt280,0x00);
	__yt280_write_8(yt280,0x18);

	__yt280_write_command(yt280,0xb6);
	__yt280_write_8(yt280,0x0a);
	__yt280_write_8(yt280,0xA2);

	__yt280_write_command(yt280,0xf2);
	__yt280_write_8(yt280,0x00);

	__yt280_write_command(yt280,0x26);
	__yt280_write_8(yt280,0x01);

	__yt280_write_command(yt280,0xE0);
	__yt280_write_8(yt280,0x0f);
	__yt280_write_8(yt280,0x2D);
	__yt280_write_8(yt280,0x1e);
	__yt280_write_8(yt280,0x09);
	__yt280_write_8(yt280,0x12);
	__yt280_write_8(yt280,0x0b);
	__yt280_write_8(yt280,0x50);
	__yt280_write_8(yt280,0xba);
	__yt280_write_8(yt280,0x44);
	__yt280_write_8(yt280,0x09);
	__yt280_write_8(yt280,0x14);
	__yt280_write_8(yt280,0x05);
	__yt280_write_8(yt280,0x23);
	__yt280_write_8(yt280,0x21);
	__yt280_write_8(yt280,0x00);

	__yt280_write_command(yt280,0xE1);
	__yt280_write_8(yt280,0x00);
	__yt280_write_8(yt280,0x19);
	__yt280_write_8(yt280,0x19);
	__yt280_write_8(yt280,0x00);
	__yt280_write_8(yt280,0x12);
	__yt280_write_8(yt280,0x07);
	__yt280_write_8(yt280,0x2d);
	__yt280_write_8(yt280,0x28);
	__yt280_write_8(yt280,0x3f);
	__yt280_write_8(yt280,0x02);
	__yt280_write_8(yt280,0x0a);
	__yt280_write_8(yt280,0x08);
	__yt280_write_8(yt280,0x25);
	__yt280_write_8(yt280,0x2d);
	__yt280_write_8(yt280,0x0F);

	__yt280_write_command(yt280,0x11);
	pi_time_wait_us(120000);
	__yt280_write_command(yt280,0x29);

}

static void __yt280_set_rotation(yt280_t *yt280, uint8_t m)
{
  int rotation = m % 4; // can't be higher than 3
  switch (rotation)
  {
    case 0:
      m = (MADCTL_MX | MADCTL_BGR);     //0x40 | 0x08
      yt280->_width  = YT280_TFTWIDTH;
      yt280->_height = YT280_TFTHEIGHT;
      break;

    case 1:
      m = ( MADCTL_MY | MADCTL_MV  | MADCTL_BGR);     //OK
      //m = ( MADCTL_MX | MADCTL_MY  | MADCTL_BGR);       //ok
      //m = ( MADCTL_MX | MADCTL_MV  | MADCTL_BGR);     //ok
      yt280->_width  = YT280_TFTHEIGHT;
      yt280->_height = YT280_TFTWIDTH;
      break;

    case 2:
      m = (MADCTL_MY | MADCTL_BGR);     //0x80 | 0x08
      yt280->_width  = YT280_TFTWIDTH;
      yt280->_height = YT280_TFTHEIGHT;
      break;

    case 3:
      m = (MADCTL_MX | MADCTL_MY | MADCTL_MV | MADCTL_BGR); //0x40 | 0x80 | 0x20 | 0x08
      yt280->_width  = YT280_TFTHEIGHT;
      yt280->_height = YT280_TFTWIDTH;
      break;
  }

  __yt280_write_command(yt280, YT280_MADCTL);   //0x36
  __yt280_write_8(yt280, m);
}


static void __yt280_set_addr_window(yt280_t *yt280,uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
  uint32_t xa = ((uint32_t)(x) << 16) | (x+w-1);
  uint32_t ya = ((uint32_t)y << 16) | (y+h-1);
  __yt280_write_command(yt280,YT280_CASET); // Column addr set
  __yt280_write_32(yt280,xa);
  __yt280_write_command(yt280,YT280_PASET); // Row addr set
  __yt280_write_32(yt280,ya);
  __yt280_write_command(yt280,YT280_RAMWR); // write to RAM
}


void writeColor(struct pi_device *device, uint16_t color, unsigned int len)
{
  yt280_t *yt280 = (yt280_t *)device->data;
  for (uint32_t t=0; t<len; t++)
  {
    __yt280_write_16(yt280, color);
  }
  return;
}

void writeFillRect(struct pi_device *device, unsigned short x, unsigned short y, unsigned short w, unsigned short h, unsigned short color)
{
  yt280_t *yt280 = (yt280_t *)device->data;

  if((x >= yt280->_width) || (y >= yt280->_height)) return;
  unsigned short x2 = x + w - 1, y2 = y + h - 1;

  // Clip right/bottom
  if(x2 >= yt280->_width)  w = yt280->_width  - x;
  if(y2 >= yt280->_height) h = yt280->_height - y;
  unsigned int len = (int32_t)w * h;

  //setAddrWindow(spim,x, y, w, h);
  __yt280_set_addr_window(yt280, x, y, w, h); // Clipped area

  writeColor(device, color, len);
}


void setTextWrap(struct pi_device *device,signed short w)
{
  yt280_t *yt280 = (yt280_t *)device->data;
  yt280->wrap = w;
}

void setCursor(struct pi_device *device,signed short x, signed short y)
{
  yt280_t *yt280 = (yt280_t *)device->data;
  yt280->cursor_x = x;
  yt280->cursor_y = y;
}

void setTextColor(struct pi_device *device,uint16_t c)
{
  yt280_t *yt280 = (yt280_t *)device->data;
  // For 'transparent' background, we'll set the bg
  // to the same as fg inyt280ead of using a flag
  yt280->textcolor = c;
}

static void __yt280_writePixelAtPos(struct pi_device *device,int16_t x, int16_t y, uint16_t color)
{
  yt280_t *yt280 = (yt280_t *)device->data;

  if((x < 0) ||(x >= (int)yt280->_width) || (y < 0) || (y >= (int)yt280->_height)) return;

  __yt280_set_addr_window(yt280, x, y, 1, 1); // Clipped area
  __yt280_write_16(yt280,color);
}

static void drawChar(struct pi_device *device,int16_t x, int16_t y, unsigned char c,
  uint16_t color, uint16_t bg, uint8_t size)
{
  yt280_t *yt280 = (yt280_t *)device->data;

  if((x >= (int)yt280->_width)  || // Clip right
     (y >= (int)yt280->_height) || // Clip bottom
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
          __yt280_writePixelAtPos(device,x+i, y+j, color);
        else
          writeFillRect(device,x+i*size, y+j*size, size, size, color);
      }
      else if(bg != color)
      {
        if(size == 1)
          __yt280_writePixelAtPos(device,x+i, y+j, bg);
        else
          writeFillRect(device,x+i*size, y+j*size, size, size, bg);
      }
    }
  }
}


static void writeChar(struct pi_device *device,uint8_t c)
{
  yt280_t *yt280 = (yt280_t *)device->data;

  if(c == '\n')
  {                        // Newline?
    yt280->cursor_x  = 0;                     // Reset x to zero,
    yt280->cursor_y += yt280->textsize * 8;          // advance y one line
  }
  else if (c != '\r')
  {                 // Ignore carriage returns
    if (yt280->wrap && ((yt280->cursor_x + yt280->textsize * 6) > (int)yt280->_width))
    { // Off right?
      yt280->cursor_x  = 0;                 // Reset x to zero,
      yt280->cursor_y += yt280->textsize * 8;      // advance y one line
    }
    drawChar(device, yt280->cursor_x, yt280->cursor_y, c, yt280->textcolor, yt280->textbgcolor, yt280->textsize);
    yt280->cursor_x += yt280->textsize * 6;          // Advance x one char
  }
}

void writeText(struct pi_device *device,char* str,int fontsize)
{
  yt280_t *yt280 = (yt280_t *)device->data;
  yt280->textsize = fontsize;
  int i=0;
  while(str[i] != '\0')
    writeChar(device,str[i++]);
}

#endif
