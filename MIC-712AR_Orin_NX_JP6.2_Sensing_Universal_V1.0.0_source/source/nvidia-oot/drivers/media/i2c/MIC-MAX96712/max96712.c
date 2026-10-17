/*
 * max96712.c - max96712 IO Expander driver
 *
 * Copyright (c) 2016-2022, NVIDIA CORPORATION.  All rights reserved.
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
/* #define DEBUG */

#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include "max96712.h"


struct max96712 {
	struct i2c_client *i2c_client;
	struct regmap *regmap;
	u32 device_id;
};
struct max96712 *global_priv[4] ;

static int debug_on = 0;
module_param(debug_on, int, 0644);

int max96712_write_reg_Dser(int device_id, int slaveAddr,
			u16 addr, u8 val)
{
	struct i2c_client *i2c_client = NULL;
	int bak = 0;
	int err;
	/* unsigned int ival = 0; */

	if(global_priv[device_id] == NULL)
		dev_err(&i2c_client->dev, "can't find Der device!\n");

	i2c_client = global_priv[device_id]->i2c_client;
	bak = i2c_client->addr;

	i2c_client->addr = slaveAddr / 2;
	err = regmap_write(global_priv[device_id]->regmap, addr, val);

	i2c_client->addr = bak;
	if(err)
	{
		if(debug_on == 1)
			dev_err(&i2c_client->dev, "0x%x-w: 0x%x = 0x%x\n", slaveAddr, addr, val);
		else
			dev_err(&i2c_client->dev, "i2c-w, write failed\n");
		return -1;
	}
	else
	{
		if(debug_on == 1)
			dev_info(&i2c_client->dev, "0x%04x-w: 0x%04X = 0x%02X\n", slaveAddr, addr, val);
	}
		
	return 0;
}

EXPORT_SYMBOL(max96712_write_reg_Dser);

int max96712_write_reg_reg16_val16(int device_id, int slaveAddr,
			struct reg_16 table[], u16 wait_ms_addr, u16 end_addr)
{
	struct i2c_client *i2c_client = NULL;
	int bak = 0;
	int err;

	if(global_priv[device_id] == NULL)
		dev_err(&i2c_client->dev, "can't find Der device!\n");

	i2c_client = global_priv[device_id]->i2c_client;
	bak = i2c_client->addr;

	i2c_client->addr = slaveAddr / 2;
	err = regmap_util_write_table_16_as_8(global_priv[device_id]->regmap,
					 table,
					 NULL, 0,
					 wait_ms_addr,
					 end_addr);

	i2c_client->addr = bak;
	if(err)
	{
		dev_err(&i2c_client->dev, "i2c-w, write failed\n");
		return -1;
	}

	return 0;
}

EXPORT_SYMBOL(max96712_write_reg_reg16_val16);


int max96712_read_reg_Dser(int device_id, int slaveAddr,
		u16 addr, unsigned int *val)
{
	struct i2c_client *i2c_client = NULL;
	int bak = 0;
	int err;

	if(global_priv[device_id] == NULL)
		dev_err(&i2c_client->dev, "can't find Der device!\n");

	i2c_client = global_priv[device_id]->i2c_client;
	bak = i2c_client->addr;
	i2c_client->addr = slaveAddr / 2;
	err = regmap_read(global_priv[device_id]->regmap, addr, val);
	i2c_client->addr = bak;
	if(err)
	{
		dev_err(&i2c_client->dev, "0x%x-r: 0x%x = 0x%x\n",
					i2c_client->addr, addr, *val);
		return -1;
	}
	// else
	// 	dev_info(&i2c_client->dev, "0x%x-r: 0x%x = 0x%x\n",
	// 				slaveAddr, addr, *val);
	return 0;

}

EXPORT_SYMBOL(max96712_read_reg_Dser);

static int max96712_read_reg(struct max96712 *priv,
			u16 addr, unsigned int *val)
{
	struct i2c_client *i2c_client = priv->i2c_client;
	int err;

	err = regmap_read(priv->regmap, addr, val);
	if (err)
		dev_err(&i2c_client->dev, "%s:i2c read failed, 0x%x = %x\n",
			__func__, addr, *val);

	return err;
}


static int max96712_stats_show(struct seq_file *s, void *data)
{
	return 0;
}

