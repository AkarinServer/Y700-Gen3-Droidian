#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/workqueue.h>

#define TEMP_NODE_SENSOR_NAMES "lenovo,temperature-names"
#define SENSOR_LISTENER_NAMES "lenovo,sensor-listener-names"
#define DEFAULT_TEMPERATURE 0
#define LENOVO_SYS_TEMP_NAME_LENGTH 20

#define MODEL_VERSION		6

struct lenovo_sys_temp_sensor {
	struct thermal_zone_device *tz_dev;
	const char *name;
	int temp;
};

struct lenovo_sys_temp_dev {
	struct platform_device *pdev;
	int num_sensors;
	int num_sensors_listener;
	struct lenovo_sys_temp_sensor *sensor;
	struct lenovo_sys_temp_sensor *sensor_listener;
	struct delayed_work temp_changed_work;
};

static struct lenovo_sys_temp_dev *sys_temp_dev;

static int lenovo_sys_temp_get(struct thermal_zone_device *thermal,
			    int *temp)
{
	struct lenovo_sys_temp_sensor *sensor = thermal->devdata;

	if (!sensor)
		return -EINVAL;

	*temp = sensor->temp;
	return 0;
}

static struct thermal_zone_device_ops lenovo_sys_temp_ops = {
	.get_temp = lenovo_sys_temp_get,
};

