/*
 * ox03c_mode_tbls.h - ox03c sensor mode tables
 *
 * Copyright (c) 2018-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __OX03C_I2C_TABLES__
#define __OX03C_I2C_TABLES__

#include <media/camera_common.h>

#define TABLE_WAIT_MS	0xff00
#define TABLE_END	    0xff01

#define isx028_SENSOR_ADDR    0x34
#define SENSOR_ADDR    0x6c
#define MAX96712_ADDR  0x52	// Update to 0x29 << 1 == 0x52
#define MAX96717_ADDR  0x80

#define sensor_reg struct reg_8

struct index_reg_8 {
	u16 source;
	u16 addr;
	u8 val;
};

static struct index_reg_8 sensor_start[] = {
	{0x00, TABLE_END, 0x00}
};

static struct index_reg_8 sensor_stop[] = {
	{0x00,TABLE_END, 0x00}
};

static struct index_reg_8 max96712_gmsl2_6G_tbl[] = {
	{MAX96712_ADDR, 0x040b, 0x00}, //csi output disable
	//{MAX96712_ADDR, 0x0006, 0xff},// Enable all 4 Links in GMSL2 mode
	{TABLE_WAIT_MS, 0x0000, 100},
	{MAX96712_ADDR, 0x0010, 0x22},//--|
	{MAX96712_ADDR, 0x0011, 0x22},//--|work in 6G mode
	{TABLE_WAIT_MS, 0x0000, 200},

	{MAX96712_ADDR, 0x00f0, 0x62},// pipe X in link B to video pipe 1, pipe X in link A to video pipe 0
	{MAX96712_ADDR, 0x00f1, 0xEA},// pipe X in link D to video pipe 3, pipe X in link C to video pipe 2
	{MAX96712_ADDR, 0x00f4, 0x0F},// Turn on 4 pipes
	
	// Efficiency updates (disable HEARTBEAT Mode) for image data pipes 0-3 
	{MAX96712_ADDR, 0x0106, 0x0a},
	{MAX96712_ADDR, 0x0118, 0x0a},
	{MAX96712_ADDR, 0x012a, 0x0a},
	{MAX96712_ADDR, 0x013c, 0x0a},

	{MAX96712_ADDR, 0x090B, 0x07}, // Enable 3 mappings  Pipe 0//video2
	{MAX96712_ADDR, 0x092D, 0x15}, // All mappings to controller 1 (port A)
	{MAX96712_ADDR, 0x090D, 0x1E}, // Input RAW12, VC0
	{MAX96712_ADDR, 0x090E, 0x1E}, // Output RAW12, VC0
	{MAX96712_ADDR, 0x090F, 0x00}, // Input FS, VC0
	{MAX96712_ADDR, 0x0910, 0x00}, // Output FS, VC0
	{MAX96712_ADDR, 0x0911, 0x01}, // Input FE, VC0
	{MAX96712_ADDR, 0x0912, 0x01}, // Output FE, VC0

	{MAX96712_ADDR, 0x094B, 0x07}, // Enable 3 mappings  Pipe 1  //video3
	{MAX96712_ADDR, 0x096D, 0x15}, // All mappings to controller 1 (port A)
	{MAX96712_ADDR, 0x094D, 0x1E}, // Input RAW12, VC0
	{MAX96712_ADDR, 0x094E, 0x5E}, // Output RAW12, VC1
	{MAX96712_ADDR, 0x094F, 0x00}, // Input FS, VC0
	{MAX96712_ADDR, 0x0950, 0x40}, // Output FS, VC1
	{MAX96712_ADDR, 0x0951, 0x01}, // Input FE, VC0
	{MAX96712_ADDR, 0x0952, 0x41}, // Output FE, VC1

	{MAX96712_ADDR, 0x098B, 0x07}, // Enable 3 mappings Pipe 2  //video1
	{MAX96712_ADDR, 0x09AD, 0x15}, // All mappings to controller 1 (port A)
	{MAX96712_ADDR, 0x098D, 0x1E}, // Input RAW12, VC0
	{MAX96712_ADDR, 0x098E, 0x9E}, // Output RAW12, VC2
	{MAX96712_ADDR, 0x098F, 0x00}, // Input FS, VC0
	{MAX96712_ADDR, 0x0990, 0x80}, // Output FS, VC2
	{MAX96712_ADDR, 0x0991, 0x01}, // Input FE, VC0
	{MAX96712_ADDR, 0x0992, 0x81}, // Output FE, VC2

	{MAX96712_ADDR, 0x09CB, 0x07}, // Enable 3 mappings  Pipe 3  //video0
	{MAX96712_ADDR, 0x09ED, 0x15}, // All mappings to controller 1 (port A)
	{MAX96712_ADDR, 0x09CD, 0x1E}, // Input RAW12, VC0
	{MAX96712_ADDR, 0x09CE, 0xDE}, // Output RAW12, VC3
	{MAX96712_ADDR, 0x09CF, 0x00}, // Input FS, VC0
	{MAX96712_ADDR, 0x09D0, 0xC0}, // Output FS, VC3
	{MAX96712_ADDR, 0x09D1, 0x01}, // Input FE, VC0
	{MAX96712_ADDR, 0x09D2, 0xC1}, // Output FE, VC3
	{TABLE_WAIT_MS, 0x0000, 200},

	{MAX96712_ADDR, 0x08A0, 0x04}, // CSI output is 2x4
	{MAX96712_ADDR, 0x08A3, 0xE4}, // Set Lane Mapping for 4-lane port A
	{MAX96712_ADDR, 0x08A4, 0xE4}, // Default 4x2 lane mapping

	{MAX96712_ADDR, 0x090A, 0xC0}, // Set 4 lane D-PHY      默认值
	{MAX96712_ADDR, 0x094A, 0xC0},
	{MAX96712_ADDR, 0x098A, 0xC0},
	{MAX96712_ADDR, 0x09CA, 0xC0},
	
	{MAX96712_ADDR, 0x08A2, 0xF0}, // Turn on MIPI PH 

	{MAX96712_ADDR, 0x0415, 0x37}, // Set Data rate to be 2300Mbps/lane for port A and enable software override
	{MAX96712_ADDR, 0x0418, 0x37}, // Set Data rate to be 2300Mbps/lane for port B and enable software override
	{MAX96712_ADDR, 0x041b, 0x37}, // Set Data rate to be 2300Mbps/lane for port B and enable software override
	{MAX96712_ADDR, 0x041e, 0x37}, // Set Data rate to be 2300Mbps/lane for port B and enable software override
	
	{TABLE_WAIT_MS, 0x0000, 100},
	{0x00, TABLE_END, 0x00 }
};

static struct index_reg_8 sensor_3840x2160_crop_30fps[] = {
	{0x00, TABLE_END, 0x00},
};

//config max96712 gmsl1 x4 96705 init
static struct index_reg_8 max96712_gmsl1_tbl[] = {
//disable MIPI output
	{MAX96712_ADDR, 0x040B, 0x00},
//disable link
	{MAX96712_ADDR, 0x0006, 0x00},
//Set 3Gbps link Rx rate for GMSL1
	{MAX96712_ADDR, 0x0010, 0x22}, // change 0x11
	{MAX96712_ADDR, 0x0011, 0x22}, // change 0x11
//config linkA as high-HIM mode
	{MAX96712_ADDR, 0x0B06, 0xEF},
	{MAX96712_ADDR, 0x0C06, 0xEF},
	{MAX96712_ADDR, 0x0D06, 0xEF},
	{MAX96712_ADDR, 0x0E06, 0xEF},
//enable linkA local-ack
	{MAX96712_ADDR, 0x0B0D, 0x81},
	{MAX96712_ADDR, 0x0C0D, 0x81},
	{MAX96712_ADDR, 0x0D0D, 0x81},
	{MAX96712_ADDR, 0x0E0D, 0x81},
//One-shot link reset for all
	{MAX96712_ADDR, 0x0018, 0x0F},
	{TABLE_WAIT_MS, 0x0000, 100},
	//cai->shenyun 10->14
	{MAX96712_ADDR, 0x0017, 0x10},
	{TABLE_WAIT_MS, 0x0000, 100},
	//{MAX96712_TABLE_WAIT_MS, 100},
	//{MAX96712_ADDR, 0x0019, 0x94},
	//{MAX96712_TABLE_WAIT_MS, 100},
//Disable HS DE processing 
	{MAX96712_ADDR, 0x0B0F, 0x01},
	{MAX96712_ADDR, 0x0C0F, 0x01},
	{MAX96712_ADDR, 0x0D0F, 0x01},
	{MAX96712_ADDR, 0x0E0F, 0x01},
//Enable Pipe 0 and 1
	{MAX96712_ADDR, 0x00F4, 0x0F},
//YUYV422 software override for all pipes since connected GMSL1 is under parallel mode
	{MAX96712_ADDR, 0x040B, 0x40},
	{MAX96712_ADDR, 0x040C, 0x00},
	{MAX96712_ADDR, 0x040D, 0x00},
	{MAX96712_ADDR, 0x040E, 0x5E},
	{MAX96712_ADDR, 0x040F, 0x7E},
//write erro
	{MAX96712_ADDR, 0x0410, 0x7A},
	{MAX96712_ADDR, 0x0411, 0x48},
	{MAX96712_ADDR, 0x0412, 0x20},
// //Turn on the mux mode
 	{MAX96712_ADDR, 0x041A, 0x00}, ///change --> 0x0f
//Set Des in 2x4 mode
	{MAX96712_ADDR, 0x08A0, 0x04},
//Set Lane Mapping for 4-lane port A
	{MAX96712_ADDR, 0x08A3, 0xE4},
	{MAX96712_ADDR, 0x08A4, 0xE4},
//Set 4 lane D-PHY
	{MAX96712_ADDR, 0x090A, 0xC0},
	{MAX96712_ADDR, 0x0944, 0xC0},
	{MAX96712_ADDR, 0x0984, 0xC0},
	{MAX96712_ADDR, 0x09C4, 0xC0},
//Turn on MIPI PHYs
	{MAX96712_ADDR, 0x08A2, 0xF0},
//Set Data rate to be 1400Mbps/lane for port A and enable software override
	{MAX96712_ADDR, 0x0415, 0xED},
	{MAX96712_ADDR, 0x0418, 0xED},
//Video Pipe to MIPI Controller Mapping YUV422, video pipe 0, map FS/FE
	{MAX96712_ADDR, 0x090B, 0x07},
	{MAX96712_ADDR, 0x092D, 0x15},
	{MAX96712_ADDR, 0x090D, 0x1E},
	{MAX96712_ADDR, 0x090E, 0x1E},
	{MAX96712_ADDR, 0x090F, 0x00},
	{MAX96712_ADDR, 0x0910, 0x00},
	{MAX96712_ADDR, 0x0911, 0x01},
	{MAX96712_ADDR, 0x0912, 0x01},
	{MAX96712_ADDR, 0x092D, 0x2A},

	{MAX96712_ADDR, 0x094B, 0x07},
	{MAX96712_ADDR, 0x096D, 0x15},
	{MAX96712_ADDR, 0x094D, 0x1E},
	{MAX96712_ADDR, 0x094E, 0x5E},
	{MAX96712_ADDR, 0x094F, 0x00},
	{MAX96712_ADDR, 0x0950, 0x40},
	{MAX96712_ADDR, 0x0951, 0x01},
	{MAX96712_ADDR, 0x0952, 0x41},
	{MAX96712_ADDR, 0x096D, 0x2A},

	{MAX96712_ADDR, 0x098B, 0x07},
	{MAX96712_ADDR, 0x09AD, 0x15},
	{MAX96712_ADDR, 0x098D, 0x1E},
	{MAX96712_ADDR, 0x098E, 0x9E},
	{MAX96712_ADDR, 0x098F, 0x00},
	{MAX96712_ADDR, 0x0990, 0x80},
	{MAX96712_ADDR, 0x0991, 0x01},
	{MAX96712_ADDR, 0x0992, 0x81},
	{MAX96712_ADDR, 0x09AD, 0x2A},

	{MAX96712_ADDR, 0x09CB, 0x07},
	{MAX96712_ADDR, 0x09ED, 0x15},
	{MAX96712_ADDR, 0x09CD, 0x1E},
	{MAX96712_ADDR, 0x09CE, 0xDE},
	{MAX96712_ADDR, 0x09CF, 0x00},
	{MAX96712_ADDR, 0x09D0, 0xC0},
	{MAX96712_ADDR, 0x09D1, 0x01},
	{MAX96712_ADDR, 0x09D2, 0xC1},
	{MAX96712_ADDR, 0x09ED, 0x2A},
//Enable HVEN, HIBW, DBL on MAX96712 depends on applications bit7:double-input mode, bit2: HVEN enable hs/vs encoding
	{MAX96712_ADDR, 0x0B07, 0x84},
	{MAX96712_ADDR, 0x0C07, 0x84},
	{MAX96712_ADDR, 0x0D07, 0x84},
	{MAX96712_ADDR, 0x0E07, 0x84},

	{MAX96712_ADDR, 0x040b, 0x42},
	{MAX96712_ADDR, 0x08a0, 0x84},

	{TABLE_WAIT_MS, 0x0000, 200},
	//GMSL2
	{MAX96712_ADDR, 0x00f0, 0x62},// pipe X in link B to video pipe 1, pipe X in link A to video pipe 0
	{MAX96712_ADDR, 0x00f1, 0xEA},// pipe X in link D to video pipe 3, pipe X in link C to video pipe 2
	{MAX96712_ADDR, 0x00f4, 0x0F},// Turn on 4 pipes
	
	// Efficiency updates (disable HEARTBEAT Mode) for image data pipes 0-3 
	{MAX96712_ADDR, 0x0106, 0x0a},
	{MAX96712_ADDR, 0x0118, 0x0a},
	{MAX96712_ADDR, 0x012a, 0x0a},
	{MAX96712_ADDR, 0x013c, 0x0a},
	
	{0x00, TABLE_END, 0x00}
};
static struct index_reg_8 isx028c_ES3_3840x2160_crop_30fps[] = {
	{SENSOR_ADDR, 0xFFFF, 0x00,},
	{TABLE_WAIT_MS, 0x0000, 20},//delay 20ms
	{SENSOR_ADDR, 0xB6F1,0x54},
	{SENSOR_ADDR, 0x95C1,0x01},
	{TABLE_WAIT_MS, 0x0000, 20},//delay 20ms
	{SENSOR_ADDR, 0x1B04,0x5C},
	{SENSOR_ADDR, 0x1B04,0xA3},
	{TABLE_WAIT_MS, 0x0000, 20},//delay 20ms
	{SENSOR_ADDR, 0xFFFF, 0x05},
	{TABLE_WAIT_MS, 0x0000, 20},//delay 20ms
	{SENSOR_ADDR, 0xB700,0x10},
	{SENSOR_ADDR, 0x95C1,0x01},
	{TABLE_WAIT_MS, 0x0000, 20},//delay 20ms
	{SENSOR_ADDR, 0xB6F1,0x55},
	{SENSOR_ADDR, 0x95C1,0x01},
	{TABLE_WAIT_MS, 0x0000, 20},//delay 20ms

};
//ISX028C 使用flash时配置
static struct index_reg_8 isx028c_ES2_3840x2160_crop_30fps[] = {
	{SENSOR_ADDR, 0x9730, 0xA5},
	{SENSOR_ADDR, 0xFFFF, 0x10},
	{TABLE_WAIT_MS, 0x0000, 3},//delay 3ms
	{SENSOR_ADDR, 0xB3F4, 0x6E},
	{SENSOR_ADDR, 0xB3F5, 0x00},
	{SENSOR_ADDR, 0xB3F6, 0x6E},
	{SENSOR_ADDR, 0xB3F7, 0x00},
	{SENSOR_ADDR, 0xB3F8, 0x26},
	{SENSOR_ADDR, 0xB3F9, 0x02},
	{SENSOR_ADDR, 0xB3FA, 0x26},
	{SENSOR_ADDR, 0xB3FB, 0x02},
	{SENSOR_ADDR, 0xB3FC, 0x01},
	{SENSOR_ADDR, 0xB3FD, 0x00},
	{SENSOR_ADDR, 0xB3FE, 0x01},
	{SENSOR_ADDR, 0xB3FF, 0x00},
	{SENSOR_ADDR, 0xB400, 0x96},
	{SENSOR_ADDR, 0xB401, 0x00},
	{SENSOR_ADDR, 0xB402, 0x96},
	{SENSOR_ADDR, 0xB403, 0x00},
	{SENSOR_ADDR, 0xB404, 0x26},
	{SENSOR_ADDR, 0xB405, 0x02},
	{SENSOR_ADDR, 0xB406, 0x26},
	{SENSOR_ADDR, 0xB407, 0x02},
	{SENSOR_ADDR, 0xB408, 0x26},
	{SENSOR_ADDR, 0xB409, 0x02},
	{SENSOR_ADDR, 0xB40A, 0x26},
	{SENSOR_ADDR, 0xB40B, 0x02},
	{SENSOR_ADDR, 0xB40C, 0x96},
	{SENSOR_ADDR, 0xB40D, 0x00},
	{SENSOR_ADDR, 0xB40E, 0x96},
	{SENSOR_ADDR, 0xB40F, 0x00},
	{SENSOR_ADDR, 0xB410, 0x96},
	{SENSOR_ADDR, 0xB411, 0x00},
	{SENSOR_ADDR, 0xB412, 0x96},
	{SENSOR_ADDR, 0xB413, 0x00},
	{SENSOR_ADDR, 0xB414, 0x26},
	{SENSOR_ADDR, 0xB415, 0x02},
	{SENSOR_ADDR, 0xB416, 0x26},
	{SENSOR_ADDR, 0xB417, 0x02},
	{SENSOR_ADDR, 0xB418, 0x6E},
	{SENSOR_ADDR, 0xB419, 0x00},
	{SENSOR_ADDR, 0xB41A, 0x6E},
	{SENSOR_ADDR, 0xB41B, 0x00},
	{SENSOR_ADDR, 0xB41C, 0x96},
	{SENSOR_ADDR, 0xB41D, 0x00},
	{SENSOR_ADDR, 0xB41E, 0x96},
	{SENSOR_ADDR, 0xB41F, 0x00},
	{SENSOR_ADDR, 0xB420, 0x01},
	{SENSOR_ADDR, 0xB421, 0x00},
	{SENSOR_ADDR, 0xB422, 0x01},
	{SENSOR_ADDR, 0xB423, 0x00},
	{SENSOR_ADDR, 0xB424, 0xFC},
	{SENSOR_ADDR, 0xB425, 0x03},
	{SENSOR_ADDR, 0xB426, 0xFC},
	{SENSOR_ADDR, 0xB427, 0x03},
	{SENSOR_ADDR, 0xB428, 0xFC},
	{SENSOR_ADDR, 0xB429, 0x03},
	{SENSOR_ADDR, 0xB42A, 0xFC},
	{SENSOR_ADDR, 0xB42B, 0x03},
	{SENSOR_ADDR, 0xB42C, 0x50},
	{SENSOR_ADDR, 0xB42D, 0x00},
	{SENSOR_ADDR, 0xB42E, 0x50},
	{SENSOR_ADDR, 0xB42F, 0x00},
	{SENSOR_ADDR, 0xB430, 0x50},
	{SENSOR_ADDR, 0xB431, 0x00},
	{SENSOR_ADDR, 0xB432, 0x50},
	{SENSOR_ADDR, 0xB433, 0x00},
	{SENSOR_ADDR, 0xB434, 0x84},
	{SENSOR_ADDR, 0xB435, 0x03},
	{SENSOR_ADDR, 0xB436, 0x84},
	{SENSOR_ADDR, 0xB437, 0x03},
	{SENSOR_ADDR, 0xB438, 0x84},
	{SENSOR_ADDR, 0xB439, 0x03},
	{SENSOR_ADDR, 0xB43A, 0x84},
	{SENSOR_ADDR, 0xB43B, 0x03},
	{SENSOR_ADDR, 0xB43C, 0x64},
	{SENSOR_ADDR, 0xB43D, 0x00},
	{SENSOR_ADDR, 0xB43E, 0x64},
	{SENSOR_ADDR, 0xB43F, 0x00},
	{SENSOR_ADDR, 0xB440, 0x64},
	{SENSOR_ADDR, 0xB441, 0x00},
	{SENSOR_ADDR, 0xB442, 0x64},
	{SENSOR_ADDR, 0xB443, 0x00},
	{SENSOR_ADDR, 0xB44F, 0x88},
	{SENSOR_ADDR, 0xFFFF, 0x00},
	{TABLE_WAIT_MS, 0x0000, 3},//delay 3ms
	{SENSOR_ADDR, 0x5B24, 0x20},
	{SENSOR_ADDR, 0x5B25, 0x0D},
	{SENSOR_ADDR, 0x5B28, 0x88},
	{SENSOR_ADDR, 0x5B29, 0x04},
	{SENSOR_ADDR, 0x9789, 0x01},
	{TABLE_WAIT_MS, 0x0000, 3},//delay 3ms
	{SENSOR_ADDR, 0x9788, 0x01},
	{SENSOR_ADDR, 0x95C1, 0x01},
	{TABLE_WAIT_MS, 0x0000, 3},//delay 3ms
	{SENSOR_ADDR, 0xFFFF, 0x00},
	{TABLE_WAIT_MS, 0x0000, 3},//delay 3ms
	{SENSOR_ADDR, 0xB6F1, 0x54},
	{SENSOR_ADDR, 0x95C1, 0x01},
	{TABLE_WAIT_MS, 0x0000, 3},//delay 3ms
	{SENSOR_ADDR, 0x1B04, 0x5C},
	{SENSOR_ADDR, 0x1B04, 0xA3},
	{TABLE_WAIT_MS, 0x0000, 50},//delay 50ms
	{SENSOR_ADDR, 0xFFFF, 0x05},
	{TABLE_WAIT_MS, 0x0000, 3},//delay 3ms
	{SENSOR_ADDR, 0xB700, 0x10},
	{SENSOR_ADDR, 0x95C1, 0x01},
	{TABLE_WAIT_MS, 0x0000, 3},//delay 3ms
	{SENSOR_ADDR, 0xB6F1, 0x55},
	{SENSOR_ADDR, 0x95C1, 0x01},
	{TABLE_WAIT_MS, 0x0000, 3},//delay 3ms
	{SENSOR_ADDR, 0x9730, 0xA5},
	{SENSOR_ADDR, 0xFFFF, 0x10},
	{TABLE_WAIT_MS, 0x0000, 3},//delay 3ms
	{SENSOR_ADDR, 0x5958, 0x1A},
	{SENSOR_ADDR, 0x5959, 0x22},
	{SENSOR_ADDR, 0x595C, 0x21},
	{SENSOR_ADDR, 0x595D, 0x23},
	{SENSOR_ADDR, 0x595E, 0x12},
	{SENSOR_ADDR, 0x595F, 0x09},
	{SENSOR_ADDR, 0x5963, 0x0F},
	{SENSOR_ADDR, 0x5967, 0x09},
	{SENSOR_ADDR, 0x596C, 0x15},
	{SENSOR_ADDR, 0x596D, 0x13},
	{SENSOR_ADDR, 0x596E, 0x10},
	{SENSOR_ADDR, 0x596F, 0x19},
	{SENSOR_ADDR, 0x5970, 0x13},
	{SENSOR_ADDR, 0x5971, 0x0F},
	{SENSOR_ADDR, 0x5972, 0x07},
	{SENSOR_ADDR, 0x5973, 0x17},
	{SENSOR_ADDR, 0x5974, 0x17},
	{SENSOR_ADDR, 0x5977, 0x03},
	{SENSOR_ADDR, 0x5978, 0x22},
	{SENSOR_ADDR, 0x5979, 0x16},
	{SENSOR_ADDR, 0x597A, 0x16},
	{SENSOR_ADDR, 0x597B, 0x16},
	{SENSOR_ADDR, 0x597C, 0x12},
	{SENSOR_ADDR, 0x597D, 0x10},
	{SENSOR_ADDR, 0x597E, 0x0F},
	{SENSOR_ADDR, 0x597F, 0x0D},
	{SENSOR_ADDR, 0x5980, 0x0B},
	{SENSOR_ADDR, 0x5986, 0x03},
	{SENSOR_ADDR, 0x5987, 0x09},
	{SENSOR_ADDR, 0x5997, 0x02},
	{SENSOR_ADDR, 0x599F, 0x06},
	{SENSOR_ADDR, 0x59A8, 0x0E},
	{SENSOR_ADDR, 0x59AC, 0x09},
	{SENSOR_ADDR, 0x59B2, 0x0F},
	{SENSOR_ADDR, 0x59CC, 0x0E},
	{SENSOR_ADDR, 0x59D5, 0x10},
	{SENSOR_ADDR, 0x59D6, 0x0B},
	{SENSOR_ADDR, 0x59DD, 0x1B},
	{SENSOR_ADDR, 0x59DE, 0x1B},
	{SENSOR_ADDR, 0x59DF, 0x1D},
	{SENSOR_ADDR, 0xAC97, 0x08},
	{SENSOR_ADDR, 0x59C2, 0x0E},
	{SENSOR_ADDR, 0x59C3, 0x13},
	{SENSOR_ADDR, 0x59C4, 0x13},
	{SENSOR_ADDR, 0x595B, 0x08},
	{SENSOR_ADDR, 0x5975, 0x08},
	{SENSOR_ADDR, 0x5976, 0x08},
	{SENSOR_ADDR, 0x5962, 0x0B},
	{SENSOR_ADDR, 0xFFFF, 0x05},
	{TABLE_WAIT_MS, 0x0000, 3},//delay 20ms
	{0x00, TABLE_END, 0x00 }
};
enum {
	SENSOR_MODE_1920X1080_CROP_30FPS,
	SENSOR_MODE_1920X1536_CROP_30FPS,
	SENSOR_MODE_2880X1860_CROP_30FPS,
	SENSOR_MODE_3840X2160_CROP_30FPS,
	SENSOR_MODE_1280X720_CROP_30FPS,
	ISX028C_ES3_MODE_3840X2160_CROP_30FPS,
	ISX028C_ES2_MODE_3840X2160_CROP_30FPS,
	SENSOR_START_STREAM,
	SENSOR_STOP_STREAM,
	MAX96712_GMSL2_6G_CONFIG,
	MAX96712_GMSL1_CONFIG,
};

static struct index_reg_8 *mode_table[] = {
	[SENSOR_MODE_1920X1080_CROP_30FPS]  = sensor_3840x2160_crop_30fps,
	[SENSOR_MODE_1920X1536_CROP_30FPS]  = sensor_3840x2160_crop_30fps,
	[SENSOR_MODE_2880X1860_CROP_30FPS]  = sensor_3840x2160_crop_30fps,
	[SENSOR_MODE_3840X2160_CROP_30FPS]  = sensor_3840x2160_crop_30fps,
	[SENSOR_MODE_1280X720_CROP_30FPS]  = sensor_3840x2160_crop_30fps,
	[ISX028C_ES3_MODE_3840X2160_CROP_30FPS] = isx028c_ES3_3840x2160_crop_30fps,
	[ISX028C_ES2_MODE_3840X2160_CROP_30FPS] = isx028c_ES2_3840x2160_crop_30fps,
	[SENSOR_START_STREAM]               = sensor_start,
	[SENSOR_STOP_STREAM]                = sensor_stop,
	[MAX96712_GMSL2_6G_CONFIG]          = max96712_gmsl2_6G_tbl,
	[MAX96712_GMSL1_CONFIG]             = max96712_gmsl1_tbl,
};

static const int freamrate_30fps[] = {
	30,
};

static const struct camera_common_frmfmt sensor_frmfmt[] = {
	{{1920, 1080}, freamrate_30fps, 1, 0, SENSOR_MODE_1920X1080_CROP_30FPS},
	{{1920, 1536}, freamrate_30fps, 1, 0, SENSOR_MODE_1920X1536_CROP_30FPS},
	{{2880, 1860}, freamrate_30fps, 1, 0, SENSOR_MODE_2880X1860_CROP_30FPS},
	{{3840, 2160}, freamrate_30fps, 1, 0, SENSOR_MODE_3840X2160_CROP_30FPS},
	{{1280, 720}, freamrate_30fps, 1, 0, SENSOR_MODE_3840X2160_CROP_30FPS},
	{{3840, 2160}, freamrate_30fps, 1, 0, ISX028C_ES3_MODE_3840X2160_CROP_30FPS},
};
#endif /* __SGX_GW5_GMSL_TABLES__ */
