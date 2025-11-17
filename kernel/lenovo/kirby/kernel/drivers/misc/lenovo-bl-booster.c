/*
 * Copyright (C) 2024 Lenovo Inc.
 *
 * License Terms: GNU General Public License v2
 *
 * Simple driver for i2c backlight driver
 *
 * Author: Hang Chen <chenhang2@lenovo.com>
 */
#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/backlight.h>
#include <linux/lenovo-bl-booster.h>
#include <linux/device.h>

#define LOG_TAG "bl_booster: "
#define BKL_NAME "bl_ic_common"
/*#define BKL_LED_DEV "bkl-bl"*/

#define MAX_IC_CMD_FIELDS   5
#define MAX_IC_CMDS  20
#define MAX_BRIGHTNESS 2047
#define DEFAULT_BRIGHTNESS 1140
#define LCD_BACKLIGHT_IC_NAME_LEN 20
#define LCD_BACKLIGHT_IC_COMP_LEN 128

#define BL_BOOSTER_HW 

enum {
    GPIO_ID_HWEN        = 0,
    GPIO_ID_ENP         = 1,
    GPIO_ID_ENN         = 2,
    GPIO_ID_PWM         = 3,
    GPIO_ID_MAX,
};

enum bl_booster_cmd_mode_t {
    CMD_MODE_NONE       = 0,
    CMD_MODE_REG_READ   = 1,
    CMD_MODE_REG_WRITE  = 2,
    CMD_MODE_REG_UPDATE = 3,
    CMD_MODE_BL_SET_LSB = 4,
    CMD_MODE_BL_SET_MSB = 5,
    CMD_MODE_GPIO_GET   = 6,
    CMD_MODE_GPIO_SET   = 7,
    CMD_MODE_MAX,
};

struct bl_booster_ic_data;

struct bl_booster_ic_cmd {
    unsigned char mode; /* refer to bl_booster_cmd_mode_t. */
    unsigned char reg_addr;
    unsigned char val;
    unsigned char mask;
    unsigned int delay;
};

struct bl_booster_ic_cmdset {
    struct bl_booster_ic_cmd *cmds;
    unsigned int nr_cmds;
};

struct bl_booster_ic_ops {
    int (*init)(struct bl_booster_ic_data *id);
    int (*set_backlight)(struct bl_booster_ic_data *id, unsigned int level);
    int (*enable_vsp)(struct bl_booster_ic_data *id, bool enable);
    int (*enable_vsn)(struct bl_booster_ic_data *id, bool enable);
    int (*set_hbm_levels)(struct bl_booster_ic_data *id, unsigned int hbm_level);
    int (*enable_bl)(struct bl_booster_ic_data *id, bool enable);
    /*int (*en_backlight)(struct bl_booster_ic_data *id, unsigned int level);*/
    int (*bl_self_test)(struct bl_booster_ic_data *id);
    int (*check_backlight)(struct bl_booster_ic_data *id);
    const char *name;
};

struct bl_booster_ic_data {
    const char *name;
    int hwen_gpio;
    int enp_gpio;
    int enn_gpio;
    bool bias_enable;
    struct bl_booster_ic_cmdset init_cmds;
    struct bl_booster_ic_cmdset bl_cmds;
    struct bl_booster_ic_cmdset vsp_on_cmds;
    struct bl_booster_ic_cmdset vsp_off_cmds;
    struct bl_booster_ic_cmdset vsn_on_cmds;
    struct bl_booster_ic_cmdset vsn_off_cmds;
    struct bl_booster_ic_cmdset bl_on_cmds;
    struct bl_booster_ic_cmdset bl_off_cmds;

    bool present;

    struct i2c_client *client;
    struct bl_booster_ic_ops *chip_ops;

    unsigned int brt;
    unsigned int chip_index;
    unsigned int *hbm_levels;
    unsigned int num_hbm_levels;
    unsigned int current_hbm_level;
};

struct bl_booster_data {
    struct led_classdev led_dev;
    struct mutex lock;
    struct work_struct work;
    enum led_brightness brightness;
    bool enable;
    bool brt_code_enable;
    u16 *brt_code_table;
    unsigned int default_brightness;
    struct backlight_device *bl_dev;

    struct delayed_work delay_work;
    /* lenovo */
    unsigned int num_dev;
    const char *part_name;
    u16 bl_bitwidth;
    u16 bl_min_lvl;
    u16 bl_max_lvl;
    u16 bl_def_lvl;
    u8 bl_lsb_mask;
    u8 bl_msb_mask;
    bool valid;
    unsigned int brt;

    struct bl_booster_ic_data dev_0;
    struct bl_booster_ic_data dev_1;
};

static struct bl_booster_data g_bl_data;
static char compatible_name[LCD_BACKLIGHT_IC_COMP_LEN] = {0};
static struct of_device_id *new_match_table;
#if 0

