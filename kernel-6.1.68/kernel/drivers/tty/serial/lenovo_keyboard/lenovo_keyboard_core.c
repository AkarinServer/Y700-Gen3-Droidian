#include "lenovo_keyboard.h"
#include <linux/of.h>
#include <uapi/linux/sched/types.h> 
#include <linux/pm.h>
#include <linux/pm_wakeirq.h>
#include "owb_ppp.h"
#include <linux/soc/qcom/panel_event_notifier.h>

#ifdef CONFIG_DRM
#include <drm/drm_panel.h>
struct drm_panel *active_panel;
#endif

struct lenovo_keyboard_data *lenovo_kb_core = NULL;
char TAG[60] = {0};
bool debug_log_flag = false;

static DECLARE_WAIT_QUEUE_HEAD(waiter);
static DECLARE_WAIT_QUEUE_HEAD(read_waiter);

static const char* fwk_cmd[10] = {"", "FWK_MUTE_ON", "FWK_MUTE_OFF",
				"FWK_MICMUTE_ON", "FWK_MICMUTE_OFF",
				"FWK_UART_INIT_DONE", "FWK_TOUCH_ENABLE",
				"FWK_TOUCH_DISABLE", "FWK_TP_AUTOSLEEP_ENABLE",
				"FWK_TP_AUTOSLEEP_DISABLE"};

static inline int get_kb_status(int status)
{
	return test_bit(status, &lenovo_kb_core->kb_status);
}

static inline void set_kb_status(int status)
{
	set_bit(status, &lenovo_kb_core->kb_status);
}

static inline void clear_kb_status(int status)
{
	clear_bit(status, &lenovo_kb_core->kb_status);
}

#if defined(CONFIG_DRM_PANEL)
static int panel_check_dt(struct device_node *np)
{
	int i;
	int count;
	struct device_node *node;
	struct drm_panel *panel;

	count = of_count_phandle_with_args(np, "panel", NULL);
	if (count <= 0)
		return 0;

	for (i = 0; i < count; i++) {
		node = of_parse_phandle(np, "panel", i);
		panel = of_drm_find_panel(node);
		of_node_put(node);
		if (!IS_ERR(panel)) {
			active_panel = panel;
			return 0;
		}
	}

	return PTR_ERR(panel);
}
#endif

static void lenovo_kb_show_buf(void *buf, int count)
{
	if (debug_log_flag) {
		int i = 0;
		char temp[512] = {0};
		int len = 0;
		char *pbuf = (char *) buf;
		if (count > UART_BUFFER_SIZE)
			count = UART_BUFFER_SIZE;
		for (i = 0; i < count; i++) {
			len += sprintf(temp + len, "%02x", pbuf[i]);
		}
		kb_debug("%s len:[%d]  %s\n", TAG, count, temp);
	}
}

static int lenovo_kb_input_dev_create(void)
{
        int ret = 0;

        if(!lenovo_kb_core->keyboard){
                ret = lenovo_kb_keyboard_init();
                if(ret){
                        kb_err("lenovo_kb keyboard init err:%d\n", ret);
                        return ret;
                }
        }

        if(!lenovo_kb_core->touchpad){
                ret = lenovo_kb_touchpad_init();
                if(ret){
                        kb_err("touchpad_input_dev_init err:%d\n", ret);
                        return ret;
                }
        }

        return 0;
}

static void lenovo_kb_input_dev_destory(void)
{
        if (lenovo_kb_core->touchpad) {
                input_unregister_device(lenovo_kb_core->touchpad);
                kb_info("touchpad input_unregister_device\n");
                lenovo_kb_core->touchpad = NULL;
        }

        if(lenovo_kb_core->keyboard){
                input_unregister_device(lenovo_kb_core->keyboard);
                kb_info("keyboard input_unregister_device\n");
                lenovo_kb_core->keyboard = NULL;
        }

        return;
}