static int max96712_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, max96712_stats_show, inode->i_private);
}

static ssize_t max96712_debugfs_write(struct file *s,
				const char __user *user_buf,
				size_t count, loff_t *ppos)
{
	struct max96712 *priv =
		((struct seq_file *)s->private_data)->private;
	struct i2c_client *i2c_client = priv->i2c_client;

	char buf[255];
	int buf_size;
	int val = 0;

	if (!user_buf || count <= 1)
		return -EFAULT;

	memset(buf, 0, sizeof(buf));
	buf_size = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	if (buf[0] == 'd') {
		dev_info(&i2c_client->dev, "%s, set daymode\n", __func__);
		max96712_read_reg(priv, 0x0010, &val);
		return count;
	}

	if (buf[0] == 'n') {
		dev_info(&i2c_client->dev, "%s, set nightmode\n", __func__);
		return count;
	}


	return count;
}


static const struct file_operations max96712_debugfs_fops = {
	.open = max96712_debugfs_open,
	.read = seq_read,
	.write = max96712_debugfs_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int max96712_debugfs_init(const char *dir_name,
				struct dentry **d_entry,
				struct dentry **f_entry,
				struct max96712 *priv)
{
	struct dentry  *dp, *fp;
	char dev_name[20];
	struct i2c_client *i2c_client = priv->i2c_client;
	int err = 0;

	err = snprintf(dev_name, sizeof(dev_name), "max96712_%d", priv->device_id);
	if (err < 0)
		return -EINVAL;

	global_priv[priv->device_id] = priv;

	dev_dbg(&i2c_client->dev, "%s: device %d\n", __func__, priv->device_id);

	dp = debugfs_create_dir(dev_name, NULL);
	if (dp == NULL) {
		dev_err(&i2c_client->dev, "%s: debugfs create dir failed\n",
			__func__);
		return -ENOMEM;
	}

	fp = debugfs_create_file("max96712", S_IRUGO|S_IWUSR,
		dp, priv, &max96712_debugfs_fops);
	if (!fp) {
		dev_err(&i2c_client->dev, "%s: debugfs create file failed\n",
			__func__);
		debugfs_remove_recursive(dp);
		return -ENOMEM;
	}

	if (d_entry)
		*d_entry = dp;
	if (f_entry)
		*f_entry = fp;
	return 0;
}

static  struct regmap_config max96712_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_NONE,
};

static int max96712_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct max96712 *priv;
	int err = 0;

	//dev_info(&client->dev, "%s: enter\n", __func__);

	priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
	priv->i2c_client = client;
	priv->regmap = devm_regmap_init_i2c(priv->i2c_client,
				&max96712_regmap_config);
	if (IS_ERR(priv->regmap)) {
		dev_err(&client->dev,
			"regmap init failed: %ld\n", PTR_ERR(priv->regmap));
		return -ENODEV;
	}

	priv->device_id = id->driver_data;

	err = max96712_debugfs_init(NULL, NULL, NULL, priv);
	if (err)
		return err;

	/*set daymode by fault*/
	dev_info(&client->dev, "%s: probe success\n", __func__);

	return err;
}


static int
max96712_remove(struct i2c_client *client)
{

	// if (client != NULL) {
	// 	i2c_unregister_device(client);
	// 	client = NULL;
	// }

	return 0;
}

static const struct i2c_device_id max96712_id[] = {
	{ "max96712_0", 0 },
	{ "max96712_1", 1 },
	{ "max96712_2", 2 },
	{ "max96712_3", 3 },
	{ },
};


MODULE_DEVICE_TABLE(i2c, max96712_id);

static struct i2c_driver max96712_i2c_driver = {
	.driver = {
		.name = "max96712",
		.owner = THIS_MODULE,
	},
	.probe = max96712_probe,
	.remove = max96712_remove,
	.id_table = max96712_id,
};

static int __init max96712_init(void)
{
	return i2c_add_driver(&max96712_i2c_driver);
}

static void __exit max96712_exit(void)
{
	i2c_del_driver(&max96712_i2c_driver);
}

module_init(max96712_init);
module_exit(max96712_exit);

MODULE_DESCRIPTION("IO Expander driver max96712");
MODULE_AUTHOR("NVIDIA Corporation");
MODULE_LICENSE("GPL v2");
