// SPDX-License-Identifier: GPL-2.0-only

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/usb/ch9.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/regmap.h>
#include <linux/ctype.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/usb/redriver.h>

#ifdef dev_dbg
#undef dev_dbg
#endif
#define dev_dbg dev_err

#define NOTIFIER_PRIORITY		1

#define REDRIVER_REG_MAX		0xff
#define LANES_DP		4
#define LANES_DP_AND_USB	2

#define PULLUP_WORKER_DELAY_US	500000

#define CHIP_MAX_PWR_UA		260000
#define CHIP_MIN_PWR_UV		1710000
#define CHIP_MAX_PWR_UV		1890000

struct chip_id {
	u8 chip_id_1;
	u8 chip_id_2;
	u8 revision_1;
	u8 revision_2;
};

enum operation_mode {
	OP_MODE_NONE,		/* 4 lanes disabled */
	OP_MODE_USB,		/* 2 lanes for USB and 2 lanes disabled */
	OP_MODE_DP,		    /* 4 lanes DP */
	OP_MODE_USB_AND_DP,	/* 2 lanes for USB and 2 lanes DP */
	OP_MODE_DEFAULT,	/* 4 lanes USB */
};

struct ps5169_redriver {
	struct usb_redriver	r;
	struct device		*dev;
	struct regmap		*regmap;
	struct i2c_client	*client;
	struct regulator	*vdd;

	int typec_orientation;
	enum operation_mode op_mode;

	u8	gen_dev_val;
	bool	lane_channel_swap;
	bool	vdd_enable;
	bool	is_set_aux;

	struct workqueue_struct *pullup_wq;
	struct work_struct	pullup_work;
	bool			work_ongoing;

	struct work_struct	host_work;

	struct dentry	*debug_root;
	struct chip_id   id;
};

enum ps5169_debug_reg_list {
	PS5169_REG_MODE_CONFIG = 0x40,
	PS5169_REG_AUX_CHANNEL = 0xa0,
	PS5169_REG_HPD = 0xa1,

	// DP
	PS5169_REG_DP_EQ0 = 0x52,
	PS5169_REG_DP_EQ1 = 0x5e,
	PS5169_REG_DP_GAIN = 0x5c,

	//DPCD
	PS5169_REG_DPCD_100 = 0x20,
	PS5169_REG_DPCD_101 = 0x21,
	PS5169_REG_DPCD_103 = 0x22,
	PS5169_REG_DPCD_104 = 0x23,
	PS5169_REG_DPCD_105 = 0x24,
	PS5169_REG_DPCD_106 = 0x25,
	PS5169_REG_DPCD_600 = 0x24,

	//TX EQ
	PS5169_REG_TX_EQ0 = 0x50,
	PS5169_REG_TX_EQ1 = 0x5d,
	PS5169_REG_TX_EQ2 = 0x54,	  //TX EQ2:bit(7:4),	RX EQ2:bit(3:0)
	PS5169_REG_TX_GFAIN = 0x5c,

	//RX EQ
	PS5169_REG_RX_EQ0 = 0x51,	  // DCCM,ITAIL,RS manual:bit(7,2,1), EQ0_RX bit(2:0)
	PS5169_REG_RX_DCCM = 0x77,	 // DCCM eq:bit(5:4),	ITAIL eq:bit(7,6)
	PS5169_REG_RX_ITAIL = 0x78,	// ITAIL eq:bit(0),	New EQ1_RX:bit(7:5)
};

int dump_reg_list[] = {
	PS5169_REG_MODE_CONFIG,
	PS5169_REG_AUX_CHANNEL,
	PS5169_REG_HPD,
	PS5169_REG_DP_EQ0,
	PS5169_REG_DP_EQ1,
	PS5169_REG_DP_GAIN,

	//DPCD
	PS5169_REG_DPCD_100,
	PS5169_REG_DPCD_101,
	PS5169_REG_DPCD_103,
	PS5169_REG_DPCD_104,
	PS5169_REG_DPCD_105,
	PS5169_REG_DPCD_106,
	PS5169_REG_DPCD_600,

	//TX EQ
	PS5169_REG_TX_EQ0,
	PS5169_REG_TX_EQ1,
	PS5169_REG_TX_EQ2,	  //TX EQ2:bit(7:4),	RX EQ2:bit(3:0)
	PS5169_REG_TX_GFAIN,

	//RX EQ
	PS5169_REG_RX_EQ0,	  // DCCM,ITAIL,RS manual:bit(7,2,1), EQ0_RX bit(2:0)
	PS5169_REG_RX_DCCM,	 // DCCM eq:bit(5:4),	ITAIL eq:bit(7,6)
	PS5169_REG_RX_ITAIL,	// ITAIL eq:bit(0),	New EQ1_RX:bit(7:5)
};
struct ps5169_reg_sequence {
	u8 reg_addr;
	u8 reg_value;
	u8 wait_time;		// ms
};