static int lenovo_kb_uart_data_process(char *buf, int len)
{
	int value = buf[0];  

	switch(value){
	case OWB_EVT_KEYPRESS_DATA:
		lenovo_kb_keyboard_report(&buf[1]);
		break;

	case OWB_EVT_MMKEY_DATA:
		lenovo_kb_cuskey_report(&buf[1]);
		break;

	case OWB_EVT_TOUCH_DATA:
		lenovo_kb_touchpad_report(&buf[1]);
		break;	

	case OWB_EVT_KB_DISABLE_STS:
		if (len >= 5) {
			kb_info("kb forbidden:%d", buf[4]);
			lenovo_kb_enkey_report(!!buf[4]);
		}
		break;

	case OWB_EVT_SYNC_UPLINK:
		if((get_kb_status(KB_CONNECT_STATUS) == UNCONNECTED)){
			kb_info("plug in\n");
			/*thread can not sleep, so async*/
			queue_work(lenovo_kb_core->lenovo_kb_workqueue,
					&lenovo_kb_core->connect_work);
		}

		if (lenovo_kb_core->fw_update_reset && buf[2] == 0x03) { //new firmware init done
			kb_info("new firmware init done, to restore led\n");
			/*thread can not sleep, so async*/
			queue_work(lenovo_kb_core->lenovo_kb_workqueue,
					&lenovo_kb_core->restore_work);
			lenovo_kb_core->fw_update_reset = false;
		}
		break;

	case OWB_CMD_SET_PARAM:
	case OWB_RESP_SET_PARAM:
	case OWB_RESP_START_FWUP:
	case OWB_CMD_START_FWUP:
	case OWB_RESP_I2C_RD:
		if(lenovo_kb_core->read_flag == 1){
			if(value == (int)lenovo_kb_core->send_data_buf[0] ||
					value == (int)lenovo_kb_core->send_data_buf[0]+1){
				lenovo_kb_core->read_flag = 0;
				wake_up_interruptible(&read_waiter);
			}
		}
		break;
	case OWB_CMD_TXDATA_FWUP:
	case OWB_RESP_TXDATA_FWUP:
	case OWB_CMD_END_FWUP:
	case OWB_CMD_SOFT_RESET:
	case OWB_RESP_SOFT_RESET:
	case OWB_CMD_I2C_WR:
	case OWB_RESP_I2C_WR:
	case OWB_CMD_I2C_RD:
	case OWB_CMD_PRODUCTION:
	case OWB_RESP_PRODUCTION:
	case OWB_CMD_TP_VERSION:
	case OWB_RESP_TP_VERSION:
		// These are known cmd that would be process in up-layer
		break;
	case OWB_RESP_END_FWUP:
		if (buf[2] == 0x03)// after firmware update, keyboard will reset
			lenovo_kb_core->fw_update_reset = true;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int uart_pack_data(char *buf, int len, unsigned char *p_out, int *p_len)
{
	unsigned char rand;
	unsigned char *p_buf = p_out;
	unsigned char buf_scramed[256] = {0};

	rand = lenovo_scram(buf_scramed, buf, len);

	PPPFrameEncode(buf_scramed, len, &rand, 1, p_buf, (unsigned short*)p_len);

	sprintf(TAG,"%s %d PackedData ", __func__, __LINE__);
	lenovo_kb_show_buf(p_buf, *p_len);
	return 0;
}

static int uart_unpack_data(char *buf, int len, unsigned char *out_buf, int *out_len)
{
	unsigned char buf_decoded[256] = {0};
	unsigned char len_decoded, rand, len_pkt;
	int f_dec;

	sprintf(TAG,"%s %d rawdata", __func__, __LINE__);
	lenovo_kb_show_buf(buf, len);

	f_dec = PPPFrameDecode(buf, len, buf_decoded, &len_decoded);

	if(f_dec < 0){
		kb_err("Decode Error, Return %d \r\n",f_dec);
		return  -EINVAL;
	}

	rand = buf_decoded[0];
	len_pkt = len_decoded - 1;
	lenovo_disscram(out_buf, buf_decoded + 1, len_pkt, rand);
	*out_len = len_pkt;

	sprintf(TAG,"%s %d unpack", __func__, __LINE__);
	lenovo_kb_show_buf(out_buf, len_pkt);
	return 0;
}

enum {
	OWB_STATE_WAIT_START_FLAG1,
	OWB_STATE_WAIT_START_FLAG2,
	OWB_STATE_WAIT_END_FLAG,
};

int lenovo_kb_analyze(char * buf, int len)
{
	char *pbuf = buf;
	int i = 0;
	int recv_count = 0;

	char *temp;
	char recv_buf[128] = {0};
	int out_len = 0;
	int ret = 0;

	if (lenovo_kb_core == NULL ||
			lenovo_kb_core->file_client == NULL || len <= 0) {
		kb_err("len:%d fail:\r\n", len);
		return -1;
	}

	temp = lenovo_kb_core->recv_dma_buf;
	for(i = 0; i < len; i++){
		char data = pbuf[i];
		switch(lenovo_kb_core->recv_status){
		case OWB_STATE_WAIT_START_FLAG1:
			if(data == PPP_FRAME_FLAG1){     //0x7E, MagicNumber1
				lenovo_kb_core->recv_status = OWB_STATE_WAIT_START_FLAG2;
				lenovo_kb_core->recv_len = 0;
				temp[lenovo_kb_core->recv_len++] = data;
			}
			break;

		case OWB_STATE_WAIT_START_FLAG2:
			if(data == PPP_FRAME_FLAG2){     //0xFF, MagicNumber2
				lenovo_kb_core->recv_status = OWB_STATE_WAIT_END_FLAG;
				temp[lenovo_kb_core->recv_len++] = data;
			}else if(data== PPP_FRAME_FLAG1){
				break;
			}else{
				lenovo_kb_core->recv_status = OWB_STATE_WAIT_START_FLAG1;
			}

			break;

		case OWB_STATE_WAIT_END_FLAG:
			if(data == PPP_FRAME_FLAG1){        //0x7E
				if(lenovo_kb_core->recv_len < 4){
					lenovo_kb_core->recv_status = OWB_STATE_WAIT_START_FLAG2;
					lenovo_kb_core->recv_len = 1;
				} else{
					temp[lenovo_kb_core->recv_len++] = data;
					recv_count = lenovo_kb_core->recv_len;

					if(uart_unpack_data(temp, recv_count, recv_buf, &out_len) == 0){ // Unpack Success
						lenovo_kb_core->recv_status = OWB_STATE_WAIT_START_FLAG1;
						lenovo_kb_core->recv_len = 0;

						if(recv_buf[0] == lenovo_kb_core->send_data_buf[0]){
							// This is the loop-back data from driver itself
							memcpy(lenovo_kb_core->lb_data_buf, recv_buf, out_len);
							lenovo_kb_core->lb_data_len = out_len;
						}else if(recv_buf[0] == (int)lenovo_kb_core->send_data_buf[0]+1){
							// This is reply data from Keyboard
							memcpy(lenovo_kb_core->reply_data_buf, recv_buf, out_len);
							lenovo_kb_core->reply_data_len = out_len;
						}

						ret = lenovo_kb_uart_data_process(recv_buf, out_len);
						if(ret)
							kb_err("uart data process fail:%d\r\n", ret);

					}else{
						kb_err("uart_unpack_data fail\n");
						lenovo_kb_core->recv_status = OWB_STATE_WAIT_START_FLAG2;
						lenovo_kb_core->recv_len = 1;
					}

				}
			}else{
				temp[lenovo_kb_core->recv_len++] = data;
				if(lenovo_kb_core->recv_len == UART_BUFFER_SIZE){
					lenovo_kb_core->recv_status = OWB_STATE_WAIT_START_FLAG1;
				}
			}
			break;
		default:
			break;
		}
	}

	return ret;
}
EXPORT_SYMBOL(lenovo_kb_analyze);

int lenovo_kb_enable_uart_tx(int value)
{
        if(lenovo_kb_core == NULL || lenovo_kb_core->file_client == NULL) {
                kb_err("lenovo_kb_core or filp is null");
                return -1;
	}

        if(value == 1){
                gpio_direction_output(lenovo_kb_core->tx_en_gpio,1);
                udelay(450);//for set mode valid
		kb_info("enable\n");
        }else if(value == 0){
                //udelay(300); //for set mode valid
                gpio_direction_output(lenovo_kb_core->tx_en_gpio,0);
		kb_info("disable\n");
        }

        return 0;
}
EXPORT_SYMBOL(lenovo_kb_enable_uart_tx);

struct file* lenovo_kb_port_to_file(struct uart_port *uart_port)
{
        struct uart_state *state = uart_port->state;
        struct tty_struct *tty = state->port.itty;
        struct tty_file_private *priv;
        struct file *filp = NULL;
        spin_lock(&tty->files_lock);
        list_for_each_entry(priv, &tty->tty_files, list){
//              if(imajor(priv->file->f_inode) == 236 &&
//                              iminor(priv->file->f_inode) == uart_port->line){
                        filp = priv->file;
//                      break;
//              }
        }
        spin_unlock(&tty->files_lock);

        if (filp == NULL)
                kb_err("fail\n");

        return filp;
}

int lenovo_kb_set_uport_filp(struct uart_port *uart_port, int type)
{
        struct file *filp;

        if (lenovo_kb_core == NULL) {
                kb_err("lenovo_kb_core is NULL\n");
                return -1;
        }

        switch (type) {
        case 1:
                filp = lenovo_kb_port_to_file(uart_port);
                if(lenovo_kb_core->file_client == NULL && filp != NULL){
                        kb_info("set uport filp ok");
                        lenovo_kb_core->file_client = filp;
                }
                break;
        case 0:
                filp = lenovo_kb_port_to_file(uart_port);
                if (lenovo_kb_core->file_client != NULL &&
                                lenovo_kb_core->file_client == filp) {
                        kb_info("set uport filp null");
                        lenovo_kb_core->file_client = NULL;
                }
                break;
        default:
                kb_err("param err!\n");
                return -1;

        }

        return 0;
}
EXPORT_SYMBOL(lenovo_kb_set_uport_filp);

static int lenovo_kb_tty_write(const char *buf, size_t count)
{
	int ret = 0;
	struct tty_struct *tty_str = NULL;
	struct tty_driver *tty_dr = NULL;
	struct cdev *cdev = NULL;
	struct kiocb kib;
	struct kvec iov = {
		.iov_base = (void *)buf,
		.iov_len = min_t(size_t, count, MAX_RW_COUNT),
	};
	struct iov_iter iter;
	iov_iter_kvec(&iter, ITER_SOURCE, &iov, 1, iov.iov_len);

	if(lenovo_kb_core->file_client == NULL){
                kb_err("file_client is null!\n");
                return -1;
        }

	kib.ki_filp = lenovo_kb_core->file_client;
	kib.ki_flags = lenovo_kb_core->file_client->f_iocb_flags;
	kib.ki_ioprio = 0;
	kib.ki_pos = 0;

	tty_str = ((struct tty_file_private *)lenovo_kb_core->file_client->private_data)->tty;
	tty_dr = tty_str->driver;
	cdev = tty_dr->cdevs[tty_str->index];

	ret = cdev->ops->write_iter(&kib, &iter);
	if(ret < 0){
		kb_err("err:%d!\n", ret);
		return -1;
	}

	return ret;
}

static int lenovo_kb_write(void *buf, int len)
{
	int ret = 0;
	const int retry_count = 3;
	void *pbuf = buf;
	char write_buf[255] = {0};
	int write_len = 0;
	int i = 0;

	memset(lenovo_kb_core->send_data_buf, 0,
			sizeof(lenovo_kb_core->send_data_buf));
	ret = uart_pack_data(pbuf, len, write_buf, &write_len);
	if(ret){
		kb_err("pack data err\n");
		return ret;
	}

	memcpy(lenovo_kb_core->send_data_buf, buf, len);
	lenovo_kb_core->read_flag = 1;
	for(i = 0; i < retry_count; i++){
		ret  = lenovo_kb_tty_write(write_buf, write_len);
		if(ret > 0)
			break;
	}
	if(i >= retry_count){
		kb_err("fail ret:%d retry_count:%d \n", ret, i);
		return ret;
	}

	wait_event_interruptible_timeout(read_waiter,
			lenovo_kb_core->read_flag != 1, HZ/10);//timeout 100ms

	if (lenovo_kb_core->lb_data_len <= 0 ||
			lenovo_kb_core->lb_data_len != len) {
		kb_err("read_flag:%d send len:%d lb len:%d self check fail\n",
				lenovo_kb_core->read_flag, len,
				lenovo_kb_core->lb_data_len);
		lenovo_kb_core->read_flag = 0;
		return -1;
	}

	if (memcmp(lenovo_kb_core->send_data_buf, lenovo_kb_core->lb_data_buf,
				len) == 0) {
		kb_debug("self check ok");
		ret = 0;
	}else{
		kb_info("%*ph", len, lenovo_kb_core->send_data_buf);
                kb_info("%*ph", lenovo_kb_core->lb_data_len,
				lenovo_kb_core->lb_data_buf);

		kb_err("self check fail\n");
		ret = -1;
	}

	lenovo_kb_core->lb_data_len = 0;
	memset(lenovo_kb_core->lb_data_buf, 0, UART_BUFFER_SIZE);
	return ret;
}

static int lenovo_kb_read(void *buf, int *r_len)
{
	lenovo_kb_core->read_flag = 1;
	wait_event_interruptible_timeout(read_waiter,
			lenovo_kb_core->read_flag != 1, HZ/10);//timeout 100ms

	if(lenovo_kb_core->reply_data_len <= 0){
		kb_err("get no reply data \n");
		lenovo_kb_core->read_flag = 0;
		return -1;
	}

	kb_debug("read_flag:%d\n",
			lenovo_kb_core->read_flag);

	*r_len = lenovo_kb_core->reply_data_len;
	memcpy(buf, lenovo_kb_core->reply_data_buf,
			lenovo_kb_core->reply_data_len);

	memset(lenovo_kb_core->reply_data_buf, 0,
			sizeof(lenovo_kb_core->reply_data_buf));
	lenovo_kb_core->reply_data_len = 0;
	return 0;
}

static int lenovo_kb_transfer_data(void *w_buf, int w_len, void *r_buf, int *r_len)
{
	int ret = 0;
	int i = 0;
	int count = 5;

	mutex_lock(&lenovo_kb_core->mutex);
	for(i = 0; i < count; i++){
		ret = lenovo_kb_write(w_buf, w_len);
		if(ret){
			msleep(50);
			continue;
		}

		if (r_buf == NULL) {
			usleep_range(10000, 11000);
			break;
		}

		ret = lenovo_kb_read(r_buf, r_len);
		if(ret == 0)
			break;
	}
	mutex_unlock(&lenovo_kb_core->mutex);

	if(i >= count){
		kb_err("fail retry:%d \n", i);
		return ret;
	}
	return 0;
}

static void lenovo_kb_restore_leds(void)
{
	char led_buf[] = {0x6A, 0x06, 0x2B, 0x04, 0x00, 0x00, 0x00, 0x00};
	int ret;

	if(get_kb_status(KB_CAPSLOCK_STATUS))
		led_buf[4] = 0x01;
	else
		led_buf[4] = 0x00;

	if(get_kb_status(KB_MUTE_STATUS))
		led_buf[6] = 0x01;
	else
		led_buf[6] = 0x00;

	if(get_kb_status(KB_MICMUTE_STATUS))
		led_buf[7] = 0x01;
	else
		led_buf[7] = 0x00;

	ret = lenovo_kb_transfer_data(led_buf, sizeof(led_buf), NULL, NULL);
	if(ret)
		kb_err("set all led err\n");
	else
		kb_info("set capslock:%s mute:%s micmute:%s ok\n",
				led_buf[4] ? "ON" : "OFF",
				led_buf[6] ? "ON" : "OFF",
				led_buf[7] ? "ON" : "OFF");
}

static void lenovo_kb_set_capsled(bool enable)
{
	char led_buf[] = {0x6A, 0x03, 0x27, 0x01, 0x00};
	int ret;

	if (enable)
		led_buf[4] = 0x01;

	ret = lenovo_kb_transfer_data(led_buf, sizeof(led_buf), NULL, NULL);
	if (ret < 0)
		kb_err("set %d fail\n", enable);
	else
		kb_info("set %d ok\n", enable);
}

static void lenovo_kb_capsled_work(struct work_struct *work)
{
	lenovo_kb_set_capsled(!!get_kb_status(KB_CAPSLOCK_STATUS));
}

void lenovo_kb_led_process(int code, int value)
{
	kb_info("type:led code:0x%2x value:0x%2x \n",
		code, value);

	if (code == LED_CAPSL) {
		if(value == 1)
			set_kb_status(KB_CAPSLOCK_STATUS);
                else
			clear_kb_status(KB_CAPSLOCK_STATUS);
        }

	if (get_kb_status(KB_LCD_STATUS) == OFF)
		return;
	//don't allow thread sleep, so async
	queue_work(lenovo_kb_core->lenovo_kb_workqueue,
			&lenovo_kb_core->capsled_work);
}

static int lenovo_kb_set_software_power(int power_status)
{
	int ret=  0;
	char power_buf[] = {0x6A, 0x03, 0x36, 0x01, 0x01};
	char buf[] = {0x6B, 0x03, 0x36, 0x01, 0x01};
	char read_buf[UART_BUFFER_SIZE] = {0};
	int read_len = 0;
	int i = 0;

	kb_info("set power_status:%d\n", power_status);

	if(power_status) {
		power_buf[4] = 0x00;
		buf[4] = 0x00;
	} else {
		power_buf[4] = 0x01;
		buf[4] = 0x01;
	}

	for (i = 0; i < 3; i++) {
		ret = lenovo_kb_transfer_data(power_buf, sizeof(power_buf),
							read_buf, &read_len);
		if(ret)
			continue;

		if(memcmp(read_buf, buf, sizeof(buf)) == 0)
			break;
	}

	if(i >= 3){
		kb_err("fail ret:0x%02x status:%d\n", ret, read_buf[4]);
		return ret;
	}

	sprintf(TAG, "%s %d", __func__, __LINE__);
	lenovo_kb_show_buf(read_buf, read_len);
	kb_info("ok\n");
	return 0;
}

static void lenovo_kb_touchpad_autosleep(bool enable)
{
	char cmd[] = {0x6A, 0x04, 0x33, 0x02, 0x00, 0x00};
	int ret;

	if (enable) {
		cmd[4] = 0x01;
		cmd[5] = 0x05;
	}

	ret = lenovo_kb_transfer_data(cmd, sizeof(cmd), NULL, NULL);
	if (ret < 0)
		kb_err("set %d fail\n", enable);
	else
		kb_info("set %d ok\n", enable);
}

static void lenovo_kb_release_key(struct lenovo_keyboard_data *core)
{
        /* Keys that are pressed now are unlikely to be
         * still pressed when we resume.
         */
        memset(core->old, 0, sizeof(core->old));
        memset(core->mm_old, 0, sizeof(core->mm_old));
        if (core->keyboard != NULL) {
                int code;
                bool need_sync = false;

                for_each_set_bit(code, core->keyboard->key, KEY_CNT) {
                        input_report_key(core->keyboard, code, 0);
                        need_sync = true;
                }

                if (need_sync)
                        input_sync(core->keyboard);
        }
}

static void lenovo_kb_lcd_work(struct work_struct *work)
{
	struct lenovo_keyboard_data *core =
		container_of(work, struct lenovo_keyboard_data, lcd_work);
	int ret;

	if (get_kb_status(KB_LCD_STATUS) == ON) {
		if(get_kb_status(KB_CONNECT_STATUS) == CONNECTED){
			ret = lenovo_kb_set_software_power(1);
			if(ret)
				kb_err("lcd on set software power on err!\n");

			lenovo_kb_restore_leds();
			if (get_kb_status(KB_TP_ASLEEP_EN_STATUS) &&
					core->lid_change_sleep) {
				kb_info("restore tp autosleep");
				lenovo_kb_touchpad_autosleep(true);
				core->lid_change_sleep = false;
			}
		}
	} else {
		if(get_kb_status(KB_CONNECT_STATUS) == CONNECTED){
			if (get_kb_status(KB_TP_ASLEEP_EN_STATUS)) {
				if (core->lid_gpio > 0 &&
					!gpio_get_value(core->lid_gpio)) {
					kb_info("lid closed, disable autosleep");
					lenovo_kb_touchpad_autosleep(false);
					core->lid_change_sleep = true;
				}
			}
			ret = lenovo_kb_set_software_power(0);
			if(ret)
				kb_err("lcd off set software power off err!\n");
		}
		lenovo_kb_release_key(core);
	}
}

static void
lenovo_panel_notifier_callback(enum panel_event_notifier_tag tag,
                            struct panel_event_notification *notification,
                            void *client_data)
{
	struct lenovo_keyboard_data *core =
		(struct lenovo_keyboard_data *)client_data;

        if (!notification) {
                kb_err("Invalid notification\n");
                return;
        }

        kb_debug("receive panel event type:%d", notification->notif_type);

        switch (notification->notif_type) {
        case DRM_PANEL_EVENT_UNBLANK:
		if (!notification->notif_data.early_trigger) {
			kb_info("resume");
			set_kb_status(KB_LCD_STATUS);
			queue_work(core->lenovo_kb_workqueue, &core->lcd_work);
		}
		break;

        case DRM_PANEL_EVENT_BLANK:
		if (notification->notif_data.early_trigger) {
			kb_info("suspend");
			clear_kb_status(KB_LCD_STATUS);
			pm_wakeup_dev_event(&core->plat_dev->dev, 1000, true);
			queue_work(core->lenovo_kb_workqueue, &core->lcd_work);
		}
                break;

        default:
                kb_debug("ignore panel event type:%d\n",
                        notification->notif_type);
                break;
        }
}

static void lenovo_kb_power_enable(int value){
	if(!lenovo_kb_core || lenovo_kb_core->power_en_gpio == 0)
		return;

	if(value == 1)
		gpio_direction_output(lenovo_kb_core->power_en_gpio,1);
	else if(value == 0)
		gpio_direction_output(lenovo_kb_core->power_en_gpio,0);

	msleep(30);

	kb_info("enable:%d\n", value);
}

static void lenovo_kb_check_keyboard(void)
{
        int value = 0;

        value = gpio_get_value(lenovo_kb_core->plug_gpio);
        if(value == 0 && (get_kb_status(KB_CONNECT_STATUS) == UNCONNECTED)) {
                if (gpio_get_value(lenovo_kb_core->power_en_gpio) == 1) {
                        lenovo_kb_power_enable(0);
                        msleep(20);
                }
		lenovo_kb_power_enable(1);
        }

        kb_info("keyboard hall status:%s\n", value ? "far" : "near");
        return;
}

static ssize_t tx_mode_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	char *after;
	unsigned long value = simple_strtoul(buf, &after, 10);

	kb_info("%d\n", value);
	if(value == 1)
		lenovo_kb_enable_uart_tx(1);
	else if(value == 0)
		lenovo_kb_enable_uart_tx(0);

	return count;
}

static ssize_t kb_hall_status_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int value = 0;
	value = gpio_get_value(lenovo_kb_core->plug_gpio);

	return sprintf(buf, "%d\n", value);
}