static int bkl_bl_get_brightness(struct backlight_device *bl_dev)
{
    return bl_dev->props.brightness;
}
static inline int bkl_bl_update_status(struct backlight_device *bl_dev)
{
    struct bl_booster_data *drvdata = bl_get_data(bl_dev);
    int brt;

    if (bl_dev->props.state & BL_CORE_SUSPENDED)
        bl_dev->props.brightness = 0;

    brt = bl_dev->props.brightness;
    /*
     * Brightness register should always be written
     * not only register based mode but also in PWM mode.
     */
    return bkl_brightness_set(drvdata, brt);
}
int bkl_backlight_device_set_brightness(struct backlight_device *bl_dev,
        unsigned long brightness)
{
    int rc = -ENXIO;
    mutex_lock(&bl_dev->ops_lock);
    if (bl_dev->ops) {
        //struct bl_booster_data *drvdata = bl_get_data(bl_dev);
        if (brightness > bl_dev->props.max_brightness)
            brightness = bl_dev->props.max_brightness;
        pr_debug(" set brightness to %lu\n", brightness);
        bl_dev->props.brightness = brightness;
        rc = bkl_bl_update_status(bl_dev);
    }
    mutex_unlock(&bl_dev->ops_lock);
    //backlight_generate_event(bl_dev, BACKLIGHT_UPDATE_SYSFS);
    return rc;
}
EXPORT_SYMBOL(bkl_backlight_device_set_brightness);
static const struct backlight_ops bkl_bl_ops = {
    .update_status = bkl_bl_update_status,
    .get_brightness = bkl_bl_get_brightness,
};
static void __bkl_work(struct bl_booster_data *led,
        enum led_brightness value)
{
    mutex_lock(&led->lock);
    bkl_brightness_set(led, value);
    mutex_unlock(&led->lock);
}
static void bkl_set_brightness(struct led_classdev *led_cdev,
        enum led_brightness brt_val)
{
    struct bl_booster_data *drvdata;
    drvdata = container_of(led_cdev, struct bl_booster_data, led_dev);
    schedule_work(&drvdata->work);
}

#endif

static int bl_booster_read_reg(struct i2c_client *client, int reg, u8 *val)
{
    int ret;

    printk(LOG_TAG "%s\n", __func__);
    ret = i2c_smbus_read_byte_data(client, reg);
    if (ret < 0) {
        printk(LOG_TAG "%s: err %d\n", __func__, ret);
        return ret;
    }
    *val = ret;
    return ret;
}

static ssize_t bl_booster_ic_reg_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct bl_booster_ic_data *drvdata = dev_get_drvdata(dev);
    ssize_t len = 0;
    int i;
    unsigned char reg_val = 0;

    printk(LOG_TAG "%s\n", __func__);
    len += snprintf(buf+len, PAGE_SIZE-len, "    ");
    for (i = 0; i < 16; i++) {
        len += snprintf(buf+len, PAGE_SIZE-len, "%2x ", i);
    }

    for (i = 0; i < 48; i++) {
        if (0 == i%16) {
            len += snprintf(buf+len, PAGE_SIZE-len, "\n%02x: ", i);
        }
        if (bl_booster_read_reg(drvdata->client, i, &reg_val) < 0)
            len += snprintf(buf+len, PAGE_SIZE-len, "-- ");
        else
            len += snprintf(buf+len, PAGE_SIZE-len, "%02x ", reg_val);
    }
    len += snprintf(buf+len, PAGE_SIZE-len, "\n");

    return len;
}

static ssize_t bl_booster_ic_reg_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct bl_booster_ic_data *ic = dev_get_drvdata(dev);
    char addr = 0, val = 0;
    int ret = 0;

    if (sscanf(buf, "%x %x", &addr, &val) == 2) {
        printk(LOG_TAG "reg=%x, val=%x\n", addr, val);
        ret = i2c_smbus_write_byte_data(ic->client, addr, val);
        if (ret < 0)
            printk(LOG_TAG "write error\n");
    }
    return count;
}

static DEVICE_ATTR(chip_reg, 0664, bl_booster_ic_reg_show, bl_booster_ic_reg_store);
static struct attribute *bl_booster_attributes[] = {
    &dev_attr_chip_reg.attr,
    NULL
};

static struct attribute_group bl_booster_attribute_group = {
    .attrs = bl_booster_attributes
};
//add store for read and weite by wuning

static void bl_booster_work(struct work_struct *work)
{
    lcd_kit_enable_vsp(true);
    msleep(150);
    lcd_kit_enable_vsn(true);
    msleep(150);
    lcd_kit_set_brightness(2000);
}

int lcd_kit_set_brightness(unsigned int brightness)
{
    int ret = 0;

    printk(LOG_TAG "%s\n", __func__);
    if (!g_bl_data.valid)
       return -EINVAL;

    if (g_bl_data.dev_0.present && g_bl_data.dev_0.chip_ops && g_bl_data.dev_0.chip_ops->set_backlight)
        ret = g_bl_data.dev_0.chip_ops->set_backlight(&(g_bl_data.dev_0), brightness);

    if (g_bl_data.dev_1.present && g_bl_data.dev_1.chip_ops && g_bl_data.dev_1.chip_ops->set_backlight)
        ret |= g_bl_data.dev_1.chip_ops->set_backlight(&(g_bl_data.dev_1), brightness);

    return ret;
}
EXPORT_SYMBOL(lcd_kit_set_brightness);

