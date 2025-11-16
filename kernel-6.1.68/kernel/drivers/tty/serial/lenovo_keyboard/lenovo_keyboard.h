#ifndef __KEYBOARD_CORE__H__
#define __KEYBOARD_CORE__H__

#include <linux/ioctl.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#include <linux/fcntl.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#include <linux/tty.h>
#include <linux/uio.h>

#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#include "lenovo_common.h"
#define KEYBOARD_NAME "Lenovo Keyboard Pack For Yoga Tab Keyboard"
#define TOUCHPAD_NAME "Lenovo Keyboard Pack For Yoga Tab Touchpad"
#define KEYBOARD_CORE_NAME "lenovo,lenovo_keyboard"

#define TOUCH_X_MAX    3211
#define TOUCH_Y_MAX    1879
#define TOUCH_FINGER_MAX 4

#define UART_BUFFER_SIZE 256

extern bool debug_log_flag;
#define KB_TAG   "lenovo_KB"
#define kb_info(fmt, args...)   pr_info("[%s-info][%s:%d] " fmt, KB_TAG, __func__, __LINE__, ##args)

#define kb_err(fmt, args...)   pr_err("[%s-err][%s:%d] " fmt, KB_TAG, __func__, __LINE__, ##args)

#define kb_debug(fmt, args...)   \
	{ if (debug_log_flag) \
		pr_info("[%s-debug][%s:%d] " fmt, KB_TAG, __func__, __LINE__, ##args);}

enum {
	KB_CONNECT_STATUS,
	KB_LCD_STATUS,
	KB_CAPSLOCK_STATUS,
	KB_MUTE_STATUS,
	KB_MICMUTE_STATUS,
	KB_TOUCH_EN_STATUS,
	KB_TP_ASLEEP_EN_STATUS,
};

enum {
	UNCONNECTED,
	CONNECTED,
};

enum {
	OFF,
	ON,
};

enum {
	FWK_MUTE_ON = 1,
	FWK_MUTE_OFF,
	FWK_MICMUTE_ON,
	FWK_MICMUTE_OFF,
	FWK_UART_INIT_DONE,
	FWK_TOUCH_ENABLE,
	FWK_TOUCH_DISABLE,
	FWK_TP_AUTOSLEEP_ENABLE,
	FWK_TP_AUTOSLEEP_DISABLE,
};

struct touch_event{
	unsigned int x;
	unsigned int y;
	unsigned char id;
	unsigned char area;
	unsigned char is_down;
	unsigned char is_left;
	unsigned char is_right;
};

struct lenovo_keyboard_data{
	struct input_dev *keyboard;
	unsigned char old[8];
	unsigned char new[8];
	unsigned char mm_old[4];
	unsigned char mm_new[4];

	struct input_dev *touchpad;
	unsigned char  touch_data[22];   //len(1Byte)+key(1Byte)+fingers(20Byte)

	void *cookie;

	unsigned char send_data_buf[UART_BUFFER_SIZE];
	unsigned char reply_data_buf[UART_BUFFER_SIZE];
	unsigned char lb_data_buf[UART_BUFFER_SIZE];
	unsigned char recv_dma_buf[UART_BUFFER_SIZE];
	int recv_len;
	int recv_status;
	int lb_data_len;
	int reply_data_len;

	struct workqueue_struct *lenovo_kb_workqueue;
	struct delayed_work hall_work;
	struct work_struct connect_work;
	struct work_struct lcd_work;
	struct work_struct capsled_work;
	struct work_struct restore_work;

	struct platform_device *plat_dev;

	int power_en_gpio;
	int tx_en_gpio;

	int plug_gpio;
	int plug_irq;

	int uart_wake_gpio;
	int uart_wake_irq;

	int lid_gpio;
	bool lid_change_sleep;

	struct mutex mutex;

	unsigned long kb_status;
	bool fw_update_reset;

	struct file *file_client;
	int read_flag;
};

extern struct lenovo_keyboard_data *lenovo_kb_core;
extern char TAG[60];

int lenovo_kb_keyboard_init(void);
int lenovo_kb_keyboard_report(char *buf);
int lenovo_kb_cuskey_report(char *buf);
int lenovo_kb_enkey_report(bool enable);

void lenovo_kb_led_process(int code, int value);

int lenovo_kb_touchpad_init(void);
int lenovo_kb_touchpad_report(char *buf);

#endif