static ssize_t kb_power_status_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
        int value = 0;
        value = gpio_get_value(lenovo_kb_core->power_en_gpio);

        return sprintf(buf, "%d\n", value);
}

static ssize_t kb_connect_status_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int connect_status = 0;
	connect_status = get_kb_status(KB_CONNECT_STATUS);

	return sprintf(buf, "%d\n", connect_status);
}

static ssize_t keyboard_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "status:0x%2x\n", lenovo_kb_core->kb_status);
}

static void lenovo_kb_set_mute(bool enable)
{
        char cmd[] = {0x6A, 0x03, 0x28, 0x01, 0x00};
        int ret;

        if (enable)
                cmd[4] = 0x01;

        ret = lenovo_kb_transfer_data(cmd, sizeof(cmd), NULL, NULL);
        if (ret < 0)
                kb_err("set %d fail\n", enable);
        else
                kb_info("set %d ok\n", enable);
}

static void lenovo_kb_set_micmute(bool enable)
{
        char cmd[] = {0x6A, 0x03, 0x29, 0x01, 0x00};
        int ret;

        if (enable)
                cmd[4] = 0x01;

        ret = lenovo_kb_transfer_data(cmd, sizeof(cmd), NULL, NULL);
        if (ret < 0)
                kb_err("set %d fail\n", enable);
        else
                kb_info("set %d ok\n", enable);
}

