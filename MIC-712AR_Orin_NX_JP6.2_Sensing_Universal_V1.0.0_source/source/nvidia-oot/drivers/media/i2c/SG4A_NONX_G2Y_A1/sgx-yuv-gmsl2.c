/*
 * yuv - gw5xxx sensor driver
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
#define DEBUG 1
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/module.h>

#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#include "max96712.h"

#include <media/tegracam_core.h>
#include "sgx-yuv-gmsl2-tbls.h"

#define MAX_RADIAL_COEFFICIENTS         6
#define MAX_TANGENTIAL_COEFFICIENTS     2
#define MAX_FISHEYE_COEFFICIENTS        6

extern int max96712_write_reg_Dser(int device_id, int slaveAddr,
		u16 addr, u8 val);

extern int max96712_read_reg_Dser(int device_id, int slaveAddr,
		                u16 addr, unsigned int *val);

//static int Der_Ser_init_Flag = 0;

const struct of_device_id sensor_of_match[] = {
	{.compatible = "sensing,sgx-yuv-gmsl2-0",},
	{.compatible = "sensing,sgx-yuv-gmsl2-1",},
	{.compatible = "sensing,sgx-yuv-gmsl2-2",},
	{.compatible = "sensing,sgx-yuv-gmsl2-3",},
	{.compatible = "sensing,sgx-yuv-gmsl2-4",},
	{.compatible = "sensing,sgx-yuv-gmsl2-5",},
	{.compatible = "sensing,sgx-yuv-gmsl2-6",},
	{.compatible = "sensing,sgx-yuv-gmsl2-7",},
	{.compatible = "sensing,sgx-yuv-gmsl2-8",},
	{.compatible = "sensing,sgx-yuv-gmsl2-9",},
	{.compatible = "sensing,sgx-yuv-gmsl2-10",},
	{.compatible = "sensing,sgx-yuv-gmsl2-11",},
	{.compatible = "sensing,sgx-yuv-gmsl2-12",},
	{.compatible = "sensing,sgx-yuv-gmsl2-13",},
	{.compatible = "sensing,sgx-yuv-gmsl2-14",},
	{.compatible = "sensing,sgx-yuv-gmsl2-15",},
	{ },
};
MODULE_DEVICE_TABLE(of, sensor_of_match);

static const u32 ctrl_cid_list[] = {
	TEGRA_CAMERA_CID_GAIN,
	TEGRA_CAMERA_CID_EXPOSURE,
	TEGRA_CAMERA_CID_EXPOSURE_SHORT,
	TEGRA_CAMERA_CID_FRAME_RATE,
	TEGRA_CAMERA_CID_HDR_EN,
	TEGRA_CAMERA_CID_SENSOR_MODE_ID,
};

// Coefficients as per distortion model (wide FOV) being used
typedef struct
{
	// Radial coefficients count
	u32 coeff_count;
	// Radial coefficients
	float k[MAX_FISHEYE_COEFFICIENTS];
	// 0 -> equidistant, 1 -> equisolid, 2 -> orthographic, 3 -> stereographic
	u32 mapping_type;
} fisheye_lens_distortion_coeff;

// Coefficients as per distortion model being used
typedef struct
{
	// Radial coefficients count
	u32 radial_coeff_count;
	// Radial coefficients
	float k[MAX_RADIAL_COEFFICIENTS];
	// Tangential coefficients count
	u32 tangential_coeff_count;
	// Tangential coefficients
	float p[MAX_TANGENTIAL_COEFFICIENTS];
} polynomial_lens_distortion_coeff;

/*
 * Stereo Eeprom Data
 */
typedef struct
{
	// Width and height of image in pixels
	u32 width, height;
	// Focal length in pixels
	float fx, fy;
	float skew;
	// Principal point (optical center) in pixels
	float cx, cy;
	/*
	 * Structure for distortion coefficients as per the model being used
	 * 0: pinhole, assuming polynomial distortion
	 * 1: fisheye, assuming fisheye distortion)
	 * 2: ocam (omini-directional)
	 */
	u32 distortion_type;
	union distortion_coefficients {
		polynomial_lens_distortion_coeff poly;
		fisheye_lens_distortion_coeff fisheye;
	} dist_coeff;
} camera_intrinsics;

/*
 * Extrinsic parameters shared by camera and IMU.
 * All rotation + translation with respect to the same reference point
 */
typedef struct
{
	/*
	 * Rotation parameter expressed in Rodrigues notation
	 * angle = sqrt(rx^2+ry^2+rz^2)
	 * unit axis = [rx,ry,rz]/angle
	 */
	float rx, ry, rz;
	// Translation parameter from one camera to another parameter
	float tx, ty, tz;
} camera_extrinsics;

