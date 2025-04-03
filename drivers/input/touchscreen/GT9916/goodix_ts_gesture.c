/*
 * Goodix Gesture Module
 *
 * Copyright (C) 2019 - 2020 Goodix, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/input/mt.h>
#include "goodix_ts_core.h"


#define GOODIX_GESTURE_DOUBLE_TAP		0xCC
#define GOODIX_GESTURE_SINGLE_TAP		0x4C
#define GOODIX_GESTURE_FOD_DOWN			0x46
#define GOODIX_GESTURE_FOD_UP			0x55

/*N6 code for HQ-305058 by zhangzhijian5 at 2023/10/24 start*/
#define WAKEUP_OFF				0x04
#define WAKEUP_ON				0x05
/*N6 code for HQ-305058 by zhangzhijian5 at 2023/10/24 end*/

/*N6 code for HQ-334519 by liaoxianguo at 2023/10/17 start*/
#define QUERYBIT(longlong, bit) (!!(longlong[bit/8] & (1 << bit%8)))
#define GSX_GESTURE_TYPE_LEN	32
#define TYPE_B_PROTOCOL
static int  FP_Event_Gesture;
/*N6 code for HQ-334519 by liaoxianguo at 2023/10/17 start*/
/*
 * struct gesture_module - gesture module data
 * @registered: module register state
 * @sysfs_node_created: sysfs node state
 * @gesture_type: valid gesture type, each bit represent one gesture type
 * @gesture_data: store latest gesture code get from irq event
 * @gesture_ts_cmd: gesture command data
 */
struct gesture_module {
	atomic_t registered;
	struct goodix_ts_core *ts_core;
	/*N6 code for HQ-334519 by liaoxianguo at 2023/10/17 start*/
	u8 gesture_type[GSX_GESTURE_TYPE_LEN];
	u8 gesture_data;
	/*N6 code for HQ-334519 by liaoxianguo at 2023/10/17 start*/
	struct goodix_ext_module module;
};

static struct gesture_module *gsx_gesture; /*allocated in gesture init module*/
static bool module_initialized;

/*N6 code for HQ-334519 by liaoxianguo at 2023/10/17 start*/
int goodix_gesture_enable(int enable)
{
	int ret = 0;
	if (!module_initialized)
		return 0;
	ts_info("enable is %d", enable);
	if (enable) {
		if (atomic_read(&gsx_gesture->registered))
			ts_info("gesture module has been already registered");
		else
			ret = goodix_register_ext_module_no_wait(&gsx_gesture->module);
	} else {
		if (!atomic_read(&gsx_gesture->registered))
			ts_info("gesture module has been already unregistered");
		else
			ret = goodix_unregister_ext_module(&gsx_gesture->module);
	}
	return ret;
}
/*N6 code for HQ-334519 by liaoxianguo at 2023/10/17 end*/

static ssize_t gsx_double_type_show(struct goodix_ext_module *module,
		char *buf)
{
	struct gesture_module *gsx = module->priv_data;
	/*N6 code for HQ-305058 by zhangzhijian5 at 2023/10/24 start*/
	u32 type = 0;
	/*N6 code for HQ-305058 by zhangzhijian5 at 2023/10/24 end*/

	if (!gsx)
		return -EIO;

	type = gsx->ts_core->gesture_type;

	if (atomic_read(&gsx->registered) == 0) {
		ts_err("gesture module is not registered");
		return 0;
	}

	return sprintf(buf, "%s\n",
			(type & GESTURE_DOUBLE_TAP) ? "enable" : "disable");
}

static ssize_t gsx_double_type_store(struct goodix_ext_module *module,
		const char *buf, size_t count)
{
	struct gesture_module *gsx = module->priv_data;

	if (!gsx)
		return -EIO;

	if (atomic_read(&gsx->registered) == 0) {
		ts_err("gesture module is not registered");
		return 0;
	}

	if (buf[0] == '1') {
		ts_info("enable double tap");
		gsx->ts_core->gesture_type |= GESTURE_DOUBLE_TAP;
	} else if (buf[0] == '0') {
		ts_info("disable double tap");
		gsx->ts_core->gesture_type &= ~GESTURE_DOUBLE_TAP;
	} else
		ts_err("invalid cmd[%d]", buf[0]);

	return count;
}

