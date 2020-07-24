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
#include "pmsis/drivers/spi.h"
#include "pmsis/drivers/gpio.h"
#include "bsp/display/z350it008.h"
#include "bsp/bsp.h"
#include "z350it008.h"

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
  uint8_t temp_buffer[Z350_TFTWIDTH*3];

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
} z350_t;

#if 1

static void __z350_init(z350_t *z350);
static void __z350_set_rotation(z350_t *z350, uint8_t m);
static void __z350_set_addr_window(z350_t *z350,uint16_t x, uint16_t y, uint16_t w, uint16_t h) ;
static void __z350_write_8(z350_t *z350, uint8_t  d);
static void __z350_write_command(z350_t *z350,uint8_t cmd);
static void __z350_color_to_rgb565(uint8_t *input,uint16_t *output,int width, int height);
static void __z350_write_RGB565_to_RGB666(z350_t *z350, uint16_t value);

static void RGB565_to_RGB666(uint16_t *input,uint8_t *output,int width, int height);


static void RGB565_to_RGB666(uint16_t *input,uint8_t *output,int width, int height)
{
  for(int i=0;i<width*height;i++)
  {
    output[i*3] = ( ( (input[i] >> 11) & 0x1F  ) << 3);
    output[i*3+1] = (((input[i] >> 5) & 0x3F) << 2);
    output[i*3+2] = (((input[i] >> 0) & 0x1F) << 3) ;
  }
}

/*
uint8_t temp_buffer[Z350_TFTWIDTH*3];
*/
static void __z350_write_buffer_iter(void *arg)
{
	z350_t *z350 = (z350_t *)arg;
	uint32_t size = z350->current_size;

	if(size > z350->current_line_len )
		size = z350->current_line_len;

    pi_task_t *task = &z350->temp_copy_task;
    pi_spi_flags_e flags = PI_SPI_CS_KEEP;	

	  z350->current_size -= size;			//remain len

    if (z350->current_size == 0)
    {
        task = z350->current_task;
        flags = PI_SPI_CS_AUTO;
    }

    RGB565_to_RGB666((uint16_t *)z350->current_data, (uint8_t *)z350->temp_buffer, size, 1);

	  z350->current_data += size*2;
	
	pi_spi_send_async(&z350->spim, z350->temp_buffer, size*3*8, flags, task);

}


/*
x: offset for x axis.
y: offset for y axis.
w: width for display on lcd.
h: hight for display on lcd.

buffer: the buffer for display.
*/
static void __z350_write_async(struct pi_device *device, pi_buffer_t *buffer, uint16_t x, uint16_t y,uint16_t w, uint16_t h, pi_task_t *task)
{
  z350_t *z350 = (z350_t *)device->data;

    __z350_set_addr_window(z350, x, y, w, h); // Clipped area

  pi_task_callback(&z350->temp_copy_task, __z350_write_buffer_iter, (void *)z350);

  z350->buffer = buffer;
  z350->width = w;
  z350->current_task = task;			
  z350->current_data = (uint8_t*)buffer->data;	//the buffer for display	
  z350->current_size = w*h;						          //the total number for buffer
  z350->current_line_len = w;

  __z350_write_buffer_iter(z350);
}


static int __z350_open(struct pi_device *device)
{
  struct pi_z350_conf *conf = (struct pi_z350_conf *)device->config;

  z350_t *z350 = (z350_t *)pmsis_l2_malloc(sizeof(z350_t));
  if (z350 == NULL) return -1;

  if (bsp_z350_open(conf))
    goto error;

  struct pi_gpio_conf gpio_conf;
  pi_gpio_conf_init(&gpio_conf);

  pi_open_from_conf(&z350->gpio_port, &gpio_conf);

  if (pi_gpio_open(&z350->gpio_port))
    goto error;

  device->data = (void *)z350;

  z350->gpio = conf->gpio;
  z350->gpio_rst = conf->gpio_rst; 

  struct pi_spi_conf spi_conf;
  pi_spi_conf_init(&spi_conf);
  spi_conf.itf = conf->spi_itf;
  spi_conf.cs = conf->spi_cs;

  spi_conf.wordsize = PI_SPI_WORDSIZE_8;
  spi_conf.big_endian = 1;
  //spi_conf.max_baudrate = 50000000;
  spi_conf.max_baudrate =   50000000;
  spi_conf.polarity = 0;
  spi_conf.phase = 0;

  pi_open_from_conf(&z350->spim, &spi_conf);

  if (pi_spi_open(&z350->spim))
    goto error;

  __z350_init(z350);
  z350->_width = Z350_TFTWIDTH;      
  z350->_height = Z350_TFTHEIGHT;

  z350->cursor_x = 0;
  z350->cursor_y = 0;
  z350->wrap = 0;
  z350->textsize = 1;
  z350->textcolor = Z350_GREEN;
  z350->textbgcolor = Z350_WHITE;

  return 0;

error:
  pmsis_l2_malloc_free(z350, sizeof(z350_t));
  return -1;
}