typedef struct
{
	// 3D vector to add to accelerometer readings
	float linear_acceleration_bias[3];
	// 3D vector to add to gyroscope readings
	float angular_velocity_bias[3];
	// gravity acceleration
	float gravity_acceleration[3];
	// Extrinsic structure for IMU device
	camera_extrinsics extr;
} imu_params;

typedef struct
{
	// Intrinsic structure for  camera device
	camera_intrinsics cam_intr;

	// Extrinsic structure for camera device
	camera_extrinsics cam_extr;

	// Flag for IMU availability
	u32 imu_present;

	// Intrinsic structure for IMU
	imu_params imu;
} NvCamSyncSensorCalibData;

typedef struct
{
	/**
	 * EEPROM layout version
	 */
	u32 version;
	/**
	 * Factory Blob Flag, to set when factory flashed and reset to 0
	 * when user modified
	 */
	u32 factory_data;
	/**
	 * Intrinsic structure for camera device
	 */
	camera_intrinsics left_cam_intr;
	camera_intrinsics right_cam_intr;
	/**
	 * Extrinsic structure for camera device
	 */
	camera_extrinsics cam_extr;
	/**
	 * Flag for IMU 0-absent, 1-present
	 */
	u32 imu_present;
	/**
	 * Intrinsic structure for IMU
	 */
	imu_params imu;
} LiEeprom_Content_Struct;

struct sensor {
	struct i2c_client	*i2c_client;
	struct regmap *regmap;
	const struct i2c_device_id *id;
	struct v4l2_subdev	*subdev;
	u32	frame_length;
	struct camera_common_data	*s_data;
	struct tegracam_device		*tc_dev;
	u32     der_id;
	u32 	sync_sensor_index;
	NvCamSyncSensorCalibData EepromCalib;
	u32 link_num[4]; 
	int GMSLMODE[4];
	//sensor源地址
	u32 def_addr;
	//sensor目标地址
	u32 ser_addr;
};

//max96712 gmsl2 mode link lock status,    link a, link b , link c, link d
static int max96712_gmsl2_link_reg_tbl[4] = {0x001A, 0x000A, 0x000B, 0x000C}; //

//for max96712 gmsl1 mode link lock status,    link a, link b , link c, link d
static int max96712_gmsl1_link_reg_tbl[4] = {0x0BCB, 0x0CCB, 0x0DCB, 0x0ECB}; //0x90 or 0xb0

static const struct regmap_config sensor_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};


static int n_para = 4;
static int GMSLMODE_0[4] = {0,0,0,0};
static int GMSLMODE_1[4] = {0,0,0,0};
static int GMSLMODE_2[4] = {0,0,0,0};
static int GMSLMODE_3[4] = {0,0,0,0};
static int *GMSLMODE[4] = {GMSLMODE_0, GMSLMODE_1, GMSLMODE_2, GMSLMODE_3};

module_param_array(GMSLMODE_0, int, &n_para, S_IRUGO);
module_param_array(GMSLMODE_1, int, &n_para, S_IRUGO);
module_param_array(GMSLMODE_2, int, &n_para, S_IRUGO);
module_param_array(GMSLMODE_3, int, &n_para, S_IRUGO);

static inline int sensor_read_reg(struct sensor *priv, int slaveAddr,
		u16 addr, unsigned int *val)
{
	struct i2c_client *i2c_client = NULL;
	int bak = 0;
	int err;

	i2c_client = priv->i2c_client;
	bak = i2c_client->addr;
	i2c_client->addr = slaveAddr / 2;
	err = regmap_read(priv->regmap, addr, val);
	i2c_client->addr = bak;
	if(err)
	{
		dev_err(&i2c_client->dev, "0x%x-r: 0x%x = 0x%x\n",
					i2c_client->addr, addr, *val);
		return -1;
	}
	else
		dev_info(&i2c_client->dev, "0x%x-r: 0x%x = 0x%x\n",
					slaveAddr, addr, *val);

	return 0;
}

static int sensor_write_reg(struct sensor *priv, int slaveAddr,
		u16 addr, u8 val)
{
	struct i2c_client *i2c_client = NULL;
	int bak = 0;
	int err;

	i2c_client = priv->i2c_client;
	bak = i2c_client->addr;
	i2c_client->addr = slaveAddr / 2;
	err = regmap_write(priv->regmap, addr, val);
	i2c_client->addr = bak;
	if(err)
		dev_err(&i2c_client->dev, "0x%x-w: 0x%x = 0x%x\n", slaveAddr, addr, val);
	else
		dev_info(&i2c_client->dev, "0x%04x-w: 0x%04X = 0x%02X\n", slaveAddr, addr, val);
		
	return 0;
}