static void lenovo_kb_touchpad_enable(bool enable)
{
	char cmd[] = {0x6A, 0x03, 0x21, 0x01, 0x00};
	int ret;

	if (enable)
		cmd[4] = 0x01;

	ret = lenovo_kb_transfer_data(cmd, sizeof(cmd), NULL, NULL);
	if (ret < 0)
		kb_err("set %d fail\n", enable);
	else
		kb_info("set %d ok\n", enable);
}

#define IS_CONDITION_OK \
		get_kb_status(KB_CONNECT_STATUS) == CONNECTED && \
		get_kb_status(KB_LCD_STATUS) == ON \

static ssize_t keyboard_status_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	int cmd;

	if (!buf || count <= 0)
		return -EINVAL;

	if (sscanf(buf, "%d", &cmd) != 1) {
		kb_err("get cmd failed\n");
		return -EINVAL;
	}

	kb_info("framework send cmd:%s", fwk_cmd[cmd]);
	switch (cmd) {
	case FWK_MUTE_ON:
		set_kb_status(KB_MUTE_STATUS);
		if (IS_CONDITION_OK)
			lenovo_kb_set_mute(true);
		break;
	case FWK_MUTE_OFF:
		clear_kb_status(KB_MUTE_STATUS);
		if (IS_CONDITION_OK)
			lenovo_kb_set_mute(false);
		break;
	case FWK_MICMUTE_ON:
		set_kb_status(KB_MICMUTE_STATUS);
		if (IS_CONDITION_OK)
			lenovo_kb_set_micmute(true);
		break;
	case FWK_MICMUTE_OFF:
		clear_kb_status(KB_MICMUTE_STATUS);
		if (IS_CONDITION_OK)
			lenovo_kb_set_micmute(false);
		break;
	case FWK_UART_INIT_DONE:
		lenovo_kb_check_keyboard();
		break;
	case FWK_TOUCH_ENABLE:
		set_kb_status(KB_TOUCH_EN_STATUS);
		lenovo_kb_touchpad_enable(true);
		break;
	case FWK_TOUCH_DISABLE:
		clear_kb_status(KB_TOUCH_EN_STATUS);
		lenovo_kb_touchpad_enable(false);
		break;
	case FWK_TP_AUTOSLEEP_ENABLE:
		set_kb_status(KB_TP_ASLEEP_EN_STATUS);
		lenovo_kb_touchpad_autosleep(true);
		break;
	case FWK_TP_AUTOSLEEP_DISABLE:
		clear_kb_status(KB_TP_ASLEEP_EN_STATUS);
		lenovo_kb_touchpad_autosleep(false);
		break;
	default:
		kb_err("not support:%d", cmd);
		break;
	}

        return count;
}