int lcd_kit_enable_vsp(bool enable)
{
    int ret = 0;

    printk(LOG_TAG "%s\n", __func__);
    if (!g_bl_data.valid)
       return -EINVAL;
   
    if (g_bl_data.dev_0.present && g_bl_data.dev_0.chip_ops && g_bl_data.dev_0.chip_ops->enable_vsp)
        ret = g_bl_data.dev_0.chip_ops->enable_vsp(&(g_bl_data.dev_0), enable);

    if (g_bl_data.dev_1.present && g_bl_data.dev_1.chip_ops && g_bl_data.dev_1.chip_ops->enable_vsp)
        ret |= g_bl_data.dev_1.chip_ops->enable_vsp(&(g_bl_data.dev_1), enable);

    return ret;
}
EXPORT_SYMBOL(lcd_kit_enable_vsp);

int lcd_kit_enable_vsn(bool enable)
{
    int ret = 0;

    printk(LOG_TAG "%s\n", __func__);
    if (!g_bl_data.valid)
       return -EINVAL;

    if (g_bl_data.dev_0.present && g_bl_data.dev_0.chip_ops && g_bl_data.dev_0.chip_ops->enable_vsn)
        ret = g_bl_data.dev_0.chip_ops->enable_vsn(&(g_bl_data.dev_0), enable);

    if (g_bl_data.dev_1.present && g_bl_data.dev_1.chip_ops && g_bl_data.dev_1.chip_ops->enable_vsn)
        ret |= g_bl_data.dev_1.chip_ops->enable_vsn(&(g_bl_data.dev_1), enable);

    return ret;
}
EXPORT_SYMBOL(lcd_kit_enable_vsn);

int lcd_kit_set_hbm_level(unsigned int hbm_level)
{
    int ret = 0;
    static int last = 0xFF;
    bool enable = (hbm_level > 0);

    if ((int)enable != last) {
        printk(LOG_TAG "%s %s\n", __func__, enable? "on":"off");
        last = (int)enable;
    }
    if (!g_bl_data.valid)
       return -EINVAL;

    if (g_bl_data.dev_0.present && g_bl_data.dev_0.chip_ops && g_bl_data.dev_0.chip_ops->set_hbm_levels) {
        ret = g_bl_data.dev_0.chip_ops->set_hbm_levels(&(g_bl_data.dev_0), hbm_level);
        if (!ret)
            g_bl_data.dev_0.current_hbm_level = hbm_level;
    }
    if (g_bl_data.dev_1.present && g_bl_data.dev_1.chip_ops && g_bl_data.dev_1.chip_ops->set_hbm_levels) {
        ret |= g_bl_data.dev_1.chip_ops->set_hbm_levels(&(g_bl_data.dev_1), hbm_level);
        if (!ret)
            g_bl_data.dev_1.current_hbm_level = hbm_level;
    }
    return ret;
}
EXPORT_SYMBOL(lcd_kit_set_hbm_level);

int lcd_kit_get_hbm_level(void)
{
    if (g_bl_data.dev_0.present)
        return g_bl_data.dev_0.current_hbm_level;
    else
        return -EINVAL;
}
EXPORT_SYMBOL(lcd_kit_get_hbm_level);

int lcd_kit_enable_bl(bool enable)
{
    int ret = 0;

    printk(LOG_TAG "%s\n", __func__);
    if (!g_bl_data.valid)
       return -EINVAL;

    if (g_bl_data.dev_0.present && g_bl_data.dev_0.chip_ops && g_bl_data.dev_0.chip_ops->enable_bl)
        ret = g_bl_data.dev_0.chip_ops->enable_bl(&(g_bl_data.dev_0), enable);

    if (g_bl_data.dev_1.present && g_bl_data.dev_1.chip_ops && g_bl_data.dev_1.chip_ops->enable_bl)
        ret |= g_bl_data.dev_1.chip_ops->enable_bl(&(g_bl_data.dev_1), enable);

    return ret;
}
EXPORT_SYMBOL(lcd_kit_enable_bl);

static int bl_booster_ic_exec_cmd(struct bl_booster_ic_cmd *cmd, struct bl_booster_ic_data *ic)
{
    int ret = 0;
    printk(LOG_TAG "cmd: [%d, 0x%02X, 0x%02X] for %s\n", cmd->mode, cmd->reg_addr, cmd->val, ic->name);
#ifdef BL_BOOSTER_HW
    switch (cmd->mode) {
    case CMD_MODE_REG_WRITE:
    case CMD_MODE_BL_SET_MSB:
    case CMD_MODE_BL_SET_LSB:
        ret = i2c_smbus_write_byte_data(ic->client, cmd->reg_addr, cmd->val);
        if (ret < 0)
            printk(LOG_TAG "failed to write reg 0x%02X, ret=%d\n", cmd->reg_addr, ret);
        break;
    case CMD_MODE_GPIO_SET:
        if ((GPIO_ID_HWEN == cmd->reg_addr) && gpio_is_valid(ic->hwen_gpio)) {
            printk(LOG_TAG "%s set hwen-gpio to %d\n", __func__, cmd->val);
            gpio_set_value(ic->hwen_gpio, cmd->val);
        }
        if ((GPIO_ID_ENP == cmd->reg_addr) && gpio_is_valid(ic->enp_gpio)) {
            printk(LOG_TAG "%s set enp_gpio to %d\n", __func__, cmd->val);
            gpio_set_value(ic->enp_gpio, cmd->val);
        }
        if ((GPIO_ID_ENN == cmd->reg_addr) && gpio_is_valid(ic->enn_gpio)) {
            printk(LOG_TAG "%s set enn_gpio to %d\n", __func__, cmd->val);
            gpio_set_value(ic->enn_gpio, cmd->val);
        }
        break;
    default:
        printk(LOG_TAG "unsupported mode in %s\n", __func__);
    }
#endif
    if (cmd->delay) {
        usleep_range(cmd->delay, cmd->delay + cmd->delay/10);
    }
    return ret;
}