static ssize_t gsx_single_type_show(struct goodix_ext_module *module,
		char *buf)
{
	struct gesture_module *gsx = module->priv_data;
	/*N6 code for HQ-305058 by zhangzhijian5 at 2023/10/24 start*/
	u32 type = 0;
	/*N6 code for HQ-305058 by zhangzhijian5 at 2023/10/24 end*/

	if (!gsx)
		return -EIO;

	type = gsx->ts_core->gesture_type;

	if (atomic_read(&gsx->registered) == 0) {
		ts_err("gesture module is not registered");
		return 0;
	}

	return sprintf(buf, "%s\n",
			(type & GESTURE_SINGLE_TAP) ? "enable" : "disable");
}

static ssize_t gsx_single_type_store(struct goodix_ext_module *module,
		const char *buf, size_t count)
{
	struct gesture_module *gsx = module->priv_data;

	if (!gsx)
		return -EIO;

	if (atomic_read(&gsx->registered) == 0) {
		ts_err("gesture module is not registered");
		return 0;
	}

	if (buf[0] == '1') {
		ts_info("enable single tap");
		gsx->ts_core->gesture_type |= GESTURE_SINGLE_TAP;
	} else if (buf[0] == '0') {
		ts_info("disable single tap");
		gsx->ts_core->gesture_type &= ~GESTURE_SINGLE_TAP;
	} else
		ts_err("invalid cmd[%d]", buf[0]);

	return count;
}

static ssize_t gsx_fod_type_show(struct goodix_ext_module *module,
		char *buf)
{
	struct gesture_module *gsx = module->priv_data;
	/*N6 code for HQ-305058 by zhangzhijian5 at 2023/10/24 start*/
	u32 type = 0;
	/*N6 code for HQ-305058 by zhangzhijian5 at 2023/10/24 end*/

	if (!gsx)
		return -EIO;

	type = gsx->ts_core->gesture_type;

	if (atomic_read(&gsx->registered) == 0) {
		ts_err("gesture module is not registered");
		return 0;
	}

	return sprintf(buf, "%s\n",
			(type & GESTURE_FOD_PRESS) ? "enable" : "disable");
}

static ssize_t gsx_fod_type_store(struct goodix_ext_module *module,
		const char *buf, size_t count)
{
	struct gesture_module *gsx = module->priv_data;

	if (!gsx)
		return -EIO;

	if (atomic_read(&gsx->registered) == 0) {
		ts_err("gesture module is not registered");
		return 0;
	}

	if (buf[0] == '1') {
		ts_info("enable fod");
		gsx->ts_core->gesture_type |= GESTURE_FOD_PRESS;
	} else if (buf[0] == '0') {
		ts_info("disable fod");
		gsx->ts_core->gesture_type &= ~GESTURE_FOD_PRESS;
	} else
		ts_err("invalid cmd[%d]", buf[0]);

	return count;
}


const struct goodix_ext_attribute gesture_attrs[] = {
	__EXTMOD_ATTR(double_en, 0664,
			gsx_double_type_show, gsx_double_type_store),
	__EXTMOD_ATTR(single_en, 0664,
			gsx_single_type_show, gsx_single_type_store),
	__EXTMOD_ATTR(fod_en, 0664,
			gsx_fod_type_show, gsx_fod_type_store),
};

static int gsx_gesture_init(struct goodix_ts_core *cd,
		struct goodix_ext_module *module)
{
	struct gesture_module *gsx = module->priv_data;

	if (!cd || !cd->hw_ops->gesture) {
		ts_err("gesture unsupported");
		return -EINVAL;
	}

	gsx->ts_core = cd;
	gsx->ts_core->gesture_type = 0;
	atomic_set(&gsx->registered, 1);

