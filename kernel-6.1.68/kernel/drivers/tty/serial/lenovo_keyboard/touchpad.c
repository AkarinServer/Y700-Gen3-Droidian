#include "lenovo_keyboard.h"
#include <linux/input/mt.h>

int lenovo_kb_touchpad_report(char *buf)
{
	struct input_dev *input_dev = lenovo_kb_core->touchpad;
	char *touch_data = lenovo_kb_core->touch_data;
	struct touch_event event;
	int i;
	int finger_num = 0;
	int len = 0;
	int offset = 0;
	int num = 0;

	if(!buf)
		return -EINVAL;

	if(!input_dev)
		return -EINVAL;

	finger_num = (buf[0] - 1)/5;
        len = finger_num * 5 + 2;

	if (!memcmp(touch_data, buf, len)) {
		kb_debug("touch data repeat\n");
		return 0;
	}
	memcpy(touch_data, buf, len);

	memset(&event, 0, sizeof(struct touch_event));
	event.is_left  = touch_data[1] & (1 << 0);
	event.is_right = touch_data[1] & (1 << 1);

	for (i = 0; i < finger_num; i++) {
		offset = 2 + 5 * i;
		event.id = touch_data[offset] >> 4 & 0xff;
		event.is_down = touch_data[offset] & 0x01;
		event.x = touch_data[offset+ 1] | touch_data[offset+ 2]<<8;
		event.y = touch_data[offset+ 3] | touch_data[offset+ 4]<<8;

		input_mt_slot(input_dev, event.id);

		if (event.is_down) {
			input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, true);
			input_report_abs(input_dev, ABS_MT_POSITION_X, event.x);
			input_report_abs(input_dev, ABS_MT_POSITION_Y, event.y);
			num++;
			kb_debug("id:%d x:%d y:%d\n", event.id, event.x, event.y);
		}else{
			input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
			kb_debug("id:%d release\n", event.id);
		}
	}

	input_report_key(input_dev, BTN_MOUSE, event.is_left);
	input_report_key(input_dev, BTN_RIGHT, event.is_right);
	input_report_key(input_dev, BTN_TOUCH, num > 0 ? 1 : 0);
	input_mt_report_finger_count(input_dev, num);
	input_sync(input_dev);

	return 0;
}

int lenovo_kb_touchpad_init(void)
{
	int ret = 0;
	struct input_dev *input_dev = input_allocate_device();
	if(!input_dev){
		kb_err("input_allocate_device err\n");
		return -ENOMEM;
	}

	input_dev->name = TOUCHPAD_NAME;
	input_dev->phys = "input/lenovo_touchpad";
	input_dev->id.bustype = BUS_HOST;
	input_dev->id.product = 0x61BA;
	input_dev->id.vendor = 0x17EF;
	input_dev->id.version = 0x0010;

	set_bit(EV_KEY, input_dev->evbit);
	set_bit(BTN_TOUCH, input_dev->keybit);
	set_bit(BTN_MOUSE, input_dev->keybit);
	set_bit(BTN_RIGHT, input_dev->keybit);

	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, TOUCH_X_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, TOUCH_Y_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_X, 0, TOUCH_X_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, TOUCH_Y_MAX, 0, 0);
	input_abs_set_res(input_dev, ABS_X, TOUCH_X_MAX);
	input_abs_set_res(input_dev, ABS_Y, TOUCH_Y_MAX);

	set_bit(INPUT_PROP_BUTTONPAD, input_dev->propbit);
	input_mt_init_slots(input_dev, 0x04, INPUT_MT_POINTER);

	ret = input_register_device(input_dev);
	if(ret){
		input_free_device(input_dev);
		kb_err("input_register_device err\n");
		return ret;
	}

	lenovo_kb_core->touchpad = input_dev;
	return 0;
}