static int dser_link_check(struct sensor *priv)
{
	struct tegracam_device *tc_dev = priv->tc_dev;
	struct device *dev = tc_dev->dev;
	unsigned int link = 0;
	unsigned int port_addr = 0;
	int err = 0, val = 0;
	int cam_id = priv->id->driver_data % 4;

	dev_info(dev, "%s Der-%d link port %ld---%d device....\n", __func__, priv->der_id, priv->id->driver_data, cam_id);

	if(priv->GMSLMODE[cam_id] != 0) {
		max96712_read_reg_Dser(priv->der_id, MAX96712_ADDR, 0x0006, &val);
		port_addr = max96712_gmsl2_link_reg_tbl[cam_id];
		err = max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x0006, val + (1 << cam_id));
		if (err){
			dev_err(dev,"can't find DER in Convert broad port %d!\n", priv->der_id);
			return 0;
		}
		msleep(200);
		max96712_read_reg_Dser(priv->der_id, MAX96712_ADDR, port_addr, &link);
		
		dev_info(dev, "%s link:0x%02x\n", __func__, link);
		if (link & 0x8) {
			max96712_write_reg_Dser(priv->der_id, MAX96717_ADDR, 0x02BE, 0x10);
			max96712_write_reg_Dser(priv->der_id, MAX96717_ADDR, 0x02d6, 0x10);
			msleep(100);
			max96712_write_reg_Dser(priv->der_id, MAX96717_ADDR, 0x0318, 0x5E);
			max96712_write_reg_Dser(priv->der_id, MAX96717_ADDR, 0x02d3, 0x00);
			max96712_write_reg_Dser(priv->der_id, MAX96717_ADDR, 0x02d6, 0x00);
			msleep(100);
			max96712_write_reg_Dser(priv->der_id, MAX96717_ADDR, 0x02d3, 0x10);
			max96712_write_reg_Dser(priv->der_id, MAX96717_ADDR, 0x02d6, 0x10);
			max96712_write_reg_Dser(priv->der_id, MAX96717_ADDR, 0x0002, 0x03);
			max96712_write_reg_Dser(priv->der_id, MAX96717_ADDR, 0x0000, 0x84 + cam_id * 4);

			return 1;
		} else {
			return 0;
		}
	}
	else{
		max96712_read_reg_Dser(priv->der_id, MAX96712_ADDR, 0x0006, &val);
		port_addr = max96712_gmsl1_link_reg_tbl[cam_id];
		err = max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x0006, val+ (1 << cam_id));
		if (err){
			dev_err(dev,"can't find DER in Convert broad port %d!\n", priv->der_id);
			return 0;
		}
		msleep(200);
		sensor_write_reg(priv, 0x80, 0x04, 0x47);msleep(3);
		sensor_read_reg(priv, 0x80, 0x00 , &val);
		max96712_read_reg_Dser(priv->der_id, MAX96712_ADDR, port_addr, &link);
		dev_info(dev, "%s link:0x%02x\n", __func__, link);
		if (link & 0x90) {
			sensor_write_reg(priv, 0x80, 0x07, 0x84);msleep(1);

			sensor_write_reg(priv, 0x80, 0x0F, 0xBE);msleep(2);
			sensor_write_reg(priv, 0x80, 0x0F, 0xBF);
			sensor_write_reg(priv, 0x80, 0x00, 0x84 + cam_id * 4);

			return 1;
		} else {
			return 0;
		}
	}

	return 1;
}

static int i2c_write_table(struct sensor *priv,
		const struct index_reg_8 table[])
{
	// struct tegracam_device *tc_dev = priv->tc_dev;
	// struct device *dev = tc_dev->dev;
	int i = 0;
	int ret = 0;
	int retry = 2;
	int source = 0xffff;

	//dev_dbg(dev, "%s: channel %d, ", __func__, priv->channel);
		
	while (table[i].source != 0x00) {
		if (table[i].source == TABLE_WAIT_MS)
		{
			msleep(table[i].val);
			i++;
			continue;
		}
		else if (table[i].source == SENSOR_ADDR) {
			//source = 0x6E + (priv->id->driver_data % 4) * 4;
			source = priv->ser_addr << 1;
		}
		else if (table[i].source == MAX96717_ADDR) {
			source = 0x84 + (priv->id->driver_data % 4) * 4;
		}
		else if (table[i].source == MAX96712_ADDR) {
			source = table[i].source;
		}

		ret = max96712_write_reg_Dser(priv->der_id, source, table[i].addr, (u8)table[i].val);
		if (ret)
		{
			retry--;
			if (retry > 0) {
				//dev_info(dev, "max96712_write_reg_Dser: try %d\n", retry);
				msleep(4);
				continue;
			}
			return -1;
		}
		i++;
	}
	return 0;
}