struct ps5169_reg_sequence ps5169_default_seq[] = {
	{0x9d, 0x80, 0},
	{0, 0, 10},
	{0x9d, 0x00, 0},
	{0x40, 0x80, 0},	 // auto power down
	{0x04, 0x44, 0},	 // disable U1 status RX_Det
	{0xA0, 0x02, 0},	 // disable AUX channel
	{0x51, 0x87, 0},
	#ifdef CONFIG_ARCH_LAPIS
	{0x50, 0x10, 0},
	{0x54, 0x11, 0},
	{0x5d, 0x65, 0},
	{0x52, 0x50, 0},
	#else
	{0x50, 0x00, 0},
	{0x54, 0x01, 0},
	{0x5d, 0x66, 0},
	{0x52, 0x10, 0},// DP EQ 8.5dB
	#endif
	{0x55, 0x00, 0},
	{0x56, 0x00, 0},
	{0x57, 0x00, 0},
	{0x58, 0x00, 0},
	{0x59, 0x00, 0},
	{0x5a, 0x00, 0},
	{0x5b, 0x00, 0},
	#ifdef CONFIG_ARCH_KIRBY
	{0x5e, 0x05, 0},
	#else
	{0x5e, 0x06, 0},
	#endif
	{0x5f, 0x00, 0},
	{0x60, 0x00, 0},
	{0x61, 0x03, 0},
	{0x65, 0x40, 0},
	{0x66, 0x00, 0},
	{0x67, 0x03, 0},
	{0x75, 0x0c, 0},
	{0x77, 0x00, 0},
	{0x78, 0x7c, 0},
};

struct ps5169_reg_sequence ps5169_USB_seq[] = {
	{0x40, 0xc0, 0},	 // USB mode, no FLIP
	{0x8d, 0x01, 0},	 // Fine tune LFPS swing (force RX1 50ohm termination on)
	{0x90, 0x01, 0},	 // Fine tune LFPS swing (force RX2 50ohm termination on)
};

struct ps5169_reg_sequence ps5169_USB_FLIP_seq[] = {
	{0x40, 0xd0, 0},	 // USB mode, FLIP
	{0x8d, 0x01, 0},	 // Fine tune LFPS swing (force RX1 50ohm termination on)
	{0x90, 0x01, 0},	 // Fine tune LFPS swing (force RX2 50ohm termination on)
};

struct ps5169_reg_sequence ps5169_USB_remove_seq[] = {
	{0x40, 0x80, 0},	 // autp power down
	{0x8d, 0x00, 0},	 // RX1 50ohm termination off
	{0x90, 0x00, 0},	 // RX2 50ohm termination off
};

struct ps5169_reg_sequence ps5169_DP_seq[] = {
	{0x40, 0xa0, 0},	 // DP mode, no FLIP
	{0xa0, 0x00, 0},	 // Enable AUX channel
	{0xa1, 0x04, 0},	 // HPD
};

struct ps5169_reg_sequence ps5169_DP_FLIP_seq[] = {
	{0x40, 0xb0, 0},	 // DP mode, FLIP
	{0xa0, 0x00, 0},	 // Enable AUX channel
	{0xa1, 0x04, 0},	 // HPD
};

struct ps5169_reg_sequence ps5169_DP_remove_seq[] = {
	{0x40, 0x80, 0},	 // auto power down
	{0xa0, 0x02, 0},	 // Disable AUX channel
	{0xa1, 0x00, 0},	 // HPD low
};

struct ps5169_reg_sequence ps5169_USB_AND_DP_seq[] = {
	{0x40, 0xe0, 0},	 // DP mode, no FLIP
	{0x8d, 0x01, 0},	 // Fine tune LFPS swing (force RX1 50ohm termination on)
	{0x90, 0x01, 0},	 // Fine tune LFPS swing (force RX2 50ohm termination on)
	{0xa0, 0x00, 0},	 // Enable AUX channel
	{0xa1, 0x04, 0},	 // HPD
};