static int tz_init = 0;
static int tz_count = 0;
static void temp_changed_work_func(struct work_struct *work)
{
	int num_sensors_listener, i, desc = 0;
	char buf[1024];
	int front_temp, back_temp, user_front, user_backleft, user_backright, max_temp, user_max_temp, edge_temp = 0;
	int hbm_temp = 0;
	int delay_time = 200;
	int ret;
	int prod = 0;
	struct device_node *node;

	if (!sys_temp_dev)
		return;

	num_sensors_listener = sys_temp_dev->num_sensors_listener;

	if (!tz_init) {
		if (tz_count > 10)
			return;
		else
			tz_count++;

		node = sys_temp_dev->pdev->dev.of_node;
		for (i = 0; i < num_sensors_listener; i++) {
			ret = of_property_read_string_index(node,
							SENSOR_LISTENER_NAMES, i,
							&sys_temp_dev->sensor_listener[i].name);
			if (ret) {
				dev_err(&sys_temp_dev->pdev->dev, "Unable to read of_prop string\n");
				goto work_exit;
			}

			sys_temp_dev->sensor_listener[i].temp = DEFAULT_TEMPERATURE;
			if (sys_temp_dev->sensor_listener[i].name) {
				sys_temp_dev->sensor_listener[i].tz_dev =
				thermal_zone_get_zone_by_name(sys_temp_dev->sensor_listener[i].name);
				if (IS_ERR(sys_temp_dev->sensor_listener[i].tz_dev)) {
					dev_err(&sys_temp_dev->pdev->dev,
						"thermal_zone_get_zone_by_name() failed."
						"name %s, i %d\n", sys_temp_dev->sensor_listener[i].name, i);
					goto work_exit;
				}
			} else {
				dev_err(&sys_temp_dev->pdev->dev,
					"Invalid sensor listener name\n");
				goto work_exit;
			}
		}
		tz_init = 1;
	}

	for (i = 0; i < num_sensors_listener; i++) {
		if(sys_temp_dev->sensor_listener[i].tz_dev) {

			thermal_zone_get_temp(sys_temp_dev->sensor_listener[i].tz_dev,
					&sys_temp_dev->sensor_listener[i].temp);
		} else {
			dev_err(&sys_temp_dev->pdev->dev,
				"Invalid thermal zone\n");
			return;
		}
	}

	for (i = 0; i < num_sensors_listener; i++) {
#ifdef CONFIG_ARCH_LAPIS
		if (!strncasecmp(sys_temp_dev->sensor_listener[i].name,
				 "quiet-therm",
				 LENOVO_SYS_TEMP_NAME_LENGTH)) {
			hbm_temp = sys_temp_dev->sensor_listener[i].temp;
			//dev_info(&sys_temp_dev->pdev->dev, "lapis hbm_temp = %d\n", hbm_temp);
		}
#endif
		desc +=
			sprintf(buf + desc, "[%d]%s=%d ",
			i,
			sys_temp_dev->sensor_listener[i].name,
			sys_temp_dev->sensor_listener[i].temp);
	}

#ifdef CONFIG_ARCH_KIRBY
	prod = 1;
	front_temp = (sys_temp_dev->sensor_listener[0].temp * -598
			+ sys_temp_dev->sensor_listener[1].temp * 1846
			+ sys_temp_dev->sensor_listener[2].temp * 1788
			+ sys_temp_dev->sensor_listener[3].temp * -1652
			+ sys_temp_dev->sensor_listener[4].temp * 69
			+ sys_temp_dev->sensor_listener[5].temp * -417
			+ sys_temp_dev->sensor_listener[6].temp * -6
			+ 4706)
			/ 1000;
	front_temp += 1700;
	back_temp = (sys_temp_dev->sensor_listener[0].temp * 416
			+ sys_temp_dev->sensor_listener[1].temp * 1076
			+ sys_temp_dev->sensor_listener[2].temp * -105
			+ sys_temp_dev->sensor_listener[3].temp * -665
			+ sys_temp_dev->sensor_listener[4].temp * 144
			+ 2756386)
			/ 1000;
	back_temp += 1500;
	user_front = (sys_temp_dev->sensor_listener[0].temp * -983
			+ sys_temp_dev->sensor_listener[1].temp * 1685
			+ sys_temp_dev->sensor_listener[2].temp * 2183
			+ sys_temp_dev->sensor_listener[3].temp * -1653
			+ sys_temp_dev->sensor_listener[4].temp * -239
			+ 1194058)
			/ 1000;
	user_backleft = (sys_temp_dev->sensor_listener[0].temp * 625
			+ sys_temp_dev->sensor_listener[1].temp * 610
			+ sys_temp_dev->sensor_listener[2].temp * 13
			+ sys_temp_dev->sensor_listener[3].temp * -482
			+ sys_temp_dev->sensor_listener[4].temp * 90
			+ 3204978)
			/ 1000;
	user_backright = (sys_temp_dev->sensor_listener[0].temp * 697
			+ sys_temp_dev->sensor_listener[1].temp * 215
			+ sys_temp_dev->sensor_listener[2].temp * 107
			+ sys_temp_dev->sensor_listener[3].temp * -364
			+ sys_temp_dev->sensor_listener[4].temp * 138
			+ 4627101)
			/ 1000;
	max_temp = max(front_temp, back_temp);
	user_max_temp = max(user_front, user_backleft);
	user_max_temp = max(user_max_temp, user_backright);
	user_max_temp += 2300;
	hbm_temp = front_temp;
#else
	prod = 2;
	front_temp = (sys_temp_dev->sensor_listener[0].temp * 779
			+ sys_temp_dev->sensor_listener[1].temp * 230
			+ sys_temp_dev->sensor_listener[2].temp * -644
			+ sys_temp_dev->sensor_listener[3].temp * 1023
			+ sys_temp_dev->sensor_listener[4].temp * 688
			+ sys_temp_dev->sensor_listener[7].temp * -1188
			+ sys_temp_dev->sensor_listener[8].temp * 256
			+ sys_temp_dev->sensor_listener[9].temp * -960
			+ sys_temp_dev->sensor_listener[10].temp * 690
			+ 4258164)
			/ 1000;
	front_temp -= 1500;
	back_temp = (sys_temp_dev->sensor_listener[0].temp * 582
			+ sys_temp_dev->sensor_listener[1].temp * 131
			+ sys_temp_dev->sensor_listener[2].temp * -242
			+ sys_temp_dev->sensor_listener[3].temp * 67
			+ sys_temp_dev->sensor_listener[6].temp * -130
			+ sys_temp_dev->sensor_listener[7].temp * 249
			+ sys_temp_dev->sensor_listener[9].temp * 619
			+ sys_temp_dev->sensor_listener[10].temp * -522
			+ 6650193)
			/ 1000;
	back_temp -= 800;
	user_front = (sys_temp_dev->sensor_listener[0].temp * 478
			+ sys_temp_dev->sensor_listener[1].temp * 537
			+ sys_temp_dev->sensor_listener[2].temp * 308
			+ sys_temp_dev->sensor_listener[3].temp * 1904
			+ sys_temp_dev->sensor_listener[4].temp * -355
			+ sys_temp_dev->sensor_listener[7].temp * -1872
			+ sys_temp_dev->sensor_listener[8].temp * 446
			+ sys_temp_dev->sensor_listener[9].temp * -2737
			+ sys_temp_dev->sensor_listener[10].temp * 2230
			+ 3433539)
			/ 1000;
	user_backleft = (sys_temp_dev->sensor_listener[0].temp * 278
			+ sys_temp_dev->sensor_listener[1].temp * 288
			+ sys_temp_dev->sensor_listener[2].temp * 572
			+ sys_temp_dev->sensor_listener[4].temp * -791
			+ sys_temp_dev->sensor_listener[5].temp * 196
			+ sys_temp_dev->sensor_listener[6].temp * 127
			+ sys_temp_dev->sensor_listener[8].temp * 287
			+ sys_temp_dev->sensor_listener[9].temp * -248
			+ 8375643)
			/ 1000;
	user_backright = (sys_temp_dev->sensor_listener[0].temp * 403
			+ sys_temp_dev->sensor_listener[1].temp * 294
			+ sys_temp_dev->sensor_listener[2].temp * -116
			+ sys_temp_dev->sensor_listener[3].temp * 523
			+ sys_temp_dev->sensor_listener[4].temp * 129
			+ sys_temp_dev->sensor_listener[7].temp * -697
			+ sys_temp_dev->sensor_listener[8].temp * 406
			+ sys_temp_dev->sensor_listener[9].temp * -738
			+ sys_temp_dev->sensor_listener[10].temp * 493
			+ 8498575)
			/ 1000;
	edge_temp = (sys_temp_dev->sensor_listener[0].temp * 219
			+ sys_temp_dev->sensor_listener[1].temp * 90
			+ sys_temp_dev->sensor_listener[2].temp * 237
			+ sys_temp_dev->sensor_listener[4].temp * 147
			+ sys_temp_dev->sensor_listener[5].temp * -32
			+ sys_temp_dev->sensor_listener[6].temp * -61
			+ sys_temp_dev->sensor_listener[8].temp * -69
			+ sys_temp_dev->sensor_listener[9].temp * 204
			+ 6908150)
			/ 1000;
	edge_temp -= 1000;
	max_temp = max(front_temp, back_temp);
	max_temp = max(max_temp, edge_temp);
	user_max_temp = max(user_front, user_backleft);
	user_max_temp = max(user_max_temp, user_backright);
	user_max_temp -= 1550;
#endif
	dev_info(&sys_temp_dev->pdev->dev, "temperature model V=%d prod=%d front_temp=%d back_temp=%d user_temp=%d edge_temp=%d user_front=%d user_backleft=%d user_backright=%d\n",
		MODEL_VERSION, prod, front_temp, back_temp, user_max_temp, edge_temp, user_front, user_backleft, user_backright);

	for (i = 0; i < sys_temp_dev->num_sensors; i++) {
		if (!strncasecmp(sys_temp_dev->sensor[i].name,
				 "front_temp",
				 LENOVO_SYS_TEMP_NAME_LENGTH)) {
			sys_temp_dev->sensor[i].temp = front_temp;
		} else if (!strncasecmp(sys_temp_dev->sensor[i].name,
				 "back_temp",
				 LENOVO_SYS_TEMP_NAME_LENGTH)) {
			sys_temp_dev->sensor[i].temp = max_temp;
		} else if (!strncasecmp(sys_temp_dev->sensor[i].name,
				 "user_temp",
				 LENOVO_SYS_TEMP_NAME_LENGTH)) {
			sys_temp_dev->sensor[i].temp = user_max_temp;
		} else if (!strncasecmp(sys_temp_dev->sensor[i].name,
				 "user_front_temp",
				 LENOVO_SYS_TEMP_NAME_LENGTH)) {
			sys_temp_dev->sensor[i].temp = user_front;
		} else if (!strncasecmp(sys_temp_dev->sensor[i].name,
				 "user_back_temp",
				 LENOVO_SYS_TEMP_NAME_LENGTH)) {
			sys_temp_dev->sensor[i].temp = max(user_backright, user_backleft);
		} else if (!strncasecmp(sys_temp_dev->sensor[i].name,
				 "hbm_temp",
				 LENOVO_SYS_TEMP_NAME_LENGTH)) {
			sys_temp_dev->sensor[i].temp = max(user_backright, hbm_temp);
		}
	}

	for (i = 0; i < sys_temp_dev->num_sensors; i++) {
		desc +=
			sprintf(buf + desc, "[%d]%s=%d ",
			i,
			sys_temp_dev->sensor[i].name,
			sys_temp_dev->sensor[i].temp);
	}

	dev_info(&sys_temp_dev->pdev->dev, "V=%d prod=%d %s\n", MODEL_VERSION, prod, buf);
	delay_time = 5000;

work_exit:
	schedule_delayed_work(&sys_temp_dev->temp_changed_work, msecs_to_jiffies(delay_time));
	return;
}

