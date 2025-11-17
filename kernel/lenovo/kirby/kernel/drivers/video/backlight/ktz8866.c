/*
 * KTZ Semiconductor ktz8866 LED Driver
 *
 * Copyright (C) 2013 Ideas on board SPRL
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/ktz8866.h>
#define BL_I2C_ADDRESS			  0x11
#define u8	unsigned char
#define LCD_BL_I2C_ID_NAME "ktz8866"

static DEFINE_MUTEX(store_lock);
int ktz8866_id = 0;
EXPORT_SYMBOL(ktz8866_id);
static struct ktz8866_data *ktz8866_driver_data;

static DEFINE_MUTEX(read_lock);
static struct i2c_client *lcd_bl_i2c_client;
static int lcd_bl_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static void lcd_bl_i2c_remove(struct i2c_client *client);

static int lcd_bl_write_byte(unsigned char addr, unsigned char value)
{
	int ret = 0;
	unsigned char write_data[2] = {0};

	write_data[0] = addr;
	write_data[1] = value;

	if (NULL == lcd_bl_i2c_client) {
		dev_warn(&lcd_bl_i2c_client->dev, "[LCD][BL] lcd_bl_i2c_client is null!!\n");
		return -EINVAL;
	}
	ret = i2c_master_send(lcd_bl_i2c_client, write_data, 2);

	if (ret < 0)
		dev_warn(&lcd_bl_i2c_client->dev, "[LCD][BL] i2c write data fail !!\n");

	return ret;
}

static int lcd_bl_read_byte(u8 regnum)
{
	u8 buffer[1], reg_value[1];
	int res = 0;

	if (NULL == lcd_bl_i2c_client) {
		dev_warn(&lcd_bl_i2c_client->dev, "[LCD][BL] lcd_bl_i2c_client is null!!\n");
		return -EINVAL;
	}

	mutex_lock(&read_lock);

	buffer[0] = regnum;
	res = i2c_master_send(lcd_bl_i2c_client, buffer, 0x1);
	if (res <= 0)	{
	  mutex_unlock(&read_lock);
	  dev_warn(&lcd_bl_i2c_client->dev, "read reg send res = %d\n", res);
	  return res;
	}
	res = i2c_master_recv(lcd_bl_i2c_client, reg_value, 0x1);
	if (res <= 0) {
	  mutex_unlock(&read_lock);
	  dev_warn(&lcd_bl_i2c_client->dev, "read reg recv res = %d\n", res);
	  return res;
	}
	mutex_unlock(&read_lock);

	return reg_value[0];
}

static const struct of_device_id i2c_of_match[] = {
	{ .compatible = "ktz,ktz8866", },
	{},
};

static const struct i2c_device_id lcd_bl_i2c_id[] = {
	{LCD_BL_I2C_ID_NAME, 0},
	{},
};

static struct i2c_driver lcd_bl_i2c_driver = {
/************************************************************
Attention:
Althouh i2c_bus do not use .id_table to match, but it must be defined,
otherwise the probe function will not be executed!
************************************************************/
	.id_table = lcd_bl_i2c_id,
	.probe = lcd_bl_i2c_probe,
	.remove = lcd_bl_i2c_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = LCD_BL_I2C_ID_NAME,
		.of_match_table = i2c_of_match,
	},
};

int dsi_panel_update_backlight_kirby(int value)//for set bringhtness
{
	//if(printk_ratelimit())
	dev_warn(&lcd_bl_i2c_client->dev, "8866 bl = %d\n", value);

	if (value < 0) {
		dev_warn(&lcd_bl_i2c_client->dev, "value=%d\n", value);
		return 0;
	}

	if (value > 0) {
		//lcd_bl_write_byte(0x08, 0x5F); /* BL enabled and Current sink 1/2/3/4 /5 enabled；*/
		lcd_bl_write_byte(KTZ8866_DISP_BB_LSB, value & 0x07);// lsb
		lcd_bl_write_byte(KTZ8866_DISP_BB_MSB, (value >> 3) & 0xFF);// msb
	} else {
		lcd_bl_write_byte(KTZ8866_DISP_BB_LSB, 0x00);// lsb
		lcd_bl_write_byte(KTZ8866_DISP_BB_MSB, 0x00);// msb
		//lcd_bl_write_byte(0x08, 0x00); /* BL disabled and Current sink 1/2/3/4 /5 enabled；*/
	}

	return 0;
}
EXPORT_SYMBOL_GPL(dsi_panel_update_backlight_kirby);