struct ps5169_reg_sequence ps5169_USB_AND_DP_FLIP_seq[] = {
	{0x40, 0xf0, 0},	 // DP mode, FLIP
	{0x8d, 0x01, 0},	 // Fine tune LFPS swing (force RX1 50ohm termination on)
	{0x90, 0x01, 0},	 // Fine tune LFPS swing (force RX2 50ohm termination on)
	{0xa0, 0x00, 0},	 // Enable AUX channel
	{0xa1, 0x04, 0},	 // HPD
};

struct ps5169_reg_sequence ps5169_USB_AND_DP_remove_seq[] = {
	{0x40, 0x80, 0},	 // auto power down
	{0xa0, 0x02, 0},	 // Disable AUX channel
	{0x8d, 0x00, 0},	 // RX1 50ohm termination off
	{0x90, 0x00, 0},	 // RX2 50ohm termination off
	{0xa1, 0x00, 0},	 // HPD low
};


static int redriver_i2c_reg_set(struct ps5169_redriver *redriver, u8 reg, u8 val);

static const char * const opmode_string[] = {
	[OP_MODE_NONE] = "NONE",
	[OP_MODE_USB] = "USB",
	[OP_MODE_DP] = "DP",
	[OP_MODE_USB_AND_DP] = "USB and DP",
	[OP_MODE_DEFAULT] = "DEFAULT",
};
#define OPMODESTR(x) opmode_string[x]

static const struct regmap_config redriver_regmap = {
	.name = "ps5169",
	.max_register = REDRIVER_REG_MAX,
	.reg_bits = 8,
	.val_bits = 8,
};

static bool redriver_exist = false;

bool get_redriver_exist(void) {
	return redriver_exist;
}
EXPORT_SYMBOL_GPL(get_redriver_exist);

static int redriver_i2c_reg_table_set(struct ps5169_redriver *redriver,
						struct ps5169_reg_sequence *table, int len)
{
	int i = 0;
	int ret = -EINVAL;
	if (IS_ERR_OR_NULL(table))
		return ret;

	for (i = 0; i < len; i++) {
		if (table[i].wait_time !=0 ) {
			msleep(table[i].wait_time);
			continue;
		}

		ret = redriver_i2c_reg_set(redriver, table[i].reg_addr, table[i].reg_value);
		if (ret < 0)
			dev_err(redriver->dev, "%s error: reg=0x%02x, value = 0x%02x\n",
									__func__, table[i].reg_addr, table[i].reg_value);
	}

	return ret;
}

static int redriver_i2c_reg_get(struct ps5169_redriver *redriver,
		u8 reg, u8 *val)
{
	int ret = 0;
	unsigned int val_tmp = 0;

	ret = regmap_read(redriver->regmap, (unsigned int)reg, &val_tmp);
	if (ret < 0) {
		dev_err(redriver->dev, "reading reg 0x%02x failure\n", reg);
		return ret;
	}

	*val = (u8)val_tmp;

	dev_dbg(redriver->dev, "reading reg 0x%02x=0x%02x\n", reg, *val);
	return ret;
}

static int redriver_i2c_reg_set(struct ps5169_redriver *redriver,
		u8 reg, u8 val)
{
	int ret = 0;

	ret = regmap_write(redriver->regmap, (unsigned int)reg,
			(unsigned int)val);
	if (ret < 0) {
		dev_err(redriver->dev, "writing reg 0x%02x failure\n", reg);
		return ret;
	}

	dev_dbg(redriver->dev, "writing reg 0x%02x=0x%02x\n", reg, val);
	return ret;
}

static int redriver_set_default_mode(struct ps5169_redriver *redriver)
{
	dev_info(redriver->dev, "%s:%d\n", __func__, __LINE__);

	return redriver_i2c_reg_table_set(redriver, ps5169_default_seq, ARRAY_SIZE(ps5169_default_seq));
}