	return 0;
}

static int gsx_gesture_exit(struct goodix_ts_core *cd,
		struct goodix_ext_module *module)
{
	struct gesture_module *gsx = module->priv_data;

	if (!cd || !cd->hw_ops->gesture) {
		ts_err("gesture unsupported");
		return -EINVAL;
	}

	atomic_set(&gsx->registered, 0);

	return 0;
}

/**
 * gsx_gesture_ist - Gesture Irq handle
 * This functions is excuted when interrupt happended and
 * ic in doze mode.
 *
 * @cd: pointer to touch core data
 * @module: pointer to goodix_ext_module struct
 * return: 0 goon execute, EVT_CANCEL_IRQEVT  stop execute
 */
/*N6 code for HQ-334519 by liaoxianguo at 2023/10/17 start*/
static int gsx_gesture_ist(struct goodix_ts_core *cd,
	struct goodix_ext_module *module)
{
	struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;
	struct goodix_ts_event gs_event = {0};
	int fodx = 0, fody = 0, fod_id = 0;
	int overlay_area = 0;
	int ret = 0;
	int key_value = 0;
	u8 gesture_data[32] = {0};

	if (atomic_read(&cd->suspended) == 0)
		return EVT_CONTINUE;

	mutex_lock(&cd->report_mutex);
	ret = hw_ops->event_handler(cd, &gs_event);
	if (ret) {
		ts_err("failed get gesture data");
		goto re_send_ges_cmd;
	}

	if (!(gs_event.event_type & EVENT_GESTURE)) {
		ts_err("invalid event type: 0x%x",
			cd->ts_event.event_type);
		goto re_send_ges_cmd;
	}

	memcpy(gesture_data, gs_event.touch_data.tmp_data, 32 * sizeof(u8));
	if ((gesture_data[0] & 0x08)  != 0)
		FP_Event_Gesture = 1;