/* debug level show */
static ssize_t debug_log_show(struct device *dev,
                                       struct device_attribute *attr,
                                       char *buf)
{
        int r = 0;

        r = snprintf(buf, PAGE_SIZE, "state:%s\n",
                    debug_log_flag ?
                    "enabled" : "disabled");

        return r;
}

/* debug level store */
static ssize_t debug_log_store(struct device *dev,
                                        struct device_attribute *attr,
                                        const char *buf, size_t count)
{
        if (!buf || count <= 0)
                return -EINVAL;

        if (buf[0] != '0')
                debug_log_flag = true;
        else
                debug_log_flag = false;
        return count;
}

static DEVICE_ATTR(tx_mode, S_IRUGO | S_IWUSR, NULL, tx_mode_store);
static DEVICE_ATTR(keyboard_status, S_IRUGO | S_IWUSR,
					keyboard_status_show,
					keyboard_status_store);
static DEVICE_ATTR(kb_hall_status, S_IRUGO | S_IWUSR,
					kb_hall_status_show, NULL);
static DEVICE_ATTR(kb_power_status, S_IRUGO | S_IWUSR,
					kb_power_status_show, NULL);
static DEVICE_ATTR(kb_connect_status, S_IRUGO | S_IWUSR,
					kb_connect_status_show, NULL);