static int redriver_set_USB_mode(struct ps5169_redriver *redriver)
{
	dev_info(redriver->dev, "%s:%d cc orientation = %d\n",
						__func__, __LINE__, redriver->typec_orientation);

	/* Spinel code for OSPINEL-911 by xuyc12 at 20230307 start */
	if (ORIENTATION_CC1 == redriver->typec_orientation)
		return redriver_i2c_reg_table_set(redriver, ps5169_USB_seq, ARRAY_SIZE(ps5169_USB_seq));
	else if(ORIENTATION_CC2 == redriver->typec_orientation)
		return redriver_i2c_reg_table_set(redriver, ps5169_USB_FLIP_seq, ARRAY_SIZE(ps5169_USB_FLIP_seq));

	dev_err(redriver->dev, "%s:%d unknow cc orientation: %d\n",
						__func__, __LINE__, redriver->typec_orientation);
	return -1;
	/* Spinel code for OSPINEL-911 by xuyc12 at 20230307 end */
}

static int redriver_set_DP_mode(struct ps5169_redriver *redriver)
{
	dev_info(redriver->dev, "%s:%d cc orientation = %d\n",
						__func__, __LINE__, redriver->typec_orientation);
	/* Spinel code for OSPINEL-911 by xuyc12 at 20230307 start */
	if (ORIENTATION_CC1 == redriver->typec_orientation)
		return redriver_i2c_reg_table_set(redriver, ps5169_DP_seq, ARRAY_SIZE(ps5169_DP_seq));
	else if(ORIENTATION_CC2 == redriver->typec_orientation)
		return redriver_i2c_reg_table_set(redriver, ps5169_DP_FLIP_seq, ARRAY_SIZE(ps5169_DP_FLIP_seq));

	dev_err(redriver->dev, "%s:%d unknow cc orientation:%d\n",
					__func__, __LINE__, redriver->typec_orientation);
	return -1;
	/* Spinel code for OSPINEL-911 by xuyc12 at 20230307 end */
}

static int redriver_set_USB_AND_DP_mode(struct ps5169_redriver *redriver)
{
	dev_info(redriver->dev, "%s:%d cc orientation = %d\n",
						__func__, __LINE__, redriver->typec_orientation);
	/* Spinel code for OSPINEL-911 by xuyc12 at 20230307 start */
	if (ORIENTATION_CC1 == redriver->typec_orientation)
		return redriver_i2c_reg_table_set(redriver, ps5169_USB_AND_DP_seq, ARRAY_SIZE(ps5169_USB_AND_DP_seq));
	else if(ORIENTATION_CC2 == redriver->typec_orientation)
		return redriver_i2c_reg_table_set(redriver, ps5169_USB_AND_DP_FLIP_seq, ARRAY_SIZE(ps5169_USB_AND_DP_FLIP_seq));

	dev_err(redriver->dev, "%s:%d unknow cc orientation:%d\n",
					__func__, __LINE__, redriver->typec_orientation);
	return -1;
	/* Spinel code for OSPINEL-911 by xuyc12 at 20230307 end */
}

static int redriver_remove_USB_mode(struct ps5169_redriver *redriver)
{
	dev_info(redriver->dev, "%s:%d\n", __func__, __LINE__);
	return redriver_i2c_reg_table_set(redriver, ps5169_USB_remove_seq, ARRAY_SIZE(ps5169_USB_remove_seq));
}

static int redriver_remove_DP_mode(struct ps5169_redriver *redriver)
{
	dev_info(redriver->dev, "%s:%d\n", __func__, __LINE__);
	return redriver_i2c_reg_table_set(redriver, ps5169_DP_remove_seq, ARRAY_SIZE(ps5169_DP_remove_seq));
}

static int redriver_remove_USB_AND_DP_mode(struct ps5169_redriver *redriver)
{
	dev_info(redriver->dev, "%s:%d\n", __func__, __LINE__);
	return redriver_i2c_reg_table_set(redriver, ps5169_USB_AND_DP_remove_seq, ARRAY_SIZE(ps5169_USB_AND_DP_remove_seq));
}

static void ps5169_redriver_gen_dev_set(
		struct ps5169_redriver *redriver, bool on)
{
	int ret = 0;

	switch (redriver->op_mode) {
	case OP_MODE_USB:
		if (on)
			ret = redriver_set_USB_mode(redriver);
		else
			ret = redriver_remove_USB_mode(redriver);

		if (ret < 0)
			goto err_exit;
		break;

	case OP_MODE_DP:
		if (on)
			ret = redriver_set_DP_mode(redriver);
		else
			ret = redriver_remove_DP_mode(redriver);

		if (ret < 0)
			goto err_exit;
		break;

	case OP_MODE_USB_AND_DP:
		if (on)
			ret = redriver_set_USB_AND_DP_mode(redriver);
		else
			ret = redriver_remove_USB_AND_DP_mode(redriver);

		if (ret < 0)
			goto err_exit;
		break;

	default:
		dev_err(redriver->dev, "Error: op mode: %d.\n", redriver->op_mode);
		goto err_exit;
	}

