/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/hqsysfs.h>
#include <linux/proc_fs.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
/*kirby code for KIRBYU-540 by liwk10 at 2024/4/24 start*/
#include <linux/string.h>
#include "smem_type.h"
#include <linux/soc/qcom/smem.h>
/*kirby code for KIRBYU-540 by liwk10 at 2024/4/24 end*/
#define PROC_BOOT_REASON_FILE "boot_status"
#define HQ_SYS_FS_VER "2024-04-15 V0.2"

/*kirby code for KIRBYU-540 by liwk10 at 2024/4/24 start*/
#define BOARD_INFO_LEN 64
char board_info[BOARD_INFO_LEN] = {0};
/*kirby code for KIRBYU-540 by liwk10 at 2024/4/24 end*/
static HW_INFO(HWID_VER, ver);
static HW_INFO(HWID_SUMMARY, hw_summary);
static HW_INFO(HWID_LCM, lcm);
static HW_INFO(HWID_CTP, ctp);
static HW_INFO(HWID_SUB_CAM, sub_cam);      //sub
static HW_INFO(HWID_SUB_CAM_2, main0_cam);  //main0
static HW_INFO(HWID_MAIN_CAM, main1_cam);  //main1
static HW_INFO(HWID_MAIN_CAM_2, main2_cam);  //main2
static HW_INFO(HWID_MAIN_CAM_3, main3_cam);  //main3
static HW_INFO(HWID_MAIN_LENS, main_cam_len);
static HW_INFO(HWID_FLASHLIGHT, flashlight);
static HW_INFO(HWID_GSENSOR, gsensor);
static HW_INFO(HWID_RGB, rgb);
static HW_INFO(HWID_TOF, tof);
static HW_INFO(HWID_SAR, sar);
static HW_INFO(HWID_HALL, hall);
static HW_INFO(HWID_ALSPS, alsps);
static HW_INFO(HWID_MSENSOR, msensor);
static HW_INFO(HWID_GYRO, gyro);
static HW_INFO(HWID_IRDA, irda);
static HW_INFO(HWID_FUEL_GAUGE_IC, fuel_gauge_ic);
static HW_INFO(HWID_NFC, nfc);
static HW_INFO(HWID_FP, fingerprint);
static HW_INFO(HWID_PCBA, pcba_config);

static struct attribute *huaqin_attrs[] = {
	&hw_info_ver.attr,
	&hw_info_hw_summary.attr,
	&hw_info_lcm.attr,
	&hw_info_ctp.attr,
	&hw_info_sub_cam.attr,
	&hw_info_main0_cam.attr,
	&hw_info_main1_cam.attr,
	&hw_info_main2_cam.attr,
	&hw_info_main3_cam.attr,
	&hw_info_main_cam_len.attr,
	&hw_info_flashlight.attr,
	&hw_info_gsensor.attr,
	&hw_info_rgb.attr,
	&hw_info_tof.attr,
	&hw_info_sar.attr,
	&hw_info_hall.attr,
	&hw_info_alsps.attr,
	&hw_info_msensor.attr,
	&hw_info_gyro.attr,
	&hw_info_irda.attr,
	&hw_info_fuel_gauge_ic.attr,
	&hw_info_nfc.attr,
	&hw_info_fingerprint.attr,
	&hw_info_pcba_config.attr,
	NULL
};


/*kirby code for KIRBYU-540 by liwk10 at 2024/4/24 start*/
static void read_pcba_config_from_smem(void)
{
	char *info = NULL;
	size_t size;
	pr_info("%s!\n", __func__);
	pr_info("SMEM_ID_VENDOR1 is %d\n", SMEM_ID_VENDOR1);
	info = (char *)qcom_smem_get(QCOM_SMEM_HOST_ANY, SMEM_ID_VENDOR1, &size);
	if (info != NULL) {
        memcpy(board_info, info, strlen(info) + 1);
    } else {
        pr_info("Failed to get info from SMEM\n");
	strcpy(board_info, "unkown info");
    }
	pr_info("_board_info%s!\n", board_info);
	pr_info("info%s!\n", info);
}
/*kirby code for KIRBYU-540 by liwk10 at 2024/4/24 end*/