static DEVICE_ATTR(debug_log, S_IRUGO | S_IWUSR,
				debug_log_show,
				debug_log_store);

/* add your attr in here*/
static struct attribute *lenovo_kb_attributes[] = {
	&dev_attr_tx_mode.attr,
	&dev_attr_keyboard_status.attr,
	&dev_attr_kb_hall_status.attr,
	&dev_attr_kb_power_status.attr,
	&dev_attr_kb_connect_status.attr,
	&dev_attr_debug_log.attr,
	NULL
};

static struct attribute_group lenovo_kb_attribute_group = {
	.attrs = lenovo_kb_attributes
};

static int lenovo_kb_get_dts_info(struct lenovo_keyboard_data *core)
{
        struct device_node *node = NULL;

        kb_info("start\n");
        node = of_find_compatible_node(NULL, NULL, KEYBOARD_CORE_NAME);
        if (!node) {
                kb_err("of node is not null\n");
                return -EINVAL;
        }

        core->plug_gpio = of_get_named_gpio(node, "plug-gpios", 0);
        if (!gpio_is_valid(core->plug_gpio)) {
                kb_err("plug_gpio is not valid %d\n", core->plug_gpio);
                return -EINVAL;
        }

        core->power_en_gpio = of_get_named_gpio(node, "power-en-gpios", 0);
        if (!gpio_is_valid(core->power_en_gpio)) {
                kb_err("power_en_gpio is not valid %d\n", core->power_en_gpio);
                return -EINVAL;
        }

        core->uart_wake_gpio = of_get_named_gpio(node, "uart-wake-gpios", 0);
        if (!gpio_is_valid(core->uart_wake_gpio)) {
                kb_err("uart_wake_gpio is not valid %d\n", core->uart_wake_gpio);
                return -EINVAL;
        }

        core->tx_en_gpio = of_get_named_gpio(node, "tx-en-gpios", 0);
        if (!gpio_is_valid(core->tx_en_gpio)) {
                kb_err("tx_en_gpio is not valid %d\n", core->tx_en_gpio);
                return -EINVAL;
        }

        /* less important */
        core->lid_gpio = of_get_named_gpio(node, "lid-gpios", 0);
        if (!gpio_is_valid(core->lid_gpio))
                kb_err("lid_gpio is not valid %d\n", core->lid_gpio);

        kb_info("end\n");

        return 0;
}