static int sensor_power_on(struct camera_common_data *s_data)
{
	int err = 0;
	struct camera_common_power_rail *pw = s_data->power;
	struct camera_common_pdata *pdata = s_data->pdata;
	struct device *dev = s_data->dev;

	//dev_dbg(dev, "%s\n", __func__);
	if (pdata && pdata->power_on) {
		err = pdata->power_on(pw);
		if (err)
			dev_err(dev, "%s failed.\n", __func__);
		else
			pw->state = SWITCH_ON;
		return err;
	}

	if (pw->reset_gpio > 0)
		gpio_set_value(pw->reset_gpio, 1);

	usleep_range(1000, 2000);
	if (pw->reset_gpio > 0)
		gpio_set_value(pw->reset_gpio, 1);

	usleep_range(10000, 20000);
	pw->state = SWITCH_ON;

	return 0;
}

static int sensor_power_off(struct camera_common_data *s_data)
{
	int err = 0;
	struct camera_common_power_rail *pw = s_data->power;
	struct camera_common_pdata *pdata = s_data->pdata;
	struct device *dev = s_data->dev;

	dev_dbg(dev, "%s\n", __func__);
	if (pdata && pdata->power_off) {
		err = pdata->power_off(pw);
		if (!err)
			goto power_off_done;
		else
			dev_err(dev, "%s failed.\n", __func__);
		return err;
	}

power_off_done:
	pw->state = SWITCH_OFF;

	return 0;
}

static int sensor_power_get(struct tegracam_device *tc_dev)
{
	struct device *dev = tc_dev->dev;
	struct camera_common_data *s_data = tc_dev->s_data;
	struct camera_common_power_rail *pw = s_data->power;
	struct camera_common_pdata *pdata = s_data->pdata;
	const char *mclk_name;
	const char *parentclk_name;
	struct clk *parent;
	int err = 0;

	mclk_name = pdata->mclk_name ?
		pdata->mclk_name : "cam_mclk1";
	pw->mclk = devm_clk_get(dev, mclk_name);
	if (IS_ERR(pw->mclk)) {
		dev_err(dev, "unable to get clock %s\n", mclk_name);
		return PTR_ERR(pw->mclk);
	}

	parentclk_name = pdata->parentclk_name;
	if (parentclk_name) {
		parent = devm_clk_get(dev, parentclk_name);
		if (IS_ERR(parent)) {
			dev_err(dev, "unable to get parent clcok %s",
					parentclk_name);
		} else
			clk_set_parent(pw->mclk, parent);
	}
	if (!err) {
		pw->reset_gpio = pdata->reset_gpio;
		pw->af_gpio = pdata->af_gpio;
		pw->pwdn_gpio = pdata->pwdn_gpio;
	}

	pw->state = SWITCH_OFF;

	return err;
}

static int sensor_power_put(struct tegracam_device *tc_dev)
{
	struct camera_common_data *s_data = tc_dev->s_data;
	struct camera_common_power_rail *pw = s_data->power;

	if (unlikely(!pw))
		return -EFAULT;

	return 0;
}

static int sensor_set_group_hold(struct tegracam_device *tc_dev, bool val)
{
	return 0;
}

static int sensor_set_gain(struct tegracam_device *tc_dev, s64 val)
{
	return 0;
}

static int sensor_set_frame_rate(struct tegracam_device *tc_dev, s64 val)
{
	//struct sensor *priv = (struct sensor *)tegracam_get_privdata(tc_dev);

	// if (val == 30000000) {  //30fps full resolution
	// 	max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x04A5, 0x35);
	// 	max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x04A6, 0xB7);
	// 	max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x04A7, 0x0C);
	// 	priv->frame_length = 0xC20;
	// } else if (val == 60000000) {//60fps full resolution
	// 	max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x04A5, 0x9A);
	// 	max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x04A6, 0x5B);
	// 	max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x04A7, 0x06);
	// 	priv->frame_length = 0x610;
	// } else if (val == 120000000) {//120fps binning resolution
	// 	max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x04A5, 0xCD);
	// 	max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x04A6, 0x2D);
	// 	max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x04A7, 0x03);
	// 	priv->frame_length = 0x308;
	// }

	return 0;
}

static int sensor_set_exposure(struct tegracam_device *tc_dev, s64 val)
{
	return 0;
}

