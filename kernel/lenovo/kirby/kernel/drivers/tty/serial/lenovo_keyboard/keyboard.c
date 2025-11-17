#include"lenovo_keyboard.h"
#define KEYBOARD_NUM_KEYS   256
#define KEYBOARD_CUSTOM_KEYS_NUM 16
#define unk	KEY_UNKNOWN
#define KEY_KB_DISABLE 0x028f
#define KEY_KB_ENABLE  0x028e

//hid usage id to linux scancode
static const unsigned char hid_to_sc[KEYBOARD_NUM_KEYS] = {
	0,  0,  0,  0, 30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38,
	50, 49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44,  2,  3,
	4,  5,  6,  7,  8,  9, 10, 11, 28,  1, 14, 15, 57, 12, 13, 26,
	27, 43, 43, 39, 40, 41, 51, 52, 53, 58, 59, 60, 61, 62, 63, 64,
	65, 66, 67, 68, 87, 88, 99, 70,119,110,102,104,111,107,109,106,
	105,108,103, 69, 98, 55, 74, 78, 96, 79, 80, 81, 75, 76, 77, 71,
	72, 73, 82, 83, 86,127,116,117,183,184,185,186,187,188,189,190,
	191,192,193,194,134,138,130,132,128,129,131,137,133,135,136,113,
	115,114,unk,unk,unk,121,unk, 89, 93,124, 92, 94, 95,unk,unk,unk,
	122,123, 90, 91, 85,unk,unk,unk,unk,unk,unk,unk,111,unk,unk,unk,
	unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,
	unk,unk,unk,unk,unk,unk,179,180,unk,unk,unk,unk,unk,unk,unk,unk,
	unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,
	unk,unk,unk,unk,unk,unk,unk,unk,111,unk,unk,unk,unk,unk,unk,unk,
	29, 42, 56,125, 97, 54,100,126,164,166,165,163,161,115,114,113,
	150,158,159,128,136,177,178,176,142,152,173,140,unk,unk,unk,unk
};

/*0x224,0xe2,0xea,0xe9,0x391,0x70,0x6f,0x392,0x394,0x395,0x397,0x38e,0x398,0x399,0x390*/
static const unsigned int custom_key_map[KEYBOARD_CUSTOM_KEYS_NUM][2] = {
        {0xe2,   KEY_MUTE},
        {0xea,   KEY_VOLUMEDOWN},
        {0xe9,   KEY_VOLUMEUP},
        {0x224,  KEY_BACK},
	{0x70,  0x0c0070/*KEY_BRIGHTNESSDOWN*/},
        {0x6f,  0x0c006F/*KEY_BRIGHTNESSUP*/},
        {0x38e,  0x0c038e/*KEY_LOCKSCREEN*/},
        {0x390,  0x0c0390/*KEY_SWITCHLANGUAGE*/}, 
        {0x391,  0x0c0391/*KEY_MICDISABLE*/},
        {0x392,  0x0c0392/*KEY_TOUCHPANELMUTE*/},
        {0x393,  0x0c0393/*KEY_GLOBALSEARCH*/},
        {0x394,  0x0c0394/*KEY_FULLSCREEN*/},
        {0x395,  0x0c0395/*KEY_SPLITSCREEN*/},
        {0x397,  0x0c0397/*KEY_SUPERINTCON*/},
        {0x398,  0x0c0398/*KEY_CUSTOMERAPP1*/},
        {0x399,  0x0c0399/*KEY_CUSTOMERAPP2*/},
};

static int lenovo_kb_event_hander(struct input_dev *dev,
		unsigned int type, unsigned int code, int value)
{
	int ret = 0;
	switch(type){
		case EV_LED:
			lenovo_kb_led_process(code, value);
			break;
		case EV_MSC:
			break;
		default:
			kb_err("no type  err \n");
			ret = -1;
	}
	return ret;
}

int lenovo_kb_keyboard_init(void)
{
	int i = 0, ret = 0;

	struct input_dev *keyboard  = input_allocate_device();
	if (!keyboard) {
		kb_err("input_allocate_device err \n");
		return -ENOMEM;
	}

	keyboard->name = KEYBOARD_NAME;
	keyboard->phys = "input/lenovo_kb";
	keyboard->id.bustype = BUS_HOST;
	keyboard->id.vendor = 0x17EF;;
	keyboard->id.product = 0x61BA;;
	keyboard->id.version = 0x0010;

	__set_bit(EV_KEY, keyboard->evbit);
	__set_bit(EV_LED, keyboard->evbit);
	__set_bit(EV_MSC, keyboard->evbit);
	__set_bit(LED_CAPSL, keyboard->ledbit);
	__set_bit(MSC_SCAN, keyboard->mscbit);
	__set_bit(KEY_MUTE, keyboard->keybit);
	__set_bit(KEY_VOLUMEDOWN, keyboard->keybit);
	__set_bit(KEY_VOLUMEUP, keyboard->keybit);
	__set_bit(KEY_BACK, keyboard->keybit);
	__set_bit(KEY_WAKEUP, keyboard->keybit);
	__set_bit(KEY_UNKNOWN, keyboard->keybit);
	__set_bit(KEY_KB_ENABLE, keyboard->keybit);
	__set_bit(KEY_KB_DISABLE, keyboard->keybit);

	keyboard->event = lenovo_kb_event_hander;

	for (i = 0; i < KEYBOARD_NUM_KEYS; i++) {
		if(hid_to_sc[i] != 0 && hid_to_sc[i] != unk){
			__set_bit(hid_to_sc[i], keyboard->keybit);
		}
	}

	ret = input_register_device(keyboard);
	if(ret){
		input_free_device(keyboard);
		kb_err("input_register_device err \n");
		return ret;
	}

	lenovo_kb_core->keyboard = keyboard;
	kb_info("ok \n");
	return 0;
}