	fod_id = gesture_data[17];
	if ((cd->fod_status != 0 && cd->fod_status != -1) && (FP_Event_Gesture == 1) &&
		(gs_event.gesture_type == GOODIX_GESTURE_FOD_DOWN)) {
		fodx = gesture_data[8] | (gesture_data[9] << 8);
		fody = gesture_data[10] | (gesture_data[11] << 8);
		overlay_area = gesture_data[12];
		ts_info("gesture coordinate fodx:0x%x, fody:0x%x, overlay_area:0x%x",
			    fodx, fody, overlay_area);
		ts_info("fod down");
			input_report_key(cd->input_dev, BTN_INFO, 1);
			input_sync(cd->input_dev);
#ifdef TYPE_B_PROTOCOL
			input_mt_slot(cd->input_dev, fod_id);
			ts_info("fod id:%d", fod_id);
			input_mt_report_slot_state(cd->input_dev,
					MT_TOOL_FINGER, 1);
#endif
			input_report_key(cd->input_dev, BTN_TOUCH, 1);
			input_report_key(cd->input_dev, BTN_TOOL_FINGER, 1);
			input_report_abs(cd->input_dev, ABS_MT_POSITION_X, fodx);
			input_report_abs(cd->input_dev, ABS_MT_POSITION_Y, fody);
			input_report_abs(cd->input_dev, ABS_MT_WIDTH_MAJOR, overlay_area);
			input_report_abs(cd->input_dev, ABS_MT_WIDTH_MINOR, overlay_area);
			input_sync(cd->input_dev);
			/* mi_disp_set_fod_queue_work(1, true); */
			cd->fod_finger = true;
			update_fod_press_status(1);
			FP_Event_Gesture = 0;
			goto re_send_ges_cmd;
	}
	if  ((FP_Event_Gesture == 1) && (gs_event.gesture_type == GOODIX_GESTURE_FOD_UP)) {
		if (cd->fod_finger) {
			ts_info("fod finger is %d", cd->fod_finger);
			ts_info("fod up");
			cd->fod_finger = false;
			input_report_key(cd->input_dev, BTN_INFO, 0);
			input_report_abs(cd->input_dev, ABS_MT_WIDTH_MAJOR, 0);
			input_report_abs(cd->input_dev, ABS_MT_WIDTH_MINOR, 0);
			input_sync(cd->input_dev);
#ifdef TYPE_B_PROTOCOL
			input_mt_slot(cd->input_dev, fod_id);
			ts_info("fod id:%d", fod_id);
			input_mt_report_slot_state(cd->input_dev,
					MT_TOOL_FINGER, 0);
#endif
			input_report_key(cd->input_dev, BTN_TOUCH, 0);
			input_report_key(cd->input_dev, BTN_TOOL_FINGER, 0);
			input_sync(cd->input_dev);
			update_fod_press_status(0);
			/* mi_disp_set_fod_queue_work(0, true); */
		}
		goto re_send_ges_cmd;
	}
	/*N6 code for HQ-337468 by zhangzhijian5 at 2023/10/28 start*/
	if (gs_event.gesture_type == GOODIX_GESTURE_SINGLE_TAP) {
		ts_info("GTP got valid gesture type 0x%x", gs_event.gesture_type);
		if (cd->gesture_type & GESTURE_SINGLE_TAP) {
			ts_info("get SINGLE-TAP gesture");
			input_report_key(cd->input_dev, KEY_GOTO, 1);
			input_sync(cd->input_dev);
			input_report_key(cd->input_dev, KEY_GOTO, 0);
			input_sync(cd->input_dev);
		} else {
			ts_debug("not enable SINGLE-TAP");
		}
		/*N6 code for HQ-342824 by zhangzhijian5 at 2023/11/7 start*/
		goto re_send_ges_cmd;
		/*N6 code for HQ-342824 by zhangzhijian5 at 2023/11/7 end*/
	}
	/*N6 code for HQ-337468 by zhangzhijian5 at 2023/10/28 end*/
	/*N6 code for HQ-305058 by zhangzhijian5 at 2023/10/24 start*/
	if (gs_event.gesture_type == GOODIX_GESTURE_DOUBLE_TAP) {
		ts_info("GTP got valid gesture type 0x%x", gs_event.gesture_type);
		key_value = KEY_POWER;
		if (cd->gesture_type & GESTURE_DOUBLE_TAP) {
			ts_info("get DOUBLE-TAP gesture");
			input_report_key(cd->input_dev, key_value, 1);
			input_sync(cd->input_dev);
			input_report_key(cd->input_dev, key_value, 0);
			input_sync(cd->input_dev);
		} else {
			ts_debug("not enable DOUBLE-TAP");
		}
	/*N6 code for HQ-305058 by zhangzhijian5 at 2023/10/24 end*/
		goto re_send_ges_cmd;
	} else {
		ts_info("unsupported gesture:%x", gs_event.gesture_type);
	}
re_send_ges_cmd:
	if (hw_ops->gesture(cd, 0))
		ts_info("warning: failed re_send gesture cmd");
	if (!cd->tools_ctrl_sync)
		hw_ops->after_event_handler(cd);
	mutex_unlock(&cd->report_mutex);
	return EVT_CANCEL_IRQEVT;
}
/*N6 code for HQ-334519 by liaoxianguo at 2023/10/17 end*/
/**
 * gsx_gesture_before_suspend - execute gesture suspend routine
 * This functions is excuted to set ic into doze mode
 *
 * @cd: pointer to touch core data
 * @module: pointer to goodix_ext_module struct
 * return: 0 goon execute, EVT_IRQCANCLED  stop execute
 */
static int gsx_gesture_before_suspend(struct goodix_ts_core *cd,
	struct goodix_ext_module *module)
{
	int ret;
	const struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;

	if (cd->gesture_type == 0)
		return EVT_CONTINUE;

	ret = hw_ops->gesture(cd, 0);
	if (ret)
		ts_err("failed enter gesture mode");
	else
		ts_info("enter gesture mode, type[0x%02X]", cd->gesture_type);