static int lenovo_kb_gpio_setup(struct lenovo_keyboard_data *core)
{
	int ret = 0;

	ret = devm_gpio_request_one(&core->plat_dev->dev, core->power_en_gpio,
					GPIOF_OUT_INIT_LOW, "kb_power_gpio");
	if (ret < 0) {
                kb_err("Failed to request power_en_gpio, ret:%d", ret);
                return ret;
        }

	ret = devm_gpio_request_one(&core->plat_dev->dev, core->tx_en_gpio,
					GPIOF_OUT_INIT_LOW, "kb_tx_gpio");
        if (ret < 0) {
		kb_err("Failed to request tx_en_gpio, ret:%d", ret);
                return ret;
        }

	ret = devm_gpio_request_one(&core->plat_dev->dev, core->plug_gpio,
					GPIOF_DIR_IN, "kb_plug_gpio");
        if (ret < 0) {
                kb_err("Failed to request plug_gpio, ret:%d\n", ret);
                return ret;
        }

	ret = devm_gpio_request_one(&core->plat_dev->dev, core->uart_wake_gpio,
					GPIOF_DIR_IN, "uart_wake_gpio");
        if (ret < 0) {
                kb_err("Failed to request uart_wake_gpio, ret:%d\n", ret);
                return ret;
        }

	return 0;
}

static void lenovo_kb_hall_work(struct work_struct *work)
{
	int plug_status = gpio_get_value(lenovo_kb_core->plug_gpio);
	if (plug_status == 0) {
		lenovo_kb_power_enable(1);
	} else {
		lenovo_kb_power_enable(0);
		if(get_kb_status(KB_CONNECT_STATUS) == CONNECTED){
			lenovo_kb_input_dev_destory();
			clear_kb_status(KB_CONNECT_STATUS);
		}
	}
}

static irqreturn_t lenovo_kb_plug_irq_handler(int irq, void *data)
{
	struct lenovo_keyboard_data *core =
		(struct lenovo_keyboard_data *)data;
        int value = 0;

        value = gpio_get_value(core->plug_gpio);
        if(value == 1)
                irq_set_irq_type(core->plug_irq, IRQ_TYPE_LEVEL_LOW);
        else
                irq_set_irq_type(core->plug_irq, IRQ_TYPE_LEVEL_HIGH);

        kb_info("plug irq gpio = %d\n", value);
	pm_wakeup_dev_event(&core->plat_dev->dev, 2000, true);
	queue_delayed_work(core->lenovo_kb_workqueue,
			&core->hall_work, msecs_to_jiffies(50));
        return IRQ_HANDLED;
}

static int lenovo_kb_plug_irq_config(struct lenovo_keyboard_data *core)
{
	struct platform_device *device = core->plat_dev;
	int ret = 0;

	core->plug_irq = gpio_to_irq(core->plug_gpio);
	if (core->plug_irq < 0) {
		kb_err("failed get plug irq num %d", core->plug_irq);
		return -EINVAL;
	}

	ret = devm_request_threaded_irq(&device->dev, core->plug_irq,
					lenovo_kb_plug_irq_handler, NULL,
					IRQF_TRIGGER_LOW | IRQF_ONESHOT,
					"keyboard_plug_irq", core);
	if (ret < 0) {
		kb_err("request irq failed : %d\n", core->plug_irq);
		return -EINVAL;
	}

	return 0;
}

static irqreturn_t lenovo_kb_wake_irq_handler(int irq, void *data)
{
	struct lenovo_keyboard_data *core =
                (struct lenovo_keyboard_data *)data;

        pm_wakeup_dev_event(&core->plat_dev->dev, 1000, true);
        return IRQ_HANDLED;
}

static int lenovo_kb_wake_irq_config(struct lenovo_keyboard_data *core)
{
	struct platform_device *device = core->plat_dev;
	int ret;

	core->uart_wake_irq = gpio_to_irq(core->uart_wake_gpio);
	if (core->uart_wake_irq < 0) {
		kb_err("failed get uart wake irq num %d", core->uart_wake_irq);
		return -EINVAL;
	}

	ret = devm_request_threaded_irq(&device->dev, core->uart_wake_irq,
					lenovo_kb_wake_irq_handler, NULL,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"keyboard_wake_irq", core);
	if (ret < 0) {
		kb_err("request wake irq failed : %d\n", core->uart_wake_irq);
		return -EINVAL;
	}

	disable_irq(core->uart_wake_irq);
	return 0;
}

static void lenovo_kb_connect_work(struct work_struct *work)
{
	int ret;

	ret = lenovo_kb_input_dev_create();
	if(ret)
		kb_err("lenovo_keyboard input dev create err!\n");

	set_kb_status(KB_CONNECT_STATUS);
}

static void lenovo_kb_restore_work(struct work_struct *work)
{
	if (IS_CONDITION_OK)
		lenovo_kb_restore_leds();

	if (get_kb_status(KB_TP_ASLEEP_EN_STATUS))
		lenovo_kb_touchpad_autosleep(true);
}