int lenovo_kb_keyboard_report(char *buf){
	int i=  0;
	struct lenovo_keyboard_data *kbd = lenovo_kb_core;
	struct input_dev  *input_dev = kbd->keyboard;
	if(!buf)
		return -EINVAL;
	if(!input_dev)
		return -EINVAL;

	memcpy(lenovo_kb_core->new, buf, 8);

//	kb_debug("current data: %*ph", 8, lenovo_kb_core->new);
//	kb_debug("last data: %*ph", 8, lenovo_kb_core->old);
	for (i = 0; i < 8; i++)
		input_report_key(input_dev, hid_to_sc[i + 224],
				(kbd->new[1] >> i) & 1);

	for (i = 2; i < 8; i++) {
		if (kbd->old[i] > 3 && memscan(kbd->new + 2, kbd->old[i], 6) ==
				kbd->new + 8) {
			if (hid_to_sc[kbd->old[i]]){
				input_report_key(input_dev,
						hid_to_sc[kbd->old[i]], 0);
				kb_debug("key:%d up\n", hid_to_sc[kbd->old[i]]);
			} else
				kb_debug("Unknown key (scancode %#x) released.\n",
						kbd->old[i]);
		}

		if (kbd->new[i] > 3 && memscan(kbd->old + 2, kbd->new[i], 6) ==
				kbd->old + 8) {
			if (hid_to_sc[kbd->new[i]]){
				input_report_key(input_dev,
						hid_to_sc[kbd->new[i]], 1);
				kb_debug("key:%d down\n",
						hid_to_sc[kbd->new[i]]);
			}else
				kb_debug("Unknown key (scancode %#x) pressed.\n",
						kbd->new[i]);
		}
	}

	input_sync(input_dev);
	memcpy(kbd->old, kbd->new, 8);
	return 0;
}

void *memscan_ex(void *addr, int c, size_t size)
{
	unsigned char *p = addr;
	unsigned short key = 0;
	while (size) {
		key = *p | ((*(p + 1)) << 8);
		if (key == c)
			return (void *)p;
		p = p + 2;
		size = size - 2;
	}
	return (void *)p;
}

int lenovo_kb_cuskey_report(char *buf){
	int i=  0, j = 0;
	unsigned short cuskey = 0;
	struct lenovo_keyboard_data *kbd = lenovo_kb_core;
	struct input_dev *input_dev = kbd->keyboard;
	if(!buf)
		return -EINVAL;
	if(!input_dev)
		return -EINVAL;

	memcpy(lenovo_kb_core->mm_new, &buf[1], 4);

	for(i = 0; i < 2; i++){
		cuskey = kbd->mm_old[2*i] | kbd->mm_old[2*i+1] << 8;
		if ( memscan_ex(kbd->mm_new, cuskey, 4) == kbd->mm_new + 4) {
			if (cuskey){
				for(j = 0; j < KEYBOARD_CUSTOM_KEYS_NUM; j++){
					if(cuskey == custom_key_map[j][0])
						break;
				}
				if(j < KEYBOARD_CUSTOM_KEYS_NUM && j > 3){
					input_event(input_dev, EV_MSC, MSC_SCAN, custom_key_map[j][1]);
					input_event(input_dev, EV_KEY, unk, 0);
					kb_debug("key:%x up\n", cuskey);
				} else if (j < 4) {
					input_report_key(input_dev, custom_key_map[j][1], 0);
					kb_debug("key:%x up\n", cuskey);
				} else
					kb_debug("Unknown key %d \n", cuskey);
			}else
				kb_debug("Unknown key \n");
		}

		cuskey = kbd->mm_new[2*i] | kbd->mm_new[2*i+1] << 8;
		if (memscan_ex(kbd->mm_old, cuskey, 4) == kbd->mm_old + 4) {
			if (cuskey){
				for(j = 0; j < KEYBOARD_CUSTOM_KEYS_NUM; j++){
					if(cuskey == custom_key_map[j][0])
						break;
				}
				if(j < KEYBOARD_CUSTOM_KEYS_NUM && j > 3){
					input_event(input_dev, EV_MSC, MSC_SCAN, custom_key_map[j][1]);
                                        input_event(input_dev, EV_KEY, unk, 1);
					kb_debug("key:%x down\n", cuskey);
				}else if (j < 4) {
					input_report_key(input_dev, custom_key_map[j][1], 1);
					kb_debug("key:%x down\n", cuskey);
				}else
					kb_debug("Unknown key %#x  pressed.\n", cuskey);

			}
			else
				kb_debug("Unknown key pressed.\n");
		}
	}

	input_sync(input_dev);
	memcpy(kbd->mm_old, kbd->mm_new, 4);
	return 0;
}

int lenovo_kb_enkey_report(bool enable)
{
	struct lenovo_keyboard_data *kbd = lenovo_kb_core;
        struct input_dev *input_dev = kbd->keyboard;
	bool en;

        if(!input_dev)
                return -EINVAL;

	en = enable;

	input_report_key(input_dev, en ? KEY_KB_ENABLE : KEY_KB_DISABLE, 1);
	input_sync(input_dev);
	mdelay(15);
	input_report_key(input_dev, en ? KEY_KB_ENABLE : KEY_KB_DISABLE, 0);
	input_sync(input_dev);
	return 0;
}