static int bl_booster_ic_exec_cmdset(struct bl_booster_ic_data *chip, struct bl_booster_ic_cmdset *cmdset)
{
    int ret = 0, i;

    for (i = 0; i < cmdset->nr_cmds; i++) {
        ret = bl_booster_ic_exec_cmd(cmdset->cmds + i, chip);
#ifdef BL_BOOSTER_HW 
        if (ret) // when error occurs, better to retry or stop, instead of stepping on.
            break;
#endif
    }

    return ret;
}

#ifdef BL_BOOSTER_HW
/* For chip that already put into work by UEFI, keep the pin status to 
 * avoid the breakage of power supply, or other flicker possibility.  */
static void bl_booster_ic_try_skip_gpio_init(int gpio_num, int expected_active, char *name)
{
    int old_val = gpio_get_value(gpio_num);
    if (old_val != expected_active)
        gpio_direction_output(gpio_num, !expected_active); 
    else {
        gpio_direction_output(gpio_num, expected_active); 
        printk(LOG_TAG "%s already active, skip.\n", name);
    }
}

static int bl_booster_ic_init_gpio(struct bl_booster_ic_data *chip)
{
    int ret = 0;
    char tmp[LCD_BACKLIGHT_IC_COMP_LEN] = {0};

    // TODO: init gpio state to low, but it might be active for specific chip.
    if (gpio_is_valid(chip->hwen_gpio)) {
        snprintf(tmp, LCD_BACKLIGHT_IC_COMP_LEN-1, "%s_hwen", chip->name);
        ret = gpio_request(chip->hwen_gpio, tmp);
        if (ret)
            printk(LOG_TAG "can not request hwen_gpio\n");
        else {
            bl_booster_ic_try_skip_gpio_init(chip->hwen_gpio, 1, "hwen_gpio");
        }
    }

    if (chip->bias_enable) {
        printk(LOG_TAG "init gpio for bias\n");
        memset(tmp, 0, sizeof(tmp));
        snprintf(tmp, LCD_BACKLIGHT_IC_COMP_LEN-1, "%s_enp", chip->name);
        if (gpio_is_valid(chip->enp_gpio)) {
            ret = gpio_request(chip->enp_gpio, tmp);
            if (ret)
                printk(LOG_TAG "can not request enp_gpio\n");
            else {
                bl_booster_ic_try_skip_gpio_init(chip->enp_gpio, 1, "enp_gpio");
            }
        }

        memset(tmp, 0, sizeof(tmp));
        snprintf(tmp, LCD_BACKLIGHT_IC_COMP_LEN-1, "%s_enn", chip->name);
        if (gpio_is_valid(chip->enn_gpio)) {
            ret = gpio_request(chip->enn_gpio, tmp);
            if (ret)
                printk(LOG_TAG "can not request enn_gpio\n");
            else {
                bl_booster_ic_try_skip_gpio_init(chip->enn_gpio, 1, "enn_gpio");
            }
        }
    }

    return 0;
}
#endif

static int bl_booster_ic_init(struct bl_booster_ic_data *chip)
{
    printk("%s, %s\n", __func__, chip->name);

#ifdef BL_BOOSTER_HW
    bl_booster_ic_init_gpio(chip);

    return bl_booster_ic_exec_cmdset(chip, &chip->init_cmds);
#else
    return 0;
#endif
}

static int bl_booster_ic_set_backlight(struct bl_booster_ic_data *chip, unsigned int level)
{
    int ret = 0, i;
    struct bl_booster_data *p = NULL;

    if (0 == chip->chip_index) 
        p = container_of(chip, struct bl_booster_data, dev_0);
    else if (1 == chip->chip_index)
        p = container_of(chip, struct bl_booster_data, dev_1);
    else
        return -1;

    level = (level < p->bl_min_lvl)? p->bl_min_lvl : level;
    level = (level > p->bl_max_lvl)? p->bl_max_lvl : level;
    printk("%s: %d, %s-%d\n", __func__, level, p->part_name, chip->chip_index);

    if (level != chip->brt) {
        // Fill backlight msb&lsb by level and relevant masks.
        for (i = 0; i < chip->bl_cmds.nr_cmds; i++) {
            struct bl_booster_ic_cmd *cmd = chip->bl_cmds.cmds + i;
            if (CMD_MODE_BL_SET_LSB == cmd->mode) {
                cmd->val = level & cmd->mask;
            } else if (CMD_MODE_BL_SET_MSB == cmd->mode) {
                cmd->val = level >> (p->bl_bitwidth-8) & cmd->mask;
            }
        }

        ret = bl_booster_ic_exec_cmdset(chip, &chip->bl_cmds);
        if (!ret)
            chip->brt = level;
    }

    return ret;
}