	return;

err_exit:
	dev_err(redriver->dev,
		"failure to (%s) the redriver chip, op mode: %d.\n",
		on ? "ENABLE":"DISABLE", redriver->op_mode);
	return;
}

static inline void orientation_set(struct ps5169_redriver *redriver, int ort)
{
	redriver->typec_orientation = ort;
}

static int ps5169_release_usb_lanes(struct usb_redriver *r, int ort, int num)
{
	struct ps5169_redriver *redriver =
		container_of(r, struct ps5169_redriver, r);

	dev_info(redriver->dev, "%s: mode %s, orientation %s-%d, lanes %d\n", __func__,
		OPMODESTR(redriver->op_mode), ort == ORIENTATION_CC1 ? "CC1" : "CC2",
		redriver->lane_channel_swap, num);

	if (num == LANES_DP)
		redriver->op_mode = OP_MODE_DP;
	else if (num == LANES_DP_AND_USB)
		redriver->op_mode = OP_MODE_USB_AND_DP;

	ps5169_redriver_gen_dev_set(redriver, true);
	orientation_set(redriver, ort);

	return 0;
}
static int ps5169_notify_connect(struct usb_redriver *r, int ort)
{
	struct ps5169_redriver *redriver =
		container_of(r, struct ps5169_redriver, r);

	dev_info(redriver->dev, "%s: mode %s, orientation %s, %d\n", __func__,
		OPMODESTR(redriver->op_mode),
		ort == ORIENTATION_CC1 ? "CC1" : "CC2",
		redriver->lane_channel_swap);


	if (redriver->op_mode == OP_MODE_NONE)
		redriver->op_mode = OP_MODE_USB;

	orientation_set(redriver, ort);

	ps5169_redriver_gen_dev_set(redriver, true);

	return 0;
}

static int ps5169_notify_disconnect(struct usb_redriver *r)
{
	struct ps5169_redriver *redriver =
		container_of(r, struct ps5169_redriver, r);

	dev_info(redriver->dev, "%s: mode %s\n", __func__,
		OPMODESTR(redriver->op_mode));

	if (redriver->op_mode == OP_MODE_NONE)
		return 0;

	ps5169_redriver_gen_dev_set(redriver, false);

	redriver->op_mode = OP_MODE_NONE;
	return 0;
}

static void ps5169_gadget_pullup_work(struct work_struct *w)
{
	struct ps5169_redriver *redriver =
		container_of(w, struct ps5169_redriver, pullup_work);

	ps5169_redriver_gen_dev_set(redriver, true);

	redriver->work_ongoing = false;
}

static void ps5169_host_work(struct work_struct *w)
{
	struct ps5169_redriver *redriver =
			container_of(w, struct ps5169_redriver, host_work);
	ps5169_redriver_gen_dev_set(redriver, true);
}

static int ps5169_gadget_pullup_exit(struct usb_redriver *r, int is_on)
{
	struct ps5169_redriver *redriver =
		container_of(r, struct ps5169_redriver, r);

	dev_info(redriver->dev, "%s: mode %s, %d, %d\n", __func__,
		OPMODESTR(redriver->op_mode), is_on, redriver->work_ongoing);

	if (redriver->op_mode != OP_MODE_USB)
		return -EINVAL;

	if (is_on)
		return 0;

	redriver->work_ongoing = true;
	queue_work(redriver->pullup_wq, &redriver->pullup_work);

	return 0;
}

static int ps5169_host_powercycle(struct usb_redriver *r)
{
	struct ps5169_redriver *redriver =
		container_of(r, struct ps5169_redriver, r);

	if (redriver->op_mode != OP_MODE_USB)
		return -EINVAL;

	schedule_work(&redriver->host_work);

	return 0;
}