static int ktz8866_id_read(void)
{
	int id_a = 0;
	id_a = lcd_bl_read_byte(ktz8866_regs[0].reg);//read ktz8866 id reg
	if (id_a <= 0)	{
	  dev_warn(&lcd_bl_i2c_client->dev, "read error!!! id_a = %d\n", id_a);
	  return id_a;
	}
	pr_info("%s: read id_a = 0x%2x\n", __func__,id_a);
	return id_a;
}
/*****************************************************************************
 * for ktz8866 to registe
 *****************************************************************************/
static ssize_t ktz8866_registers_show (struct device *dev,
struct device_attribute *attr, char *buf)
{
	struct ktz8866_data *driver_data = i2c_get_clientdata(lcd_bl_i2c_client);
	int reg_count = 0;
	int i, n = 0;
	u8 value = 0;

	struct ktz8866_reg *regs_p;
	reg_count = sizeof(ktz8866_regs) / sizeof(ktz8866_regs[0]);
	regs_p = ktz8866_regs;
	if (atomic_read(&driver_data->suspended)) {
		pr_err("%s: can't read: driver suspended\nlcd_bl_write_byte", __func__);
		n += scnprintf(buf + n, PAGE_SIZE - n, "not support\n");
	} else {
		pr_info("%s: read all registers\n", __func__);
		for (i = 0, n = 0; i < reg_count; i++) {
			value = lcd_bl_read_byte(regs_p[i].reg);
			/*n += scnprintf(buf + n, PAGE_SIZE - n,
				"%-30s (0x%02x) = 0x%02X\n",
					regs_p[i].name, regs_p[i].reg, value);*/
			pr_info("%-30s (0x%02x) = 0x%02X\n", regs_p[i].name, regs_p[i].reg, value);
		}
		ktz8866_id = ktz8866_id_read();
		if(ktz8866_id == 0x21) {
			pr_info("The backlihgt ic is ktz8866!\n");
			n += scnprintf(buf + n, PAGE_SIZE - n, "ktz8866\n");
		} else {
			pr_err("The backlihgt ic read fail!\n");
			n += scnprintf(buf + n, PAGE_SIZE - n, "not support\n");
		}
	}

	return n;
}
static ssize_t ktz8866_registers_store (struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ktz8866_data *driver_data = i2c_get_clientdata(lcd_bl_i2c_client);
	unsigned reg;
	unsigned value;
	if (atomic_read(&driver_data->suspended)) {
		pr_err("%s: can't write: driver suspended\n", __func__);
		return -ENODEV;
	}
	sscanf(buf, "%02x %02x", &reg, &value);
	if (value > 0xFF)
		return -EINVAL;
	pr_info("%s: writing reg 0x%02x = 0x%02x\n", __func__, reg, value);
	mutex_lock(&store_lock);
	lcd_bl_write_byte(reg, (uint8_t)value);
	mutex_unlock(&store_lock);
	return size;
}
static DEVICE_ATTR(ktz8866, 0664, ktz8866_registers_show, ktz8866_registers_store);
static int ktz8866_common_init(struct i2c_client *client)
{
	struct ktz8866_data *driver_data = NULL;
	int ret = 0;
	/* We should be able to read and write byte data */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err ("%s: I2C_FUNC_I2C not supported\n", __func__);
		return -ENOTSUPP;
	}
	driver_data = kzalloc(sizeof(struct ktz8866_data), GFP_KERNEL);
	if (driver_data == NULL) {
		pr_err ("%s: driver_data kzalloc failed\n", __func__);
		return -ENOMEM;
	}
	memset (driver_data, 0, sizeof (*driver_data));
	driver_data->client = client;
	i2c_set_clientdata (client, driver_data);
	atomic_set(&driver_data->suspended, 0);
	driver_data->led_dev.name = LCD_BL_I2C_ID_NAME;
	ret = led_classdev_register (&client->dev, &driver_data->led_dev);
	if (ret) {
		pr_err("%s: led_classdev_register %s failed: %d\n", __func__, LCD_BL_I2C_ID_NAME, ret);
		goto led_classdev_register_failed;
	}
	pr_info("%s: register ktz8866 ok\n", __func__);
	ktz8866_driver_data = driver_data;
	ret = device_create_file(ktz8866_driver_data->led_dev.dev, &dev_attr_ktz8866);
	if (ret) {
	  pr_err("%s: ktz8866 unable to create suspend device file for %s: %d\n", __func__, LCD_BL_I2C_ID_NAME, ret);
	  goto device_create_file_failed;
	}
	dev_set_drvdata (&client->dev, ktz8866_driver_data);
	pr_info("%s-\n", __func__);
	return 0;