static int bl_booster_ic_enable_vsp(struct bl_booster_ic_data *chip, bool enable)
{
    if (NULL == chip || chip->present == false)
        return -EINVAL;

    if (enable)
        return bl_booster_ic_exec_cmdset(chip, &chip->vsp_on_cmds);
    else
        return bl_booster_ic_exec_cmdset(chip, &chip->vsp_off_cmds);
}

static int bl_booster_ic_enable_vsn(struct bl_booster_ic_data *chip, bool enable)
{
    if (NULL == chip || chip->present == false)
        return -EINVAL;

    if (enable)
        return bl_booster_ic_exec_cmdset(chip, &chip->vsn_on_cmds);
    else
        return bl_booster_ic_exec_cmdset(chip, &chip->vsn_off_cmds);
}

static int bl_booster_ic_enable_bl(struct bl_booster_ic_data *chip, bool enable)
{
    if (NULL == chip || chip->present == false)
        return -EINVAL;

    if (enable)
        return bl_booster_ic_exec_cmdset(chip, &chip->bl_on_cmds);
    else
        return bl_booster_ic_exec_cmdset(chip, &chip->bl_off_cmds);
}

static int bl_booster_parse_hbm_levels(struct device_node *np, struct bl_booster_ic_data *ic_data)
{
    int count, ret;

    pr_info("Parsing HBM levels\n");

    count = of_property_count_u32_elems(np, "hbm-levels");
    if (count <= 0) {
        pr_err("hbm-levels not found or invalid count = %d\n", count);
        return -EINVAL;
    }
    pr_info("hbm-levels count = %d\n", count);

    if (!ic_data->client) {
        pr_err("client pointer is null\n");
        return -EINVAL;
    }

    ic_data->hbm_levels = devm_kcalloc(&ic_data->client->dev, count, sizeof(u32), GFP_KERNEL);

    if (!ic_data->hbm_levels) {
        pr_err("Failed to allocate memory for HBM levels\n");
        return -ENOMEM;
    }

    ret = of_property_read_u32_array(np, "hbm-levels", ic_data->hbm_levels, count);
    if (ret) {
        pr_err("Failed to read hbm-levels\n");
        return ret;
    }

    ic_data->num_hbm_levels = count;
    return 0;
}

static int set_hbm_level(struct bl_booster_ic_data *ic_data, unsigned int level)
{
    int ret;

    if (!ic_data || !ic_data->client) {
        pr_err("Invalid device or I2C client\n");
        return -EINVAL;
    }

    if (level >= ic_data->num_hbm_levels) {
        pr_err("Invalid HBM level index: %u\n", level);
        return -EINVAL;
    }

    ret = i2c_smbus_write_byte_data(ic_data->client, 0x15, ic_data->hbm_levels[level]);
    if (ret < 0) {
        pr_err("Failed to write HBM level to device %s, I2C error: %d\n", ic_data->name, ret);
        return ret;
    }

    pr_debug("HBM level %u set successfully on device %s\n", level, ic_data->name);
    return 0;
}

static struct bl_booster_ic_ops bl_booster_chip_ops = {
    .init = bl_booster_ic_init,
    .set_backlight = bl_booster_ic_set_backlight,
    .enable_vsp = bl_booster_ic_enable_vsp,
    .enable_vsn = bl_booster_ic_enable_vsn,
    .set_hbm_levels = set_hbm_level,
    .enable_bl = bl_booster_ic_enable_bl,
    .name = "bl_common",
};

static int bl_booster_parse_cmn_config(struct bl_booster_data *bd)
{
    int ret = 0;
    struct device_node* np;

    np = of_find_node_by_name(NULL, "i2c_bl_booster");
    if (!np) {
        printk(LOG_TAG "fail_find_node_byname!!!\n");
        return -1;
    } else {
        printk(LOG_TAG "success to get node %s\n", np->name);
    }

    ret = of_property_read_u32(np, "num-dev", &bd->num_dev)
        || of_property_read_string(np, "part-number", &bd->part_name)
        || of_property_read_u16(np, "bl-bw", &bd->bl_bitwidth)
        || of_property_read_u16(np, "bl-min-lvl", &bd->bl_min_lvl)
        || of_property_read_u16(np, "bl-max-lvl", &bd->bl_max_lvl)
        || of_property_read_u16(np, "bl-def-lvl", &bd->bl_def_lvl)
        || of_property_read_u8(np, "bl-lsb-mask", &bd->bl_lsb_mask)
        || of_property_read_u8(np, "bl-msb-mask", &bd->bl_msb_mask);

    // data validation.
    if (ret || bd->num_dev > 2 || bd->bl_bitwidth < 8 || bd->bl_bitwidth > 16
            || bd->bl_min_lvl >= bd->bl_max_lvl || bd->bl_min_lvl > bd->bl_def_lvl || bd->bl_def_lvl > bd->bl_max_lvl
            || bd->bl_max_lvl > (((u32)1<< bd->bl_bitwidth) - 1) ) {
        ret = -1;
        printk(LOG_TAG "missing some fields or wrong format\n");
    } else {
        printk(LOG_TAG "num-dev: %d, part-name: %s\n", bd->num_dev, bd->part_name);
        printk(LOG_TAG "bl_config bw=%d, min=%d, def=%d, max=%d\n", 
                bd->bl_bitwidth, bd->bl_min_lvl, bd->bl_def_lvl, bd->bl_max_lvl);
        bd->valid = true;
    }
    
    return ret;
}