static int lenovo_kb_probe(struct platform_device  *device)
{
	int ret = 0;
	struct device *dev = &device->dev;
	void *cookie = NULL;

	kb_info("start\n");
	lenovo_kb_core = devm_kzalloc(dev, sizeof(*lenovo_kb_core), GFP_KERNEL);
	if(!lenovo_kb_core){
		kb_err("lenovo_kb_core kzalloc err\n");
		return -ENOMEM;
	}

	lenovo_kb_core->plat_dev = device;
	mutex_init(&lenovo_kb_core->mutex);
#if defined(CONFIG_DRM_PANEL)
	ret = panel_check_dt(dev->of_node);
	if (ret) {
		kb_err("no drm panel\n");
		return ret;
	}
#endif
	lenovo_kb_core->lenovo_kb_workqueue = create_workqueue("lenovo_kb_wq");
	if (!lenovo_kb_core->lenovo_kb_workqueue) {
		kb_err("create workqueue fail\n");
		return -ENOMEM;
	}

	INIT_DELAYED_WORK(&lenovo_kb_core->hall_work, lenovo_kb_hall_work);
	INIT_WORK(&lenovo_kb_core->connect_work, lenovo_kb_connect_work);
	INIT_WORK(&lenovo_kb_core->lcd_work, lenovo_kb_lcd_work);
	INIT_WORK(&lenovo_kb_core->capsled_work, lenovo_kb_capsled_work);
	INIT_WORK(&lenovo_kb_core->restore_work, lenovo_kb_restore_work);

	if (active_panel)
		cookie = panel_event_notifier_register(
				PANEL_EVENT_NOTIFICATION_PRIMARY,
				PANEL_EVENT_NOTIFIER_CLIENT_KEYBOARD, active_panel,
				&lenovo_panel_notifier_callback, lenovo_kb_core);

	if (IS_ERR(cookie)) {
                ret = PTR_ERR(cookie);
                kb_err("Failed to register panel event notifier, ret=%d\n", ret);
                goto exit_register_panel;
        }
	lenovo_kb_core->cookie = cookie;

	ret = sysfs_create_group(&device->dev.kobj, &lenovo_kb_attribute_group);
	if (ret != 0) {
		kb_err("sysfs_create_group err\n");
		sysfs_remove_group(&device->dev.kobj, &lenovo_kb_attribute_group);
		goto exit_sysfs;
	}

	ret = lenovo_kb_get_dts_info(lenovo_kb_core);
	if(ret != 0)
		goto exit_out;

	ret = lenovo_kb_gpio_setup(lenovo_kb_core);
	if(ret != 0)
		goto exit_out;

	ret = lenovo_kb_plug_irq_config(lenovo_kb_core);
	if(ret != 0)
		goto exit_out;

	ret = lenovo_kb_wake_irq_config(lenovo_kb_core);
	if(ret != 0)
                goto exit_out;

	device_init_wakeup(dev, true);
	kb_info("ok\n");
	return 0;

exit_out:
	sysfs_remove_group(&device->dev.kobj, &lenovo_kb_attribute_group);
exit_sysfs:
	panel_event_notifier_unregister(lenovo_kb_core->cookie);
exit_register_panel:
	destroy_workqueue(lenovo_kb_core->lenovo_kb_workqueue);
	return ret;
}

static int lenovo_kb_remove(struct platform_device *device) {
	struct device *dev = &device->dev;

	if (lenovo_kb_core->cookie)
		panel_event_notifier_unregister(lenovo_kb_core->cookie);
	if (lenovo_kb_core->lenovo_kb_workqueue)
		destroy_workqueue(lenovo_kb_core->lenovo_kb_workqueue);

	lenovo_kb_input_dev_destory();
	sysfs_remove_group(&device->dev.kobj, &lenovo_kb_attribute_group);
	device_init_wakeup(dev, false);
	kb_info("end\n");
	return 0;
}

static int lenovo_kb_suspend(struct platform_device *device, pm_message_t state){
	enable_irq(lenovo_kb_core->uart_wake_irq);
	enable_irq_wake(lenovo_kb_core->uart_wake_irq);
	enable_irq_wake(lenovo_kb_core->plug_irq);
	kb_info("end\n");
	return 0;
}

static int lenovo_kb_resume(struct platform_device *device){
	disable_irq(lenovo_kb_core->uart_wake_irq);
	disable_irq_wake(lenovo_kb_core->uart_wake_irq);
	disable_irq_wake(lenovo_kb_core->plug_irq);
	kb_info("end\n");
	return 0;
}

static const struct of_device_id lenovo_kb_plat_of_match[] = {
	{.compatible = KEYBOARD_CORE_NAME,},
	{},
};

static struct platform_driver lenovo_kb_plat_driver ={
	.probe = lenovo_kb_probe,
	.remove = lenovo_kb_remove,
	.suspend = lenovo_kb_suspend,
	.resume = lenovo_kb_resume,
	.driver = {
		.name = KEYBOARD_CORE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = lenovo_kb_plat_of_match,
	}
};

static int __init lenovo_kb_mod_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&lenovo_kb_plat_driver);
	if(ret){
		kb_err("platform_driver_register err\n");
		return ret;
	}

	kb_info("ok\n");
	return 0;
}

static void __exit lenovo_kb_mod_exit(void)
{
	platform_driver_unregister(&lenovo_kb_plat_driver);
	kb_info("ok\n");
}

late_initcall(lenovo_kb_mod_init);
module_exit(lenovo_kb_mod_exit);

MODULE_AUTHOR("Lenovo Team Inc");
MODULE_DESCRIPTION("Lenovo Keyboard Driver v1.0");
MODULE_LICENSE("GPL");