static int ps5169_gadget_pullup_enter(struct usb_redriver *r, int is_on)
{
	struct ps5169_redriver *redriver =
		container_of(r, struct ps5169_redriver, r);
	u64 time = 0;

	dev_info(redriver->dev, "%s: mode %s, %d, %d\n", __func__,
		OPMODESTR(redriver->op_mode), is_on, redriver->work_ongoing);

	if (redriver->op_mode != OP_MODE_USB)
		return -EINVAL;

	if (!is_on)
		return 0;

	while (redriver->work_ongoing) {
		/*
		 * this function can work in atomic context, no sleep function here,
		 * it need wait pull down complete before pull up again.
		 */
		udelay(1);
		if (time++ > PULLUP_WORKER_DELAY_US) {
			dev_warn(redriver->dev, "pullup timeout\n");
			break;
		}
	}

	dev_info(redriver->dev, "pull-up disable work took %llu us\n", time);

	return 0;
}

static ssize_t register_config(struct file *file,
		const char __user *ubuf, size_t count, loff_t *ppos,
		int (*i2c_reg_set)(struct ps5169_redriver *redriver,
			u8 channel, u8 val))
{
	struct seq_file *s = file->private_data;
	struct ps5169_redriver *redriver = s->private;
	char buf[100];
	char *this_buf;
	u8 addr = 0, val = 0;

	memset(buf, 0, sizeof(buf));

	this_buf = buf;

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	while (this_buf != NULL) {
		if (sscanf(this_buf, "0x%02x=0x%02x", &addr, &val) == 2) {
			if (i2c_reg_set(redriver, addr, val) < 0) {
				dev_err(redriver->dev, "set register error: addr = %d, value = %d\n", addr, val);
			} else {
				dev_err(redriver->dev, "set register success: addr = %d, value = %d\n", addr, val);
			}
		} else {
			dev_err(redriver->dev, "cannot get register address or value\n");
		}
		strsep(&this_buf, " ");
	}
	return count;
}

static int update_register_open_l(struct seq_file *s, void *p)
{
	return 0;
}

static int update_register_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, update_register_open_l, inode->i_private);
}


static ssize_t update_register_write(struct file *file,
		const char __user *ubuf, size_t count, loff_t *ppos)
{
	return register_config(file, ubuf, count, ppos,
			redriver_i2c_reg_set);
}

static const struct file_operations update_register_ops = {
	.open	= update_register_open,
	.write	= update_register_write,
};

static int dump_register_open_l(struct seq_file *s, void *p)
{
	struct ps5169_redriver *redriver = s->private;
	u8 val = 0;
	int ret = 0, i = 0;

	for (i = 0; i < ARRAY_SIZE(dump_reg_list); i++) {
		ret = redriver_i2c_reg_get(redriver, dump_reg_list[i], &val);
		if (ret < 0) {
			seq_printf(s, "\nread 0x%02x failed\n", dump_reg_list[i]);
		} else {
			seq_printf(s, "0x%02x = 0x%02x\t", dump_reg_list[i], val);
			if ((i + 1) % 5 == 0)
				seq_printf(s, "\n");
		}
	}
	seq_printf(s, "\n");

	return 0;
}

static int dump_register_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, dump_register_open_l, inode->i_private);
}

static const struct file_operations dump_register_ops = {
	.open	= dump_register_open,
	.read	= seq_read,
};

static void ps5169_debugfs_entries(
		struct ps5169_redriver *redriver)
{
	struct dentry *ent = NULL;
	redriver->debug_root = debugfs_create_dir("ps5169_redriver", NULL);
	if (!redriver->debug_root) {
		dev_warn(redriver->dev, "Couldn't create debug dir\n");
		return;
	}
	ent = debugfs_create_file("dump_register", 0600,
			redriver->debug_root, redriver, &dump_register_ops);
	if (IS_ERR_OR_NULL(ent))
		dev_warn(redriver->dev, "Couldn't create dump_register file\n");

	ent = debugfs_create_file("update_register", 0600,
			redriver->debug_root, redriver, &update_register_ops);
	if (IS_ERR_OR_NULL(ent))
		dev_warn(redriver->dev, "Couldn't create update_register file\n");
}

