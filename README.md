# perfv_beetle_bsp

#################################################################

### 本 bsp 内容基于 greenwave gap8-sdk-v3.3.3 进行开发。

本 bsp 涉及到 4款lcd , 1款camera(分rgb和yuv) , 1款蓝牙模块 ，1款wifi模块， 这4块 配件。 

当您选择不同配件时，编译bsp之前，需要修改如下文件：

```
     src.mk        (pmsis/pmsis_bsp/src.mk)
     gapuino.h     (pmsis/pmsis_bsp/include/bsp/gapuino.h)
```



4款lcd 及对应的驱动模块如下：

```
不同lcd(尺寸)			分辨率			lcd屏对应的驱动模块			具体模块文件
0.91英寸				 80 x 160			st7735s						display/st7735s/st7735s.c
1.8英寸				128 x 160			st7735s						display/st7735s/st7735s.c
2.8英寸				240 x 320			yt280L030					display/yt280L030/yt280L030.c
3.5英寸				320 x 480			z350it008					display/z350it008/z350it008.c
```

注：

​		目前移植的人工智能的demo，都是基于 2.8英寸的lcd屏。



### 不同配件不同配置

举例：
  使用配件：2.8英寸的lcd 和 camera(rgb).
  如下配置：
  	**src.mk:**

```
  ...
  #SP_LCD_Z350_SRC = display/z350it008/z350it008.c
  BSP_LCD_YT280_SRC = display/yt280L030/yt280L030.c					//放开本行(2.8英寸lcd对应 yt280L030驱动模块)
  #BSP_LCD_ST7735S_SRC = display/st7735s/st7735s.c					
  #BSP_LCD_ILI9341_SRC = display/ili9341/ili9341.c
  #BSP_HC08_SRC = transport/transport.c ble/ble.c ble/hc08/hc08.c
  #BSP_CAMERA_OV7725_SRC = camera/ov7725_yuv/ov7725.c
  BSP_CAMERA_OV7725_SRC = camera/ov7725_rgb/ov7725.c				//放开本行(camera 输入rgb565视频流)
  ...
```

​      **gapuino.h:**

```
#define CONFIG_OV7725												//放开本行
//#define CONFIG_HIMAX

//#define CONFIG_NINA_W10
//#define CONFIG_HC08

//#define CONFIG_Z350
#define CONFIG_YT280												//放开本行
//#define CONFIG_ST7735S												
//#define CONFIG_ILI9341
```

您使用哪些配件就 放开哪些配件的对应行。src.mk 和 gapuino.h 两个文件必须同时放开，否则编译出错。

注：

​		src.mk 文件中的如下两行：

```
 		BSP_CAMERA_OV7725_SRC = camera/ov7725_yuv/ov7725.c
  		BSP_CAMERA_OV7725_SRC = camera/ov7725_rgb/ov7725.c
```

​		实际都是camera配件的驱动，只是一个驱动模块让camera输出yuv格式的视频流（主要为人脸识别等人工智能使用）。而另一个驱动模块让camera输出RGB565格式的视频流



### 特别注意：

**在使用0.91英寸lcd 和 1.8英寸lcd时，需要修改三个文件：**

```
     src.mk        (pmsis/pmsis_bsp/src.mk)
     gapuino.h     (pmsis/pmsis_bsp/include/bsp/gapuino.h)
     st7735s.h	   (pmsis/pmsis_bsp/display/st7735s/st7735s.h)			//仅0.91和1.8英寸的lcd才需要修改
```

具体使用方法：

**(1) 使用配件：0.91英寸的lcd 和 camera(rgb).**

配置如下：

  	**src.mk:**

```
  ...
  #SP_LCD_Z350_SRC = display/z350it008/z350it008.c
  #BSP_LCD_YT280_SRC = display/yt280L030/yt280L030.c					
  BSP_LCD_ST7735S_SRC = display/st7735s/st7735s.c					//放开本行(0.91英寸lcd对应 st7735s驱动模块)
  #BSP_LCD_ILI9341_SRC = display/ili9341/ili9341.c
  #BSP_HC08_SRC = transport/transport.c ble/ble.c ble/hc08/hc08.c
  #BSP_CAMERA_OV7725_SRC = camera/ov7725_yuv/ov7725.c
  BSP_CAMERA_OV7725_SRC = camera/ov7725_rgb/ov7725.c				//放开本行(camera 输入rgb565视频流)
  ...
```

​      **gapuino.h:**

```
#define CONFIG_OV7725												//放开本行
//#define CONFIG_HIMAX

//#define CONFIG_NINA_W10
//#define CONFIG_HC08

//#define CONFIG_Z350
//#define CONFIG_YT280
#define CONFIG_ST7735S												//放开本行
//#define CONFIG_ILI9341
```

​		**st7735s.h:**

```
#define  ST7735S_80X160          //如果使用0.91英寸lcd屏，请打开这个。如果使用1.8英寸屏请屏蔽这个

#ifdef ST7735S_80X160
#define ST7735S_TFTWIDTH   80       // ST7735S max TFT width
#define ST7735S_TFTHEIGHT  160       // ST7735S max TFT height
#else	/* ST7735S_128X160 */
#define ST7735S_TFTWIDTH   128       // ST7735S max TFT width
#define ST7735S_TFTHEIGHT  160       // ST7735S max TFT height
#endif
```



**(2) 使用配件：1.8英寸的lcd 和 camera(rgb).**

配置如下：

  	**src.mk:**

```
  ...
  #SP_LCD_Z350_SRC = display/z350it008/z350it008.c
  #BSP_LCD_YT280_SRC = display/yt280L030/yt280L030.c					
  BSP_LCD_ST7735S_SRC = display/st7735s/st7735s.c					//放开本行(1.8英寸lcd对应 st7735s驱动模块)
  #BSP_LCD_ILI9341_SRC = display/ili9341/ili9341.c
  #BSP_HC08_SRC = transport/transport.c ble/ble.c ble/hc08/hc08.c
  #BSP_CAMERA_OV7725_SRC = camera/ov7725_yuv/ov7725.c
  BSP_CAMERA_OV7725_SRC = camera/ov7725_rgb/ov7725.c				//放开本行(camera 输入rgb565视频流)
  ...
```

​      **gapuino.h:**

```
#define CONFIG_OV7725												//放开本行
//#define CONFIG_HIMAX

//#define CONFIG_NINA_W10
//#define CONFIG_HC08

//#define CONFIG_Z350
//#define CONFIG_YT280
#define CONFIG_ST7735S												//放开本行
//#define CONFIG_ILI9341
```

​		**st7735s.h:**

```
//#define  ST7735S_80X160          //如果使用0.91英寸lcd屏，请打开这个。如果使用1.8英寸屏请屏蔽这个

#ifdef ST7735S_80X160
#define ST7735S_TFTWIDTH   80       // ST7735S max TFT width
#define ST7735S_TFTHEIGHT  160       // ST7735S max TFT height
#else	/* ST7735S_128X160 */
#define ST7735S_TFTWIDTH   128       // ST7735S max TFT width
#define ST7735S_TFTHEIGHT  160       // ST7735S max TFT height
#endif
```



#################################################################