static int32_t __z350_ioctl(struct pi_device *device, uint32_t cmd, void *arg)
{
  z350_t *z350 = (z350_t *)device->data;

  switch (cmd)
  {
    case PI_Z350_IOCTL_ORIENTATION:
        __z350_set_rotation(z350, (uint8_t)(long)arg);
    return 0;
  }
  return -1;
}

static pi_display_api_t z350_api =
{
  .open           = &__z350_open,
  .write_async    = &__z350_write_async,
  .ioctl          = &__z350_ioctl
};

void pi_z350_conf_init(struct pi_z350_conf *conf)
{
  conf->display.api = &z350_api;
  conf->spi_itf = 0;
  conf->skip_pads_config = 0;
  __display_conf_init(&conf->display);
  bsp_z350_conf_init(conf);
}

static void __z350_write_8(z350_t *z350, uint8_t value)
{
  z350->temp_buffer[0] = value;
  pi_spi_send(&z350->spim, z350->temp_buffer, 8, PI_SPI_CS_AUTO);
}

static void __z350_write_16(z350_t *z350, uint16_t value)
{
  __z350_write_8(z350, value >> 8);
  __z350_write_8(z350, value);
}

static void __z350_write_32(z350_t *z350, uint32_t value)
{
  __z350_write_16(z350, value >> 16);
  __z350_write_16(z350, value);
}

static void __z350_write_RGB565_to_RGB666(z350_t *z350, uint16_t value)
{
  __z350_write_8(z350, (((value >> 11) & 0x1F) << 3));
  __z350_write_8(z350, (((value >> 5) & 0x3F) << 2) );
  __z350_write_8(z350, (((value >> 0) & 0x1F) << 3) );
}

static void __z350_write_command(z350_t *z350, uint8_t cmd)
{
  pi_gpio_pin_write(&z350->gpio_port, z350->gpio, 0);
  __z350_write_8(z350,cmd);
  pi_gpio_pin_write(&z350->gpio_port, z350->gpio, 1);
}