static int bl_booster_parse_ic_cmds(struct device_node *np, 
    const char *cmd_name, struct bl_booster_ic_cmdset *cmdset)
{
    int ret = 0;

    ret = of_property_count_elems_of_size(np, cmd_name, sizeof(u32));
    cmdset->nr_cmds = (ret <= 0)? 0 : ret;

    if (cmdset->nr_cmds 
        && !(cmdset->nr_cmds % MAX_IC_CMD_FIELDS)
        && (cmdset->nr_cmds/MAX_IC_CMD_FIELDS < MAX_IC_CMDS)) {
        u32* out_values = NULL;
        out_values = (u32 *)kmalloc(sizeof(u32) * cmdset->nr_cmds, GFP_KERNEL);
        if (NULL == out_values) {
            ret = -ENOMEM;
            goto BL_BOOSTER_IC_CONFIG_ERROR;
        }

        ret = of_property_read_u32_array(np, cmd_name, out_values, cmdset->nr_cmds);
        if (ret < 0) {
            printk(LOG_TAG "fail_read_u32_array!!!\n");
        } else {
            int i;
            cmdset->nr_cmds = cmdset->nr_cmds / MAX_IC_CMD_FIELDS;
            printk(LOG_TAG "num-%s: %d\n", cmd_name, cmdset->nr_cmds);
            cmdset->cmds = (struct bl_booster_ic_cmd *)kmalloc(sizeof(struct bl_booster_ic_cmd)
                    * cmdset->nr_cmds, GFP_KERNEL);
            if (NULL == cmdset->cmds) {
                ret = -ENOMEM;
            } else {
                for(i = 0; i < cmdset->nr_cmds; i++) {
                    if (out_values[i*MAX_IC_CMD_FIELDS] >= CMD_MODE_MAX || out_values[i*MAX_IC_CMD_FIELDS+1] > 255
                            || out_values[i*MAX_IC_CMD_FIELDS+2] > 255 || out_values[i*MAX_IC_CMD_FIELDS+3] > 255) {
                        ret = -1;
                        kfree(cmdset->cmds);
                        cmdset->cmds = NULL;
                        break;
                    }

                    cmdset->cmds[i].mode = *(out_values + i*MAX_IC_CMD_FIELDS);
                    cmdset->cmds[i].reg_addr = *(out_values + i*MAX_IC_CMD_FIELDS + 1);
                    cmdset->cmds[i].val = *(out_values + i*MAX_IC_CMD_FIELDS + 2);
                    cmdset->cmds[i].mask = *(out_values + i*MAX_IC_CMD_FIELDS + 3);
                    cmdset->cmds[i].delay = *(out_values + i*MAX_IC_CMD_FIELDS + 4);
                    printk(LOG_TAG "%s[%d]: %d 0x%02X 0x%02X 0x%02X 0x%X\n", cmd_name,
                            i, cmdset->cmds[i].mode,
                            cmdset->cmds[i].reg_addr, cmdset->cmds[i].val,
                            cmdset->cmds[i].mask, cmdset->cmds[i].delay); 
                }
            }
        }
        kfree(out_values);
        out_values = NULL;
    } else {
        printk(LOG_TAG "error: len of items in %s: %d\n", cmd_name, cmdset->nr_cmds);
    }

BL_BOOSTER_IC_CONFIG_ERROR:
    return ret;
}

static int bl_booster_parse_ic_config(struct bl_booster_ic_data *id, const struct i2c_client *client)
{
    struct device_node *np;
    int ret = 0;

    np = client->dev.of_node;
    memset(id, 0, sizeof(struct bl_booster_ic_data));
    id->client = (struct i2c_client *)client;

    id->bias_enable = of_property_read_bool(np, "lcd-bias-enable");
    printk(LOG_TAG "bias-enabled: %d\n", id->bias_enable);

    // mandatory to parse init/bl cmdset and hwen-gpio.
    ret = bl_booster_parse_ic_cmds(np, "init-cmds", &(id->init_cmds)) 
        || bl_booster_parse_ic_cmds(np, "bl-cmds", &id->bl_cmds)
        || !gpio_is_valid(id->hwen_gpio = of_get_named_gpio(np, "hwen-gpio", 0));
    if (ret) 
        goto BL_BOOSTER_IC_CONF_ERR;
 
    // parse bias relevant config, only when bias function was configured as on.
    if (id->bias_enable) {
        ret = bl_booster_parse_ic_cmds(np, "enable-vsp-cmds", &id->vsp_on_cmds)
            || bl_booster_parse_ic_cmds(np, "disable-vsp-cmds", &id->vsp_off_cmds)
            || bl_booster_parse_ic_cmds(np, "enable-vsn-cmds", &id->vsn_on_cmds)
            || bl_booster_parse_ic_cmds(np, "disable-vsn-cmds", &id->vsn_off_cmds)
            || !gpio_is_valid(id->enp_gpio = of_get_named_gpio(np, "enp-gpio", 0))
            || !gpio_is_valid(id->enn_gpio = of_get_named_gpio(np, "enn-gpio", 0));

        if (ret)
            goto BL_BOOSTER_IC_CONF_ERR;
    }

        ret = bl_booster_parse_ic_cmds(np, "enable-bl-cmds", &id->bl_on_cmds)
        || bl_booster_parse_ic_cmds(np, "disable-bl-cmds", &id->bl_off_cmds);
    if (ret)
        goto BL_BOOSTER_IC_CONF_ERR;

       ret = bl_booster_parse_hbm_levels(np, id);
       if (ret) {
         pr_err("Failed to parse HBM levels\n");
       }
    pr_info(LOG_TAG "hwen_gpio:%d, enp_gpio:%d, enn_gpio:%d\n", id->hwen_gpio, id->enp_gpio, id->enn_gpio);
    return ret;

BL_BOOSTER_IC_CONF_ERR:
    printk(LOG_TAG "critical ic config of %s absent!\n", np->name);
    return ret;
}