static int redriver_get_chip_id(struct ps5169_redriver *redriver)
{
	memset(&redriver->id, 0 , sizeof(struct chip_id));

	if (redriver_i2c_reg_get(redriver, 0xad, &redriver->id.chip_id_1) < 0) {
		return -EINVAL;
	}
	if (redriver_i2c_reg_get(redriver, 0xac, &redriver->id.chip_id_2) < 0) {
		return -EINVAL;
	}
	if (redriver_i2c_reg_get(redriver, 0xae, &redriver->id.revision_1) < 0) {
		return -EINVAL;
	}
	if (redriver_i2c_reg_get(redriver, 0xaf, &redriver->id.revision_2) < 0) {
		return -EINVAL;
	}
	dev_info(redriver->dev, "get chip id: id_1 = 0x%02x, id_2 = 0x%02x, revision_1 = 0x%02x, revision_2 = 0x%02x\n",
								redriver->id.chip_id_1, redriver->id.chip_id_2,
								redriver->id.revision_1, redriver->id.revision_2);
	return 0;
}

static int ps5169_probe(struct i2c_client *client,
				   const struct i2c_device_id *dev_id)
{
	struct ps5169_redriver *redriver = NULL;
	int ret = 0;
	dev_info(&client->dev, "ps5169_probe enter\n");
	redriver = devm_kzalloc(&client->dev, sizeof(struct ps5169_redriver),
			GFP_KERNEL);
	if (!redriver)
		return -ENOMEM;

	redriver->pullup_wq = alloc_workqueue("%s:pullup",
				WQ_UNBOUND | WQ_HIGHPRI, 0,
				dev_name(&client->dev));
	if (!redriver->pullup_wq) {
		dev_err(&client->dev, "Failed to create pullup workqueue\n");
		return -ENOMEM;
	}

	redriver->regmap = devm_regmap_init_i2c(client, &redriver_regmap);
	if (IS_ERR(redriver->regmap)) {
		ret = PTR_ERR(redriver->regmap);
		dev_err(&client->dev,
			"Failed to allocate register map: %d\n", ret);
		return ret;
	}

	redriver->dev = &client->dev;
	i2c_set_clientdata(client, redriver);

	ret = redriver_get_chip_id(redriver);
	if (ret < 0) {
		dev_err(&client->dev,
			"Failed to get chip id: %d, countinue\n", ret);
	} else 
		redriver_exist = true;

	INIT_WORK(&redriver->pullup_work, ps5169_gadget_pullup_work);
	INIT_WORK(&redriver->host_work, ps5169_host_work);

	/* disable it at start, one i2c register write time is acceptable */
	redriver->op_mode = OP_MODE_NONE;

	ps5169_debugfs_entries(redriver);

	redriver->r.of_node = redriver->dev->of_node;
	redriver->r.release_usb_lanes = ps5169_release_usb_lanes;
	redriver->r.notify_connect = ps5169_notify_connect;
	redriver->r.notify_disconnect = ps5169_notify_disconnect;
	redriver->r.gadget_pullup_enter = ps5169_gadget_pullup_enter;
	redriver->r.gadget_pullup_exit = ps5169_gadget_pullup_exit;
	redriver->r.host_powercycle = ps5169_host_powercycle;

	usb_add_redriver(&redriver->r);

	redriver_set_default_mode(redriver);
	dev_info(&client->dev, "ps5169_probe success \n");
	return 0;
}
static void ps5169_remove(struct i2c_client *client)
{
	struct ps5169_redriver *redriver = i2c_get_clientdata(client);

	if (usb_remove_redriver(&redriver->r))
		return;

	debugfs_remove_recursive(redriver->debug_root);
	redriver->work_ongoing = false;
	destroy_workqueue(redriver->pullup_wq);

	if (redriver->vdd)
		regulator_disable(redriver->vdd);
}

static void ps5169_shutdown(struct i2c_client *client)
{
	struct ps5169_redriver *redriver = i2c_get_clientdata(client);

	/* Set back to USB mode with four channel enabled */
	ps5169_redriver_gen_dev_set(redriver, false);

	dev_info(&client->dev,
		"%s: successfully set back to USB mode.\n",
		__func__);
}

static const struct of_device_id ps5169_match_table[] = {
	{ .compatible = "hq_redriver,ps5169" },
	{ }
};

static struct i2c_driver ps5169_driver = {
	.driver = {
		.name	= "ssusb_redriver",
		.of_match_table	= ps5169_match_table,
	},

	.probe		= ps5169_probe,
	.remove		= ps5169_remove,
	.shutdown	= ps5169_shutdown,
};

module_i2c_driver(ps5169_driver);

MODULE_DESCRIPTION("USB Super Speed Linear Re-Driver");
MODULE_LICENSE("GPL");