static struct tegracam_ctrl_ops sensor_ctrl_ops = {
	.numctrls = ARRAY_SIZE(ctrl_cid_list),
	.ctrl_cid_list = ctrl_cid_list,
	.set_gain = sensor_set_gain,
	.set_exposure = sensor_set_exposure,
	.set_exposure_short = sensor_set_exposure,
	.set_frame_rate = sensor_set_frame_rate,
	.set_group_hold = sensor_set_group_hold,
};

static struct camera_common_pdata *sensor_parse_dt(struct tegracam_device *tc_dev)
{
	struct device *dev = tc_dev->dev;
	struct device_node *node = dev->of_node;
	struct camera_common_pdata *board_priv_pdata;
	const struct of_device_id *match;
	int err;
	//int gpio = 0;

	if (!node)
		return NULL;

	match = of_match_device(sensor_of_match, dev);
	if (!match) {
		dev_err(dev, "Failed to find matching dt id\n");
		return NULL;
	}

	board_priv_pdata = devm_kzalloc(dev, sizeof(*board_priv_pdata), GFP_KERNEL);

	err = of_property_read_string(node, "mclk",
			&board_priv_pdata->mclk_name);
	if (err)
		dev_err(dev, "mclk not in DT\n");

	board_priv_pdata->reset_gpio = of_get_named_gpio(node,
			"reset-gpios", 0);
	gpio_direction_output(board_priv_pdata->reset_gpio, 1);

	// gpio = of_get_named_gpio(node,
	// 		"pwdn-gpios", 0);

	// gpio_direction_output(gpio, 1);
	// gpio = of_get_named_gpio(node,
	// 		"pwr-gpios", 0);

	// gpio_direction_output(gpio, 1);

	board_priv_pdata->has_eeprom = 0;
	//	of_property_read_bool(node, "has-eeprom");
	return board_priv_pdata;
}

static int max96712_set_lane_rate(struct sensor *priv, u16 lane_rate)
{
	int i, err = 0;
	int val = 0 , link_num = 0;
	u8 rate = 0;

	for(i = 0 ; i < 4 ; i++){
		max96712_read_reg_Dser(priv->der_id, MAX96712_ADDR, max96712_gmsl2_link_reg_tbl[i], &val);
		if((val & 0x80 ) != 0){
			link_num++;
		}
	}
	priv->link_num[priv->der_id] = link_num;
	
	rate = 0xE0 + lane_rate + 2*(priv->link_num[priv->der_id]);
	
	// Fixed set max
	err += max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x041B, 0xF7);
	dev_info(priv->tc_dev->dev, "link_num = %d ,lane_rate = 0x%x\n", priv->link_num[priv->der_id] ,rate);

	//descrew
	val = 0x80;
	err = max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x903, val);
	err += max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x943, val);
	err += max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x983, val);
	err += max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x9c3, val);
		
	return err;
}

static int sensor_set_mode(struct tegracam_device *tc_dev)
{
	struct sensor *priv = (struct sensor *)tegracam_get_privdata(tc_dev);
	struct camera_common_data *s_data = tc_dev->s_data;
	struct device *dev = tc_dev->dev;
	struct sensor_mode_properties *mode =
	 	&s_data->sensor_props.sensor_modes[s_data->mode_prop_idx];
	const struct of_device_id *match;
	int lane_rate;
    int err = 0;
	match = of_match_device(sensor_of_match, dev);
	if (!match) {
		dev_err(dev, "Failed to find matching dt id\n");
		return -EINVAL;
	}

	dev_dbg(dev, "%s set_mode enter\n", __func__);
	dev_dbg(dev, "%s mode_prop_idx= %d/n", __func__,s_data->mode_prop_idx);
	//配置ISX028C
	if(s_data->mode_prop_idx == 5){
		int val = 0;

		max96712_write_reg_Dser(priv->der_id, 0x84 + (priv->id->driver_data % 4) * 4, 0x0042, priv->ser_addr << 1);
		max96712_write_reg_Dser(priv->der_id, 0x84 + (priv->id->driver_data % 4) * 4, 0x0043, 0x34);
		max96712_write_reg_Dser(priv->der_id, priv->ser_addr << 1, 0xffff, 0x05);
		msleep(40);
		max96712_read_reg_Dser(priv->der_id, priv->ser_addr << 1, 0x8395, &val);
		msleep(40);
		if(val == 0x04){
			err = i2c_write_table(priv, mode_table[ISX028C_ES2_MODE_3840X2160_CROP_30FPS]);
		}
		else{
			err = i2c_write_table(priv, mode_table[ISX028C_ES3_MODE_3840X2160_CROP_30FPS]);	
		}
		max96712_write_reg_Dser(priv->der_id, 0x84 + (priv->id->driver_data % 4) * 4, 0x02d3, 0x00);
		msleep(300);
		max96712_write_reg_Dser(priv->der_id, 0x84 + (priv->id->driver_data % 4) * 4, 0x02d3, 0x10);
		msleep(40);
		if (err) 
		dev_err(dev, "can't find sensor in port\n");
	}

	lane_rate = mode->signal_properties.serdes_pixel_clock.val * 16 / 100000000 / mode->signal_properties.num_lanes;
	max96712_set_lane_rate(priv, lane_rate);
	i2c_write_table(priv, mode_table[s_data->mode_prop_idx]);
	if(s_data->mode_prop_idx == 4) {
		//timing
		int cam_id = priv->id->driver_data % 4;
		sensor_write_reg(priv, (0x84 + cam_id * 4), 0x20, 0x00);
		sensor_write_reg(priv, (0x84 + cam_id * 4), 0x47, 0x24);
		sensor_write_reg(priv, (0x84 + cam_id * 4), 0x48, 0xF5);
		sensor_write_reg(priv, (0x84 + cam_id * 4), 0x49, 0x80);
		sensor_write_reg(priv, (0x84 + cam_id * 4), 0x43, 0x25);
	}
	
	return 0;
}