static ssize_t huaqin_show(struct kobject *kobj, struct attribute *a, char *buf)
{
	ssize_t count = 0;
	//int i = 0;
	struct hw_info *hw = container_of(a, struct hw_info, attr);
	if (NULL == hw) {
		return sprintf(buf, "Data error\n");
	}
	if (HWID_VER == hw->hw_id) {
		count = sprintf(buf, "%s\n", HQ_SYS_FS_VER);
	} else if (HWID_SUMMARY == hw->hw_id) {
		//iterate all device and output the detail
		int iterator = 0;
		struct hw_info *curent_hw = NULL;
		struct attribute *attr = huaqin_attrs[iterator];
		while (attr) {
			curent_hw = container_of(attr, struct hw_info, attr);
			iterator += 1;
			attr = huaqin_attrs[iterator];
			if (curent_hw->hw_exist && (NULL != curent_hw->hw_device_name)) {
				count += sprintf(buf+count, "%s: %s\n", curent_hw->attr.name, curent_hw->hw_device_name);
			}
		}
	/*kirby code for KIRBYU-540 by liwk10 at 2024/4/24 start*/
	} else if (HWID_PCBA == hw->hw_id) {
		count = sprintf(buf, "%s\n", board_info);
		pr_info("[%s]:board_info: %s\n", __func__, board_info);
		return count;
	/*kirby code for KIRBYU-540 by liwk10 at 2024/4/24 end*/
	} else {
		if (0 == hw->hw_exist) {
			count = sprintf(buf, "Not support\n");
		} else if (NULL == hw->hw_device_name) {
			count = sprintf(buf, "Installed with no device Name\n");
		} else {
			count = sprintf(buf, "%s\n", hw->hw_device_name);
		}
	}
	return count;
}

static ssize_t huaqin_store(struct kobject *kobj, struct attribute *a, const char *buf, size_t count)
{
	/*Kirby code for KIRBYU-954 by chenyushuai at 20240424 start*/
	char name[40];
	memset(name,'\0',sizeof(name));
	strcpy(name,buf);
	pr_info("%s %s \n", __func__,name);

	if (NULL != strstr(name, "Accelerometer"))
			hq_register_hw_info(HWID_GSENSOR,"lsm6dso_acc");
	if (NULL != strstr(name, "Gyroscope"))
			hq_register_hw_info(HWID_GYRO,"lsm6dso_gyro");
	if (NULL != strstr(name, "Magnetometer"))
			hq_register_hw_info(HWID_MSENSOR,"qmc6308");
	if (NULL != strstr(name, "RGB"))
			hq_register_hw_info(HWID_RGB,"tcs3408");
	if (NULL != strstr(name, "Alsps"))
			hq_register_hw_info(HWID_ALSPS,"stk33562");
	if (NULL != strstr(name, "TOF"))
			hq_register_hw_info(HWID_TOF,"ADS6102");
	if (NULL != strstr(name, "Sar"))
			hq_register_hw_info(HWID_SAR,"aw96305");
	if (NULL != strstr(name, "HALL"))
			hq_register_hw_info(HWID_HALL,"as1821");
	/*Kirby code for KIRBYU-954 by chenyushuai at 20240424 end*/
	return count;
}

/* huaqin object */
static struct kobject huaqin_kobj;

static const struct sysfs_ops huaqin_sysfs_ops = {
	.show = huaqin_show,
	.store = huaqin_store,
};
static struct attribute_group huaqin_group = {
 	.attrs = huaqin_attrs,
 };

/* huaqin type */
static struct kobj_type huaqin_ktype = {
	.sysfs_ops = &huaqin_sysfs_ops,
	.default_groups =(const struct attribute_group *[]) {
 		&huaqin_group,
 		NULL,
	},
};

/* huaqin device class */
static struct class  *huaqin_class;

static struct device *huaqin_hw_device;
int register_kboj_under_hqsysfs(struct kobject *kobj, struct kobj_type *ktype, const char *fmt, ...)
{
	return kobject_init_and_add(kobj, ktype, &(huaqin_hw_device->kobj), fmt);
}
EXPORT_SYMBOL_GPL(register_kboj_under_hqsysfs);

static int __init create_sysfs(void)
{
	int ret;
	/* create class (device model) */
	huaqin_class = class_create(THIS_MODULE, HUAQIN_CLASS_NAME);
	if (IS_ERR(huaqin_class)) {
		pr_err("%s fail to create class\n", __func__);
		return 0;
	}
	huaqin_hw_device = device_create(huaqin_class, NULL, MKDEV(0, 0), NULL, HUAIN_INTERFACE_NAME);
	if (IS_ERR(huaqin_hw_device)) {
		pr_warn("fail to create device\n");
		return 0;
	}
	/* add kobject */
	ret = kobject_init_and_add(&huaqin_kobj, &huaqin_ktype, &(huaqin_hw_device->kobj), HUAQIN_HWID_NAME);
	if (ret < 0) {
		pr_err("%s fail to add kobject\n", __func__);
		return ret;
	}
	return 0;
}

