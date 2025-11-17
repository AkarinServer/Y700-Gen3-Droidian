#include <linux/platform_device.h>

#define MAX_UEVENT_PROP_NUM	20

#define SYSFS_FIELD_RW(_name, _prop, _report)							\
{												\
	.attr = __ATTR(_name, 0664, lenovo_qi_sysfs_show, lenovo_qi_sysfs_store),	\
	.prop = _prop,										\
	.report_uevent = _report,								\
}

#define SYSFS_FIELD_RO(_name, _prop, _report)				\
{									\
	.attr = __ATTR(_name, 0664, lenovo_qi_sysfs_show, NULL),	\
	.prop = _prop,							\
	.report_uevent = _report,					\
}

#define BATT_SYSFS_FIELD_RW(_name, _prop, _report)							\
{												\
	.attr = __ATTR(_name, 0664, lenovo_battery_sysfs_show, lenovo_battery_sysfs_store),	\
	.prop = _prop,										\
	.report_uevent = _report,								\
}

#define BATT_SYSFS_FIELD_RO(_name, _prop, _report)				\
{									\
	.attr = __ATTR(_name, 0664, lenovo_battery_sysfs_show, NULL),	\
	.prop = _prop,							\
	.report_uevent = _report,					\
}

enum sysfs_property {
	/* Nodes under power_supply/battery/ */
	POWER_SUPPLY_PROP_WLS_TX_OPEN = 0,
	POWER_SUPPLY_PROP_WLS_TX_CHARGE_STATE,
	POWER_SUPPLY_PROP_WLS_TX_LEVEL,
	POWER_SUPPLY_PROP_WLS_TX_ATTACHED,
	POWER_SUPPLY_PROP_WLS_TX_MAC,
	POWER_SUPPLY_PROP_WLS_TX_TX_IIN,
	POWER_SUPPLY_PROP_WLS_TX_MAC_H,
	POWER_SUPPLY_PROP_WLS_TX_MAC_L,
	POWER_SUPPLY_PROP_WLS_TX_CMD,
	POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLE,
	POWER_SUPPLY_PROP_BATTERY_PROTECTION_SETTING,
	POWER_SUPPLY_PROP_BATTERY_PROTECTION_SETTING_EU,
	POWER_SUPPLY_PROP_BATTERY_RECHARGE_SETTING,
	POWER_SUPPLY_PROP_BATTERY_MAINTENANCE,
	POWER_SUPPLY_PROP_BATTERY_MAINTENANCE20,
	POWER_SUPPLY_PROP_BATTERY_SHIPPING_MODE,
	POWER_SUPPLY_PROP_BATTERY_PRODUCE_DATE,
	POWER_SUPPLY_PROP_BATTERY_ACTIVATE_DATE,
	POWER_SUPPLY_PROP_BATTERY_SOH,
	POWER_SUPPLY_PROP_BATTERY_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_BATTERY_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_BATTERY_CV,
	POWER_SUPPLY_PROP_BATTERY_CHG_FV,
	POWER_SUPPLY_PROP_BATTERY_FAKE_CYCLE_COUNT,
	POWER_SUPPLY_PROP_BATTERY_FAKE_SOC,
	POWER_SUPPLY_PROP_BATTERY_ABNORMAL_STATE,
};

struct sysfs_info {
	struct device_attribute attr;
	enum sysfs_property prop;
	bool report_uevent;
};