static int sensor_start_streaming(struct tegracam_device *tc_dev)
{
	int val = 0;
	struct sensor *priv = (struct sensor *)tegracam_get_privdata(tc_dev);
	struct camera_common_data *s_data = priv->s_data;
	struct device *dev = s_data->dev;
	int cam_id = priv->id->driver_data % 4;

	dev_dbg(dev, "%s enter 11111\n", __func__);
	if(priv->GMSLMODE[cam_id] != 0) {
		max96712_write_reg_Dser(priv->der_id, (0x84 + cam_id * 4), 0x0002, 0x53);
	}
	else{
		max96712_read_reg_Dser(priv->der_id, MAX96712_ADDR, 0x0006, &val);
		val = val | (1 << cam_id);
		max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x0006, val);

		sensor_read_reg(priv, (0x84 + cam_id * 4), 0x42 , &val);
		val = val | 0x01;
		sensor_write_reg(priv, (0x84 + cam_id * 4), 0x42, val);msleep(3);
		sensor_write_reg(priv, (0x84 + cam_id * 4), 0x04, 0x87);
	}
	
	return 0;
}

static int sensor_stop_streaming(struct tegracam_device *tc_dev)
{
	int val = 0;
	struct device *dev = tc_dev->dev;
	struct sensor *priv = (struct sensor *)tegracam_get_privdata(tc_dev);
	int cam_id = priv->id->driver_data % 4;

	dev_dbg(dev, "%s enter 22222\n", __func__);
	if(priv->GMSLMODE[cam_id] != 0) {
		max96712_write_reg_Dser(priv->der_id, (0x84 + cam_id * 4), 0x0002, 0x03);
	}
	else{
		sensor_write_reg(priv, (0x84 + cam_id * 4), 0x04, 0x47);
		sensor_read_reg(priv, (0x84 + cam_id * 4), 0x42 , &val);
		val = val & 0xFE;
		sensor_write_reg(priv, (0x84 + cam_id * 4), 0x42, val);msleep(3);
	}

	
	return 0;
}