int hq_deregister_hw_info(enum hardware_id id, char *device_name)
{
	int ret = 0;
	int find_hw_id = 0;
	int iterator = 0;
	struct hw_info *hw = NULL;
	struct attribute *attr = huaqin_attrs[iterator];
	if (NULL == device_name) {
		pr_err("[%s]: device_name does not allow empty\n", __func__);
		ret = -2;
		goto err;
	}
	while (attr) {
		hw = container_of(attr, struct hw_info, attr);
		iterator += 1;
		attr = huaqin_attrs[iterator];
		if (NULL == hw) {
			continue;
		}
		if (id == hw->hw_id) {
			find_hw_id = 1;
			if (0 == hw->hw_exist) {
				pr_err("[%s]: device has not registed hw->id:0x%x . Cant be deregistered\n"
					, __func__
					, hw->hw_id);
				ret = -4;
				goto err;
			} else if (NULL == hw->hw_device_name) {
				pr_err("[%s]:hw_id is 0x%x Device name cant be NULL\n"
					, __func__
					, hw->hw_id);
				ret = -5;
				goto err;
			} else {
				if (0 == strncmp(hw->hw_device_name, device_name, strlen(hw->hw_device_name))) {
					hw->hw_device_name = NULL;
					hw->hw_exist = 0;
				} else {
					pr_err("[%s]: hw_id is 0x%x Registered device name %s , want to deregister: %s\n"
						, __func__
						, hw->hw_id
						, hw->hw_device_name
						, device_name);
					ret = -6;
					goto err;
				}
			}
			goto err;
		} else
			continue;
	}
	if (0 == find_hw_id) {
		pr_err("[%s]: Cant find correct hardware_id: 0x%x\n", __func__, id);
		ret = -3;
	}
err:
	return ret;
}

int hq_register_hw_info(enum hardware_id id, char *device_name)
{
	int ret = 0;
	int find_hw_id = 0;
	int iterator = 0;
	struct hw_info *hw = NULL;
	struct attribute *attr = huaqin_attrs[iterator];
	if (NULL == device_name) {
		pr_err("[%s]: device_name does not allow empty\n", __func__);
		ret = -2;
		goto err;
	}
	while (attr) {
		hw = container_of(attr, struct hw_info, attr);
		iterator += 1;
		attr = huaqin_attrs[iterator];
		if (NULL == hw) {
			continue;
		}
		if (id == hw->hw_id) {
			find_hw_id = 1;
			if (hw->hw_exist) {
				pr_err("[%s]: device has already registed hw->id:0x%x hw_device_name:%s\n"
					, __func__
					, hw->hw_id
					, hw->hw_device_name);
				ret = -4;
				goto err;
			}
			switch (hw->hw_id) {
				/*
					if (map_cam_drv_to_vendor(device_name))
						hw->hw_device_name = map_cam_drv_to_vendor(device_name);
					else
						hw->hw_device_name = "Can't find Camera Vendor";
					break;
				*/
			default:
					hw->hw_device_name = device_name;
					break;
			}
			hw->hw_exist = 1;
			goto err;
		} else
			continue;
	}
	if (0 == find_hw_id) {
		pr_err("[%s]: Cant find correct hardware_id: 0x%x\n", __func__, id);
		ret = -3;
	}
err:
	return ret;
}
EXPORT_SYMBOL_GPL(hq_register_hw_info);
static struct proc_dir_entry *boot_reason_proc;

static unsigned int boot_into_factory;

static int boot_reason_proc_show(struct seq_file *file, void *data)
{
	char temp[40] = {0};
	sprintf(temp, "%d\n", boot_into_factory);
	seq_printf(file, "%s\n", temp);
	return 0;
}

static int boot_reason_proc_open (struct inode *inode, struct file *file)
{
	return single_open(file, boot_reason_proc_show, inode->i_private);
}

static const struct proc_ops boot_reason_proc_fops = {
	.proc_open = boot_reason_proc_open,
	.proc_read = seq_read,
};

static int __init hq_harware_init(void)
{
	/* create sysfs entry at /sys/class/huaqin/interface/hw_info */
	create_sysfs();
	/*kirby code for KIRBYU-627 by liwk10 at 2024/4/24 start*/
	read_pcba_config_from_smem();
	/*kirby code for KIRBYU-627 by liwk10 at 2024/4/24 end*/
	boot_reason_proc = proc_create(PROC_BOOT_REASON_FILE, 0644, NULL, &boot_reason_proc_fops);
	if (boot_reason_proc == NULL) {
		pr_err("[%s]: create_proc_entry boot_reason_proc failed\n", __func__);
	}
	return 0;
}

core_initcall(hq_harware_init);
MODULE_AUTHOR("gaobowei@huaqin.com");
MODULE_DESCRIPTION("Huaqin Hardware Info Driver");
MODULE_LICENSE("GPL");
