#ifndef __LENOVO_COMMON__H__
#define __LENOVO_COMMON__H__

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/serial_core.h>
extern int lenovo_kb_analyze(char *buf, int len);
extern int lenovo_kb_set_uport_filp(struct uart_port*, int type);
extern int lenovo_kb_enable_uart_tx(int value);
#endif