static void __z350_init(z350_t *z350)
{
	pi_gpio_pin_configure(&z350->gpio_rst_port, z350->gpio_rst, PI_GPIO_OUTPUT);
	pi_gpio_pin_configure(&z350->gpio_port, z350->gpio, PI_GPIO_OUTPUT);

	pi_gpio_pin_write(&z350->gpio_rst_port, z350->gpio_rst, 1);
	pi_time_wait_us(500000);
	pi_gpio_pin_write(&z350->gpio_rst_port, z350->gpio_rst, 0);
	pi_time_wait_us(500000);
	pi_gpio_pin_write(&z350->gpio_rst_port, z350->gpio_rst, 1);
	pi_time_wait_us(500000);
  pi_gpio_pin_write(&z350->gpio_port, z350->gpio, 0);
    
	__z350_write_command(z350,0xE0);
	__z350_write_8(z350,0x00);
	__z350_write_8(z350,0x07);
	__z350_write_8(z350,0x0f);
	__z350_write_8(z350,0x0D);
	__z350_write_8(z350,0x1B);
	__z350_write_8(z350,0x0A);
	__z350_write_8(z350,0x3c);
	__z350_write_8(z350,0x78);
	__z350_write_8(z350,0x4A);
	__z350_write_8(z350,0x07);
	__z350_write_8(z350,0x0E);
	__z350_write_8(z350,0x09);
	__z350_write_8(z350,0x1B);
	__z350_write_8(z350,0x1e);
	__z350_write_8(z350,0x0f);

	__z350_write_command(z350,0xE1);
	__z350_write_8(z350,0x00);
	__z350_write_8(z350,0x22);
	__z350_write_8(z350,0x24);
	__z350_write_8(z350,0x06);
	__z350_write_8(z350,0x12);
	__z350_write_8(z350,0x07);
	__z350_write_8(z350,0x36);
	__z350_write_8(z350,0x47);
	__z350_write_8(z350,0x47);
	__z350_write_8(z350,0x06);
	__z350_write_8(z350,0x0a);
	__z350_write_8(z350,0x07);
	__z350_write_8(z350,0x30);
	__z350_write_8(z350,0x37);
	__z350_write_8(z350,0x0f);

	__z350_write_command(z350,0xC0);
	__z350_write_8(z350,0x10);
	__z350_write_8(z350,0x10);

	__z350_write_command(z350,0xC1);
	__z350_write_8(z350,0x41);

	__z350_write_command(z350,0xC5);
	__z350_write_8(z350,0x00);
	__z350_write_8(z350,0x22);
	__z350_write_8(z350,0x80);

	__z350_write_command(z350,0x36);
	__z350_write_8(z350,0x48);

	__z350_write_command(z350,0x3A); //Interface Mode Control
	__z350_write_8(z350,0x66);        //z350 not suport RGB565 for spi mode.only RGB666 suport

	__z350_write_command(z350,0XB0); //Interface Mode Control  
	__z350_write_8(z350,0x00);
	__z350_write_command(z350,0xB1); //Frame rate 70HZ  
	__z350_write_8(z350,0xB0);
	__z350_write_8(z350,0x11);
	__z350_write_command(z350,0xB4);
	__z350_write_8(z350,0x02);
	__z350_write_command(z350,0xB6); //RGB/MCU Interface Control
	__z350_write_8(z350,0x02);
	__z350_write_8(z350,0x02);

	__z350_write_command(z350,0xB7);
	__z350_write_8(z350,0xC6);

	// __z350_write_command(z350,0XBE);
	// __z350_write_8(z350,0x00);
	// __z350_write_8(z350,0x04);

	__z350_write_command(z350,0xE9);
	__z350_write_8(z350,0x00);

	__z350_write_command(z350,0XF7);
	__z350_write_8(z350,0xA9);
	__z350_write_8(z350,0x51);
	__z350_write_8(z350,0x2C);
	__z350_write_8(z350,0x82);

	__z350_write_command(z350,0x11);
	pi_time_wait_us(500000);
	__z350_write_command(z350,0x29);
  
}

static void __z350_set_rotation(z350_t *z350, uint8_t m)
{
  int rotation = m % 4; // can't be higher than 3
  switch (rotation)
  {
    case 0:
      m = (MADCTL_MX | MADCTL_BGR);
      z350->_width  = Z350_TFTWIDTH;
      z350->_height = Z350_TFTHEIGHT;
      break;

    case 1:
      m = (MADCTL_MV | MADCTL_BGR);
      z350->_width  = Z350_TFTHEIGHT;
      z350->_height = Z350_TFTWIDTH;
      break;

    case 2:
      m = (MADCTL_MY | MADCTL_BGR);
      z350->_width  = Z350_TFTWIDTH;
      z350->_height = Z350_TFTHEIGHT;
      break;

    case 3:
      m = (MADCTL_MX | MADCTL_MY | MADCTL_MV | MADCTL_BGR);
      z350->_width  = Z350_TFTHEIGHT;
      z350->_height = Z350_TFTWIDTH;
      break;
  }

  __z350_write_command(z350, Z350_MADCTL);
  __z350_write_8(z350, m);
}