static int lenovo_sys_temp_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i;
	struct device_node *node;
	int num_sensors;
	int num_registered = 0;
	int num_sensors_listener;

	dev_err(&pdev->dev, "Enter\n");

	if (!pdev)
		return -ENODEV;

	node = pdev->dev.of_node;
	if (!node) {
		dev_err(&pdev->dev, "bad of_node\n");
		return -ENODEV;
	}

	num_sensors = of_property_count_strings(node, TEMP_NODE_SENSOR_NAMES);
	if (num_sensors <= 0) {
		dev_err(&pdev->dev,
			"bad number of sensors: %d\n", num_sensors);
		return -EINVAL;
	}

	num_sensors_listener = of_property_count_strings(node, SENSOR_LISTENER_NAMES);
	if (num_sensors_listener <= 0) {
		dev_err(&pdev->dev,
			"bad number of sensors-listener: %d\n", num_sensors_listener);
	}
	sys_temp_dev = devm_kzalloc(&pdev->dev, sizeof(struct lenovo_sys_temp_dev),
				    GFP_KERNEL);
	if (!sys_temp_dev) {
		dev_err(&pdev->dev,
			"Unable to alloc memory for sys_temp_dev\n");
		return -ENOMEM;
	}

	sys_temp_dev->pdev = pdev;
	sys_temp_dev->num_sensors = num_sensors;

	sys_temp_dev->sensor =
				(struct lenovo_sys_temp_sensor *)devm_kzalloc(&pdev->dev,
				(num_sensors *
				       sizeof(struct lenovo_sys_temp_sensor)),
				       GFP_KERNEL);
	if (!sys_temp_dev->sensor) {
		dev_err(&pdev->dev,
			"Unable to alloc memory for sensor\n");
		return -ENOMEM;
	}

	for (i = 0; i < num_sensors; i++) {
		ret = of_property_read_string_index(node,
						TEMP_NODE_SENSOR_NAMES, i,
						&sys_temp_dev->sensor[i].name);
		if (ret) {
			dev_err(&pdev->dev, "Unable to read of_prop string\n");
			goto err_thermal_unreg;
		}

		sys_temp_dev->sensor[i].temp = DEFAULT_TEMPERATURE;
		sys_temp_dev->sensor[i].tz_dev =
		   thermal_zone_device_register(sys_temp_dev->sensor[i].name,
						0, 0,
						&sys_temp_dev->sensor[i],
						&lenovo_sys_temp_ops,
						NULL, 0, 0);
		if (IS_ERR(sys_temp_dev->sensor[i].tz_dev)) {
			dev_err(&pdev->dev,
				"thermal_zone_device_register() failed.\n");
			ret = -ENODEV;
			goto err_thermal_unreg;
		}
		num_registered = i + 1;
	}

	platform_set_drvdata(pdev, sys_temp_dev);

	if (num_sensors_listener <= 0) {
		dev_info(&sys_temp_dev->pdev->dev,
				"No configure sensors listener !\n");
		goto err_sensors_listener;
	}

	sys_temp_dev->num_sensors_listener = num_sensors_listener;
	sys_temp_dev->sensor_listener =
				(struct lenovo_sys_temp_sensor *)devm_kzalloc(&pdev->dev,
				(num_sensors_listener *
				       sizeof(struct lenovo_sys_temp_sensor)),
				       GFP_KERNEL);
	if (!sys_temp_dev->sensor_listener) {
		dev_err(&pdev->dev,
			"Unable to alloc memory for sensor_listener\n");
		goto err_sensors_listener;
	}

	INIT_DELAYED_WORK(&sys_temp_dev->temp_changed_work, temp_changed_work_func);
	schedule_delayed_work(&sys_temp_dev->temp_changed_work, msecs_to_jiffies(1000));

	dev_err(&pdev->dev, "Done\n");
	return 0;