static int bl_booster_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    /*struct bl_booster_data *drvdata;*/
    /*struct backlight_device *bl_dev;*/
    /*struct backlight_properties props;*/
    int err = 0;
    /*char compatible_name[LCD_BACKLIGHT_IC_COMP_LEN] = {0};*/
    struct bl_booster_ic_data *ptr = NULL;

    pr_info("%s\n", __func__);
    if (!client->dev.of_node) {
        pr_err("%s: can't find common node!!!\n", __func__);
        err = -EINVAL;
    } else {
        pr_info("%s, %s\n", __func__, client->dev.of_node->name);

        snprintf(compatible_name, sizeof(compatible_name)-1, "%s-0", g_bl_data.part_name);
        if (!strncmp(compatible_name, client->dev.of_node->name, LCD_BACKLIGHT_IC_COMP_LEN-1)) {
            pr_info(LOG_TAG "attach dev_0 data to %s\n", compatible_name);
            ptr = &(g_bl_data.dev_0);
            err = bl_booster_parse_ic_config(ptr, client);
            if (!err) {
                ptr->client = client;
                ptr->chip_ops = &bl_booster_chip_ops;
                ptr->name = client->dev.of_node->name;
                ptr->chip_index = 0;
                i2c_set_clientdata(client, ptr);
            }
        }

        memset(compatible_name, 0, sizeof(compatible_name));
        snprintf(compatible_name, sizeof(compatible_name), "%s-1", g_bl_data.part_name);
        if (!strncmp(compatible_name, client->dev.of_node->name, LCD_BACKLIGHT_IC_COMP_LEN-1)) {
            pr_info(LOG_TAG "attach dev_1 data to %s\n", compatible_name);
            ptr = &(g_bl_data.dev_1);
            err = bl_booster_parse_ic_config(ptr, client);
            if ((!err)) {
                ptr->client = client;
                ptr->chip_ops = &bl_booster_chip_ops;
                ptr->name = client->dev.of_node->name;
                ptr->chip_index = 1;
                i2c_set_clientdata(client, ptr); 
            }
        }
    }

    if (!err && NULL != ptr) {
        err = (*ptr->chip_ops->init)(ptr);
        if (err) {
            // TODO: free all allocated resources? or just ignore it?
        } else {
            ptr->present = true;
            err = sysfs_create_group(&client->dev.kobj, &bl_booster_attribute_group);
            if (err < 0) {
                dev_info(&client->dev, "%s error creating sysfs attr files\n", __func__);
            }
        }
    }
#if 0
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        pr_err("%s :    I2C_FUNC_I2C not supported\n", __func__);
        err = -EIO;
        goto err_out;
    }
    pr_info("%s,%d\n", __func__, __LINE__);
    drvdata->client = client;
    drvdata->adapter = client->adapter;
    drvdata->addr = client->addr;
    drvdata->brightness = LED_OFF;
    drvdata->enable = true;
    drvdata->led_dev.default_trigger = "bkl-trigger";
    drvdata->led_dev.name = BKL_LED_DEV;
    drvdata->led_dev.brightness_set = bkl_set_brightness;
    drvdata->led_dev.max_brightness = MAX_BRIGHTNESS;
    mutex_init(&drvdata->lock);
    INIT_WORK(&drvdata->work, bkl_work);
    bkl_get_dt_data(&client->dev, drvdata);
    i2c_set_clientdata(client, drvdata);
    bkl_gpio_init(drvdata);
#if 0
    err = bkl_read_chipid(drvdata);
    if (err < 0) {
        pr_err("%s : ID idenfy failed\n", __func__);
        goto err_init;
    }
#endif
    err = led_classdev_register(&client->dev, &drvdata->led_dev);
    if (err < 0) {
        pr_err("%s :   Register led class failed\n", __func__);
        err = -ENODEV;
        goto err_init;
    } else {
        pr_debug("%s:   Register led class successful\n", __func__);
    }
    memset(&props, 0, sizeof(struct backlight_properties));
    props.type = BACKLIGHT_RAW;
    props.brightness = DEFAULT_BRIGHTNESS;
    props.max_brightness = MAX_BRIGHTNESS;
    bl_dev = backlight_device_register(BKL_NAME, &client->dev,
            drvdata, &bkl_bl_ops, &props);
    pr_info("%s,%d\n", __func__, __LINE__);
    g_bl_booster_data = drvdata;
    bkl_init_registers(drvdata);
    bkl_backlight_enable(drvdata);
    bkl_brightness_set(drvdata, DEFAULT_BRIGHTNESS);
    pr_info("%s   exit\n", __func__);
    return 0;