static int max96712_init_mode(struct sensor *priv)
{
	
	int mode_phyab, mode_phycd;
	int err, val = 0;

	dev_info(priv->tc_dev->dev, "set sensor mode !!!\n");
	err =  max96712_read_reg_Dser(priv->der_id, MAX96712_ADDR, 0x10, &mode_phyab);
	err =  max96712_read_reg_Dser(priv->der_id, MAX96712_ADDR, 0x11, &mode_phycd);

	if (priv->GMSLMODE[0] != 1) 
		mode_phyab = (mode_phyab & ~0x3) | 0x1;
	if (priv->GMSLMODE[1] != 1) 
		mode_phyab = (mode_phyab & ~(0x3 << 4)) | (0x1 << 4);
	if (priv->GMSLMODE[2] != 1) 
		mode_phycd = (mode_phycd & ~0x3) | 0x1;
	if (priv->GMSLMODE[3] != 1) 
		mode_phycd = (mode_phycd & ~(0x3 << 4)) | (0x1 << 4);
	
	err =  max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x0010, mode_phyab);
	err =  max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x0011, mode_phycd);
	dev_err(priv->tc_dev->dev, "###GMSLMODE=%d, %d, %d, %d###\n", priv->GMSLMODE[0], 
																priv->GMSLMODE[1], priv->GMSLMODE[2], priv->GMSLMODE[3]);

	if (priv->GMSLMODE[0] == 0) {
		max96712_read_reg_Dser(priv->der_id, MAX96712_ADDR, 0x041A, &val);
		max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x041A, ((0x01 << 4) | val));
		max96712_read_reg_Dser(priv->der_id, MAX96712_ADDR, 0x0415, &val);
		max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x0415, ((0x04 << 4) | val));
	}
	else{
		max96712_read_reg_Dser(priv->der_id, MAX96712_ADDR, 0x0006, &val);
		max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x0006, ((0x01 << 4) | val));
	} 
	if (priv->GMSLMODE[1] == 0) {
		max96712_read_reg_Dser(priv->der_id, MAX96712_ADDR, 0x041A, &val);
		max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x041A, ((0x02 << 4) | val));
		max96712_read_reg_Dser(priv->der_id, MAX96712_ADDR, 0x0415, &val);
		max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x0415, ((0x08 << 4) | val));
	}
	else{
		max96712_read_reg_Dser(priv->der_id, MAX96712_ADDR, 0x0006, &val);
		max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x0006, ((0x02 << 4) | val));
	}
	if (priv->GMSLMODE[2] == 0) {
		max96712_read_reg_Dser(priv->der_id, MAX96712_ADDR, 0x041A, &val);
		max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x041A, ((0x04 << 4) | val));
		max96712_read_reg_Dser(priv->der_id, MAX96712_ADDR, 0x0418, &val);
		max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x0418, ((0x04 << 4) | val));
	}
	else{
		max96712_read_reg_Dser(priv->der_id, MAX96712_ADDR, 0x0006, &val);
		max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x0006, ((0x04 << 4) | val));
	}
	if (priv->GMSLMODE[3] == 0) { 
		max96712_read_reg_Dser(priv->der_id, MAX96712_ADDR, 0x041A, &val);
		max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x041A, ((0x08 << 4) | val));
		max96712_read_reg_Dser(priv->der_id, MAX96712_ADDR, 0x0418, &val);
		max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x0418, ((0x08<< 4) | val));
	}
	else{
		max96712_read_reg_Dser(priv->der_id, MAX96712_ADDR, 0x0006, &val);
		max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x0006, ((0x08 << 4) | val));
	}

	if (err)
		dev_err(priv->tc_dev->dev, "des set 3g/6g mode err!!\n");
	msleep(200);

	return err;
}

static struct camera_common_sensor_ops sensor_common_ops = {
	.numfrmfmts = ARRAY_SIZE(sensor_frmfmt),
	.frmfmt_table = sensor_frmfmt,
	.power_on = sensor_power_on,
	.power_off = sensor_power_off,
	.parse_dt = sensor_parse_dt,
	.power_get = sensor_power_get,
	.power_put = sensor_power_put,
	.set_mode = sensor_set_mode,
	.start_streaming = sensor_start_streaming,
	.stop_streaming = sensor_stop_streaming,
};

static int sensor_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	//struct i2c_client *client = v4l2_get_subdevdata(sd);

	//dev_dbg(&client->dev, "%s:\n", __func__);

	return 0;
}

static const struct v4l2_subdev_internal_ops sensor_subdev_internal_ops = {
	.open = sensor_open,
};

static int der_init_flag[4];
static int probe_count = 0;
static int sensor_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct tegracam_device *tc_dev;
	struct sensor *priv;
	int err = 0;
	//int i = 0;

	//printk("+++> camera %s init!!!!\n", id->name);

	if (!IS_ENABLED(CONFIG_OF) || !node)
		return -EINVAL;

	priv = devm_kzalloc(dev, sizeof(struct sensor), GFP_KERNEL);
	if (!priv) {
		dev_err(dev, "unable to allocate memory!\n");
		return -ENOMEM;
	}
	tc_dev = devm_kzalloc(dev,
			sizeof(struct tegracam_device), GFP_KERNEL);
	if (!tc_dev)
		return -ENOMEM;

	err = of_property_read_u32(node, "der_id", &priv->der_id);
	if (err)
		dev_err(dev, "der_id not found\n");

	priv->i2c_client = tc_dev->client = client;
	if(priv->regmap == NULL) {
		priv->regmap = devm_regmap_init_i2c(priv->i2c_client,
				&sensor_regmap_config);
	}
	if (IS_ERR(priv->regmap)) {
		dev_err(&client->dev,
			"regmap init failed: %ld\n", PTR_ERR(priv->regmap));
		return -ENODEV;
	}
	err = of_property_read_u32(node, "reg", &priv->ser_addr);
	if (err)
		dev_err(dev, "ser_addr not found\n");
	
	
	priv->id = id;
	tc_dev->dev = dev;
	strncpy(tc_dev->name, id->name, sizeof(tc_dev->name));
	tc_dev->dev_regmap_config = &sensor_regmap_config;
	tc_dev->sensor_ops = &sensor_common_ops;
	tc_dev->v4l2sd_internal_ops = &sensor_subdev_internal_ops;
	tc_dev->tcctrl_ops = &sensor_ctrl_ops;

	err = tegracam_device_register(tc_dev);
	if (err) {
		dev_err(dev, "tegra camera driver registration failed\n");
		return err;
	}
	priv->tc_dev = tc_dev;
	priv->s_data = tc_dev->s_data;
	priv->subdev = &tc_dev->s_data->subdev;
	tegracam_set_privdata(tc_dev, (void *)priv);
	memcpy(priv->GMSLMODE, GMSLMODE[priv->der_id], 4 * sizeof(int));

	sensor_power_on(priv->s_data);
	msleep(100);
	probe_count ++;