device_create_file_failed:
	device_remove_file (ktz8866_driver_data->led_dev.dev, &dev_attr_ktz8866);
	kfree (ktz8866_driver_data);
led_classdev_register_failed:
	led_classdev_unregister(&driver_data->led_dev);
	kfree (driver_data);
	return ret;
}

static int lcd_bl_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret;
	if (NULL == client) {
		dev_warn(&lcd_bl_i2c_client->dev, "[LCD][BL] i2c_client is NULL\n");
		return -EINVAL;
	}

	lcd_bl_i2c_client = client;

	dev_warn(&lcd_bl_i2c_client->dev, "bias_backlight, i2c address: %0x\n", client->addr);

	ret = lcd_bl_write_byte(KTZ8866_DISP_BC1, 0XDA); /* BL_CFG1；OVP=34V，线性调光，PWM enabled */
	ret = lcd_bl_write_byte(KTZ8866_DISP_OPTION2, 0x37); /* BL_OPTION2；电感4.7uH，BL_CURRENT_LIMIT 2.5A；*/
	ret = lcd_bl_write_byte(KTZ8866_DISP_FULL_CURRENT, 0xB8); /* Backlight Full-scale LED Current 23.6mA/CH；*/
	ret = lcd_bl_write_byte(KTZ8866_DISP_BL_ENABLE, 0x4F); /* BL enabled and Current sink 1/2/3/4 enabled；*/
	dev_warn(&lcd_bl_i2c_client->dev, "bias_backlight, backlight probe1200!\n");
	if (ret < 0) {
		dev_warn(&lcd_bl_i2c_client->dev, "I2C write reg is fail!");
		return -EINVAL;
	} else {
		dev_warn(&lcd_bl_i2c_client->dev, "I2C write reg is success!");
	}
	ret = ktz8866_common_init(lcd_bl_i2c_client);
	if (ret < 0) {
		dev_err(&lcd_bl_i2c_client->dev, "ktz8866 node creat fail!");
	} else {
		dev_warn(&lcd_bl_i2c_client->dev, "ktz8866 node create success!");
	}
	return 0;
}

static void lcd_bl_i2c_remove(struct i2c_client *client)
{
	struct ktz8866_data *ktz8866a_driver_data = i2c_get_clientdata(lcd_bl_i2c_client);
	struct ktz8866_data *driver_data = i2c_get_clientdata(lcd_bl_i2c_client);
	device_remove_file (ktz8866a_driver_data->led_dev.dev, &dev_attr_ktz8866);
	kfree (ktz8866a_driver_data);
	led_classdev_unregister(&driver_data->led_dev);
	kfree (driver_data);

	lcd_bl_i2c_client = NULL;
	i2c_unregister_device(client);
	return;
}

module_i2c_driver(lcd_bl_i2c_driver);
MODULE_AUTHOR("yuyang <yuyang8@huanqin.com>");
MODULE_DESCRIPTION("LCD BL I2C Driver");
MODULE_LICENSE("GPL");