static void __z350_set_addr_window(z350_t *z350,uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
  uint32_t xa = ((uint32_t)(x) << 16) | (x+w-1);
  uint32_t ya = ((uint32_t)y << 16) | (y+h-1);
  __z350_write_command(z350,Z350_CASET); // Column addr set
  __z350_write_32(z350,xa);
  __z350_write_command(z350,Z350_PASET); // Row addr set
  __z350_write_32(z350,ya);
  __z350_write_command(z350,Z350_RAMWR); // write to RAM
}


void writeColor(struct pi_device *device, uint16_t color, unsigned int len)
{
  z350_t *z350 = (z350_t *)device->data;
  for (uint32_t t=0; t<len; t++)
  {
    __z350_write_RGB565_to_RGB666(z350, color);
  }
  return;
}

void writeFillRect(struct pi_device *device, unsigned short x, unsigned short y, unsigned short w, unsigned short h, unsigned short color)
{
  z350_t *z350 = (z350_t *)device->data;

  if((x >= z350->_width) || (y >= z350->_height)) return;
  unsigned short x2 = x + w - 1, y2 = y + h - 1;

  // Clip right/bottom
  if(x2 >= z350->_width)  w = z350->_width  - x;
  if(y2 >= z350->_height) h = z350->_height - y;
  unsigned int len = (int32_t)w * h;

  //setAddrWindow(spim,x, y, w, h);
  __z350_set_addr_window(z350, x, y, w, h); // Clipped area

  writeColor(device, color, len);
}


void setTextWrap(struct pi_device *device,signed short w)
{
  z350_t *z350 = (z350_t *)device->data;
  z350->wrap = w;
}

void setCursor(struct pi_device *device,signed short x, signed short y)
{
  z350_t *z350 = (z350_t *)device->data;
  z350->cursor_x = x;
  z350->cursor_y = y;
}

void setTextColor(struct pi_device *device,uint16_t c)
{
  z350_t *z350 = (z350_t *)device->data;
  // For 'transparent' background, we'll set the bg
  // to the same as fg inz350ead of using a flag
  z350->textcolor = c;
}

static void __z350_writePixelAtPos(struct pi_device *device,int16_t x, int16_t y, uint16_t color)
{
  z350_t *z350 = (z350_t *)device->data;

  if((x < 0) ||(x >= (int)z350->_width) || (y < 0) || (y >= (int)z350->_height)) return;

  __z350_set_addr_window(z350, x, y, 1, 1); // Clipped area
  __z350_write_16(z350,color);
}

static void drawChar(struct pi_device *device,int16_t x, int16_t y, unsigned char c,
  uint16_t color, uint16_t bg, uint8_t size)
{
  z350_t *z350 = (z350_t *)device->data;

  if((x >= (int)z350->_width)  || // Clip right
     (y >= (int)z350->_height) || // Clip bottom
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
          __z350_writePixelAtPos(device,x+i, y+j, color);
        else
          writeFillRect(device,x+i*size, y+j*size, size, size, color);
      }
      else if(bg != color)
      {
        if(size == 1)
          __z350_writePixelAtPos(device,x+i, y+j, bg);
        else
          writeFillRect(device,x+i*size, y+j*size, size, size, bg);
      }
    }
  }
}


static void writeChar(struct pi_device *device,uint8_t c)
{
  z350_t *z350 = (z350_t *)device->data;

  if(c == '\n')
  {                        // Newline?
    z350->cursor_x  = 0;                     // Reset x to zero,
    z350->cursor_y += z350->textsize * 8;          // advance y one line
  }
  else if (c != '\r')
  {                 // Ignore carriage returns
    if (z350->wrap && ((z350->cursor_x + z350->textsize * 6) > (int)z350->_width))
    { // Off right?
      z350->cursor_x  = 0;                 // Reset x to zero,
      z350->cursor_y += z350->textsize * 8;      // advance y one line
    }
    drawChar(device, z350->cursor_x, z350->cursor_y, c, z350->textcolor, z350->textbgcolor, z350->textsize);
    z350->cursor_x += z350->textsize * 6;          // Advance x one char
  }
}

void writeText(struct pi_device *device,char* str,int fontsize)
{
  z350_t *z350 = (z350_t *)device->data;
  z350->textsize = fontsize;
  int i=0;
  while(str[i] != '\0')
    writeChar(device,str[i++]);
}

#endif