#if 1
	if (der_init_flag[priv->der_id] == 0) {
		err = i2c_write_table(priv, mode_table[MAX96712_GMSL1_CONFIG]);
		//err = i2c_write_table(priv, mode_table[MAX96712_GMSL2_6G_CONFIG]);
		if (err)
			dev_err(&client->dev, "can't find DER adapt broad in port %s \n",id->name);
		else
		{
			der_init_flag[priv->der_id] = 1;

			max96712_init_mode(priv);
			dev_info(&client->dev,"Init DER %d adapt broad success\n", priv->der_id);
		}
	}

	if (der_init_flag[priv->der_id] == 1)
	{
		if (dser_link_check(priv) == true) {
			printk("+++> Der-%d port %ld camera %s been detected!\n", priv->der_id, priv->id->driver_data, id->name);
		}
		else
			dev_err(&client->dev,"%s camera %s detect error\n", __func__, id->name);
		
		// if ((probe_count%4) == 0) {
		// 	msleep(100);
		// 	max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x0006, 0xff);
		// 	max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x0018, 0x0F);
		// 	msleep(100);
		// 	max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x040B, 0x02);
		// 	max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x08A0, 0x84);
		// 	msleep(50);
		// }
	}
#endif
	err = tegracam_v4l2subdev_register(tc_dev, true);
	if (err) {
		dev_err(dev, "tegra camera subdev registration failed\n");
		return err;
	}
		
	return 0;
}

static int sensor_remove(struct i2c_client *client)
{
	struct camera_common_data *s_data = to_camera_common_data(&client->dev);
	struct sensor *priv = (struct sensor *)s_data->priv;

	tegracam_v4l2subdev_unregister(priv->tc_dev);
	tegracam_device_unregister(priv->tc_dev);

	gpio_set_value(s_data->power->reset_gpio, 0);
	return 0;
}

static const struct i2c_device_id sensor_id[] = {
	{ "sgx-yuv-gmsl2-0",  0 },
	{ "sgx-yuv-gmsl2-1",  1 },
	{ "sgx-yuv-gmsl2-2",  2 },
	{ "sgx-yuv-gmsl2-3",  3 },
	{ "sgx-yuv-gmsl2-4",  4 },
	{ "sgx-yuv-gmsl2-5",  5 },
	{ "sgx-yuv-gmsl2-6",  6 },
	{ "sgx-yuv-gmsl2-7",  7 },
	{ "sgx-yuv-gmsl2-8",  8 },
	{ "sgx-yuv-gmsl2-9",  9 },
	{ "sgx-yuv-gmsl2-10",  10 },
	{ "sgx-yuv-gmsl2-11",  11 },
	{ "sgx-yuv-gmsl2-12",  12 },
	{ "sgx-yuv-gmsl2-13",  13 },
	{ "sgx-yuv-gmsl2-14",  14 },
	{ "sgx-yuv-gmsl2-15",  15 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, sensor_id);

static struct i2c_driver sensor_i2c_driver = {
	.driver = {
		.name = "sgx-yuv-gmsl2",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sensor_of_match),
	},
	.probe = sensor_probe,
	.remove = sensor_remove,
	.id_table = sensor_id,
};

module_i2c_driver(sensor_i2c_driver);

MODULE_DESCRIPTION("Media Controller driver for SGX-YUV-GMSL2");
MODULE_AUTHOR("SENSING Corporation");
MODULE_AUTHOR("Sudhir ethan chen<chenxinhai@sensing-world.com");
MODULE_LICENSE("GPL v2");