err_sysfs:
err_init:
    kfree(drvdata);
err_out:
#endif
    pr_info("%s done with err=%d.\n", __func__, err);
    return err;
}

static void bl_booster_remove(struct i2c_client *client)
{
    struct bl_booster_ic_data *id = i2c_get_clientdata(client);

    pr_info("%s\n", __func__);

    if (id->present) {
        if (id->bias_enable) {
            if (gpio_is_valid(id->enp_gpio)) {
                gpio_free(id->enp_gpio);
                id->enp_gpio = -1;
            }
            if (gpio_is_valid(id->enn_gpio)) {
                gpio_free(id->enn_gpio);
                id->enn_gpio = -1;
            }
        }

        if (gpio_is_valid(id->hwen_gpio)) {
            gpio_set_value(id->hwen_gpio, 0); // power off the chip.
            gpio_free(id->hwen_gpio);
            id->hwen_gpio = -1;
        }

        kfree(id->init_cmds.cmds);
        id->init_cmds.cmds = NULL;

        kfree(id->bl_cmds.cmds);
        id->bl_cmds.cmds = NULL;
        
        kfree(id->vsp_on_cmds.cmds);
        id->vsp_on_cmds.cmds = NULL;

        kfree(id->vsp_off_cmds.cmds);
        id->vsp_off_cmds.cmds = NULL;

        kfree(id->vsn_on_cmds.cmds);
        id->vsn_on_cmds.cmds = NULL;

        kfree(id->vsn_off_cmds.cmds);
        id->vsn_off_cmds.cmds = NULL;

        kfree(id->bl_on_cmds.cmds);
        id->bl_on_cmds.cmds = NULL;

        kfree(id->bl_off_cmds.cmds);
        id->bl_off_cmds.cmds = NULL;

        devm_kfree(&id->client->dev, id->hbm_levels);
        id->hbm_levels = NULL;

        id->present = false;
    }
    /*led_classdev_unregister(&drvdata->led_dev);*/
    /*kfree(drvdata);*/
}

static struct i2c_device_id lenovo_bl_booster_id[] = {
    { BKL_NAME, 0},
    {}
};

static struct of_device_id match_table[] = {
    { .compatible = BKL_NAME, },
    { },
};

MODULE_DEVICE_TABLE(i2c, lenovo_bl_booster_id);

static struct i2c_driver lenovo_bl_booster_driver = {
    .probe = bl_booster_probe,
    .remove = bl_booster_remove,
    .driver = {
        .name = BKL_NAME,
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(match_table),
    },
    .id_table = lenovo_bl_booster_id,
};

static int __init lenovo_bl_booster_init(void)
{
    int ret = 0, i;

    int len;
    /*struct class *backlight_class = NULL;*/

    pr_info("%s\n", __func__);
    memset(&g_bl_data, 0, sizeof(struct bl_booster_data));

    ret = bl_booster_parse_cmn_config(&g_bl_data);
    if (ret)
        return -EINVAL;

    new_match_table = kmalloc(sizeof(struct of_device_id) * (g_bl_data.num_dev+1), GFP_KERNEL);
    if (!new_match_table) {
        return -ENOMEM;
    }
    memset(new_match_table, 0, sizeof(struct of_device_id) * (g_bl_data.num_dev+1));
    lenovo_bl_booster_driver.driver.of_match_table = of_match_ptr(new_match_table);

    for (i = 0; i < g_bl_data.num_dev; i++) {
        snprintf(compatible_name, sizeof(compatible_name)-1, "lenovo,%s,%d", 
                g_bl_data.part_name, i);
        len = strlen(compatible_name);
        if (len >= LCD_BACKLIGHT_IC_COMP_LEN)
            len = LCD_BACKLIGHT_IC_COMP_LEN - 1;
        strncpy(new_match_table[i].compatible, compatible_name, len);
        new_match_table[i].compatible[LCD_BACKLIGHT_IC_COMP_LEN - 1] = 0;

    }
    pr_info("add i2c driver for %s\n", compatible_name);
    ret |= i2c_add_driver(&lenovo_bl_booster_driver);

    /*INIT_WORK(&g_bl_data.work, bkl_work);*/
    INIT_DELAYED_WORK(&g_bl_data.delay_work, bl_booster_work);
    /*schedule_delayed_work(&g_bl_data.delay_work, msecs_to_jiffies(5000));*/
#if 0
    backlight_class = class_create(THIS_MODULE, "lcd_backlight");
    if (IS_ERR(backlight_class)) {
        pr_info("Unable to create backlight class, errno = %ld\n", PTR_ERR(backlight_class));
        backlight_class = NULL;
    }
#endif
    return ret;
}
module_init(lenovo_bl_booster_init);

static void __exit lenovo_bl_booster_exit(void)
{
    pr_info("%s\n", __func__);

    if (!g_bl_data.valid)
        return;

    pr_info("%s: del driver \n", __func__);
    i2c_del_driver(&lenovo_bl_booster_driver);
    kfree(new_match_table);

    g_bl_data.valid = false;
}
module_exit(lenovo_bl_booster_exit);

MODULE_DESCRIPTION("Lenovo I2C BackLight driver 2024-04-28");
MODULE_LICENSE("GPL v2");
