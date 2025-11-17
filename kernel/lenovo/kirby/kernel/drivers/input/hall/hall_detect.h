#ifndef __HALL_DETECT_H__
#define __HALL_DETECT_H__

#define HALL_NEAR  (1)
#define HALL_FAR   (0)

#define KEY_HALL_SLOW_IN 0x2ee
#define KEY_HALL_SLOW_OUT 0x2ef
#define POGO_HALL_SLOW_IN 0x2f0
#define POGO_HALL_SLOW_OUT 0x2f1

struct hall_t {
	unsigned int irq1;
	unsigned int irq2;
	unsigned int gpiopin1;
	unsigned int gpiopin2;
	unsigned int curr_mode;
	unsigned int retry_cnt;
	struct input_dev * hall_dev;
	spinlock_t   spinlock1;
	spinlock_t   spinlock2;
};

#endif /* __HALL_DETECT_H__*/

