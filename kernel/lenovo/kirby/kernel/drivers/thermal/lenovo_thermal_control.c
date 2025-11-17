#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/module.h>

#define MAX_CMD 128

struct thermal_control {
	unsigned char control[MAX_CMD];
	struct attribute_group *attr_group;
	struct kobject *kobj;
};

static struct thermal_control control_info;

static ssize_t show_control(struct kobject *kobj,
               struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", control_info.control);
}

static ssize_t store_control(struct kobject *kobj,
               struct kobj_attribute *attr, const char *buf, size_t count)
{
	snprintf(control_info.control, MAX_CMD, "%s", buf);
	sysfs_notify(control_info.kobj, NULL, "control");
	return count;
}

static struct kobj_attribute control_attr =
	__ATTR(control, S_IWUSR | S_IRUSR, show_control,
			store_control);

static struct attribute *control_attrs[] = {
	&control_attr.attr,
	NULL,
};

static struct attribute_group control_attr_group = {
	.attrs = control_attrs,
};

static int init_control_attribs(void)
{
	int err;

	control_info.attr_group = &control_attr_group;

	/* Create /sys/devices/system/cpu/cpu0/thermal/... */
	control_info.kobj = kobject_create_and_add("thermal",
			&get_cpu_device(0)->kobj);
	if (!control_info.kobj)
		return -ENOMEM;

	err = sysfs_create_group(control_info.kobj, control_info.attr_group);
	if (err)
		kobject_put(control_info.kobj);
	else
		kobject_uevent(control_info.kobj, KOBJ_ADD);

	return err;
}

static int __init lenovo_thermal_control_stats_init(void)
{
	int ret;

	ret = init_control_attribs();

	return ret;
}
late_initcall(lenovo_thermal_control_stats_init);

MODULE_DESCRIPTION("Lenovo Thermal Control");
MODULE_LICENSE("GPL");
