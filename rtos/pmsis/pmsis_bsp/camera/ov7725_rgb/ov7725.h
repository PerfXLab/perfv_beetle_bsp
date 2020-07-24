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

#ifndef __BSP_CAMERA_OV7725_OV7725_H__
#define __BSP_CAMERA_OV7725_OV7725_H__

enum {
  OV7725_STANDBY = 0x0,
  OV7725_STREAMING = 0x1,        // I2C triggered streaming enable
  OV7725_STREAMING2 = 0x3,       // Output N frames
  OV7725_STREAMING3 = 0x5        // Hardware Trigger
};


#endif