err_sensors_listener:
	return ret;

err_thermal_unreg:
	for (i = 0; i < num_registered; i++)
		thermal_zone_device_unregister(sys_temp_dev->sensor[i].tz_dev);

	devm_kfree(&pdev->dev, sys_temp_dev);
	return ret;
}

static int lenovo_sys_temp_remove(struct platform_device *pdev)
{
	int i;
	struct lenovo_sys_temp_dev *dev =  platform_get_drvdata(pdev);

	for (i = 0; i < dev->num_sensors; i++)
		thermal_zone_device_unregister(dev->sensor[i].tz_dev);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, sys_temp_dev);
	return 0;
}

static const struct of_device_id lenovo_sys_temp_match_table[] = {
	{.compatible = "lenovo,sys-temp"},
	{},
};
MODULE_DEVICE_TABLE(of, lenovo_sys_temp_match_table);

static struct platform_driver lenovo_sys_temp_driver = {
	.probe = lenovo_sys_temp_probe,
	.remove = lenovo_sys_temp_remove,
	.driver = {
		.name = "lenovo_sys_temp",
		.owner = THIS_MODULE,
		.of_match_table = lenovo_sys_temp_match_table,
	},
};

static int __init lenovo_sys_temp_init(void)
{
	return platform_driver_register(&lenovo_sys_temp_driver);
}

static void __exit lenovo_sys_temp_exit(void)
{
	platform_driver_unregister(&lenovo_sys_temp_driver);
}

late_initcall(lenovo_sys_temp_init);
module_exit(lenovo_sys_temp_exit);

MODULE_ALIAS("platform:lenovo_sys_temp");
MODULE_AUTHOR("Lenovo");
MODULE_DESCRIPTION("Lenovo Thermal");
MODULE_LICENSE("GPL");