	hw_ops->irq_enable(cd, true);
	enable_irq_wake(cd->irq);

	return EVT_CANCEL_SUSPEND;
}

static int gsx_gesture_before_resume(struct goodix_ts_core *cd,
	struct goodix_ext_module *module)
{
	const struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;

	if (cd->gesture_type == 0)
		return EVT_CONTINUE;

	disable_irq_wake(cd->irq);
	hw_ops->reset(cd, GOODIX_NORMAL_RESET_DELAY_MS);

	return EVT_CANCEL_RESUME;
}

/*N6 code for HQ-305058 by zhangzhijian5 at 2023/10/24 start*/
int gsx_gesture_switch(struct input_dev *dev, unsigned int type,
	unsigned int code, int value)
{
	if (type == EV_SYN && code == SYN_CONFIG) {
		if (value == WAKEUP_OFF) {
			gsx_gesture->ts_core->gesture_type &= ~GESTURE_DOUBLE_TAP;
			ts_info("gsx_gesture disabled !");
		} else if (value == WAKEUP_ON) {
			gsx_gesture->ts_core->gesture_type |= GESTURE_DOUBLE_TAP;
			ts_info("gsx_gesture enabled !");
		}
	}

	return EVT_CANCEL_RESUME;
}
/*N6 code for HQ-305058 by zhangzhijian5 at 2023/10/24 end*/

static struct goodix_ext_module_funcs gsx_gesture_funcs = {
	.irq_event = gsx_gesture_ist,
	.init = gsx_gesture_init,
	.exit = gsx_gesture_exit,
	.before_suspend = gsx_gesture_before_suspend,
	.before_resume = gsx_gesture_before_resume,
};

int gesture_module_init(void)
{
	int ret;
	int i;
	struct kobject *def_kobj = goodix_get_default_kobj();
	struct kobj_type *def_kobj_type = goodix_get_default_ktype();

	gsx_gesture = kzalloc(sizeof(struct gesture_module), GFP_KERNEL);
	if (!gsx_gesture)
		return -ENOMEM;

	gsx_gesture->module.funcs = &gsx_gesture_funcs;
	gsx_gesture->module.priority = EXTMOD_PRIO_GESTURE;
	gsx_gesture->module.name = "Goodix_gsx_gesture";
	gsx_gesture->module.priv_data = gsx_gesture;

	atomic_set(&gsx_gesture->registered, 0);

	/* gesture sysfs init */
	ret = kobject_init_and_add(&gsx_gesture->module.kobj,
			def_kobj_type, def_kobj, "gesture");
	if (ret) {
		ts_err("failed create gesture sysfs node!");
		goto err_out;
	}

	for (i = 0; i < ARRAY_SIZE(gesture_attrs) && !ret; i++)
		ret = sysfs_create_file(&gsx_gesture->module.kobj,
				&gesture_attrs[i].attr);
	if (ret) {
		ts_err("failed create gst sysfs files");
		while (--i >= 0)
			sysfs_remove_file(&gsx_gesture->module.kobj,
					&gesture_attrs[i].attr);

		kobject_put(&gsx_gesture->module.kobj);
		goto err_out;
	}

	module_initialized = true;
	goodix_register_ext_module_no_wait(&gsx_gesture->module);
	ts_info("gesture module init success");

	return 0;

err_out:
	ts_err("gesture module init failed!");
	kfree(gsx_gesture);
	return ret;
}

void gesture_module_exit(void)
{
	int i;

	ts_info("gesture module exit");
	if (!module_initialized)
		return;

	goodix_unregister_ext_module(&gsx_gesture->module);

	/* deinit sysfs */
	for (i = 0; i < ARRAY_SIZE(gesture_attrs); i++)
		sysfs_remove_file(&gsx_gesture->module.kobj,
					&gesture_attrs[i].attr);

	kobject_put(&gsx_gesture->module.kobj);
	kfree(gsx_gesture);
	module_initialized = false;
}
