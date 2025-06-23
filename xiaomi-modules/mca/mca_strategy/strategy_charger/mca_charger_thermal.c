// SPDX-License-Identifier: GPL-2.0
/*
 *mca_charger_thermal.c
 *
 * mca strategy class driver
 *
 * Copyright (c) 2023-2023 Xiaomi Technologies Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <mca/platform/platform_wireless_class.h>
#include <mca/common/mca_parse_dts.h>
#include <mca/common/mca_event.h>
#include <mca/common/mca_sysfs.h>
#include <mca/common/mca_voter.h>
#include <mca/common/mca_log.h>
#include <mca/common/mca_smem.h>
#include "inc/mca_charger_thermal.h"
#include <mca/strategy/strategy_class.h>
#include <mca/smartchg/smart_chg_class.h>
#include <linux/types.h>
#include <linux/jiffies.h>
#include <linux/time.h>
#include <linux/timekeeping.h>
#include <mca/protocol/protocol_class.h>
#include <mca/platform/platform_fg_ic_ops.h>
#include <mca/platform/platform_buckchg_class.h>

#ifndef MCA_LOG_TAG
#define MCA_LOG_TAG "mca_thermal"
#endif

static struct mca_thermal_info *g_wlscharger_thermal_info;
static void mca_charger_thermal_update_chg_curr(struct mca_thermal_info *info);

static void mca_wireless_charger_thermal_handle_limit(struct mca_thermal_ctrl_info *ctrl,
	struct mca_wireless_thermal_data *data, struct mca_votable **voter)
{
	struct mca_thermal_info *info = g_wlscharger_thermal_info;
	int *th_data = NULL;
	int chg_curr[THERMAL_MODE_WIRELESS_MAX] = { 0 };
	int phone_flag = 0;
	int i;
	static int count;
	int thermal_level = 0;

	if (!info || !ctrl)
		return;

	if (!ctrl->voter_ok)
		return;

	if (thermal_level > ctrl->max_level) {
		mca_log_err("thermal level invalid\n");
		return;
	}

	thermal_level = info->smartchg_data.wls_super_sts ? ctrl->quickchg_limit_level : ctrl->limit_level;

	phone_flag = thermal_level > info->wireless_phone_level ? 1 : 0;
	mca_strategy_func_process(STRATEGY_FUNC_TYPE_QUICK_WIRELESS,
		MCA_EVENT_WIRELESS_THERMAL_PHONE_FLAG, phone_flag);
	mca_strategy_func_process(STRATEGY_FUNC_TYPE_BASIC_WIRELESS,
		MCA_EVENT_WIRELESS_THERMAL_PHONE_FLAG, phone_flag);

	if (!info->wls_online)
		return;

	if (!thermal_level) {
		chg_curr[THERMAL_MODE_WIRELESS_AUTHEN_20W] = ctrl->chg_curr;
		chg_curr[THERMAL_MODE_WIRELESS_AUTHEN_30W] = ctrl->chg_curr;
		chg_curr[THERMAL_MODE_WIRELESS_AUTHEN_50W] = ctrl->chg_curr;
		chg_curr[THERMAL_MODE_WIRELESS_AUTHEN_80W] = ctrl->chg_curr;
		chg_curr[THERMAL_MODE_WIRELESS_AUTHEN_VOICE_BOX] = ctrl->chg_curr;
		chg_curr[THERMAL_MODE_WIRELESS_AUTHEN_MAGNET_30W] = ctrl->chg_curr;
	} else {
		th_data = &data[thermal_level - 1].wireless_bpp_in;
		for (i = 0; i < THERMAL_MODE_WIRELESS_MAX;  i++) {
			if (!th_data[i]) {
				if (i > THERMAL_MODE_WIRELESS_INLIMIT)
					chg_curr[i] = ctrl->chg_curr;
			} else if (!ctrl->chg_curr) {
				chg_curr[i] = th_data[i];
			} else {
				chg_curr[i] = min(th_data[i], ctrl->chg_curr);
			}
		}
	}

	if (ctrl->wls_thermal_remove) {
		if (!count) {
			for (i = 0; i < THERMAL_MODE_WIRELESS_MAX; i++)
				mca_vote(voter[i], "mca_wireless_thermal", false, chg_curr[i]);
			count = 1;
		}
		mca_log_err("set disable vote %d\n", count);
	} else {
		count = 0;
		mca_log_err("set disable vote %d\n", count);
		for (i = 0; i < THERMAL_MODE_WIRELESS_MAX; i++) {
			if (chg_curr[i])
				mca_vote(voter[i], "mca_wireless_thermal", true, chg_curr[i]);
			else
				mca_vote(voter[i], "mca_wireless_thermal", false, chg_curr[i]);
			mca_rerun_election(voter[i]);
		}
	}
}

int mca_set_wls_charger_thermal_remove(bool wls_thermal_remove)
{
	struct mca_thermal_info *info = g_wlscharger_thermal_info;

	if (!info)
		return -1;

	if (wls_thermal_remove)
		info->wireless_ctrl_info.wls_thermal_remove = true;
	else
		info->wireless_ctrl_info.wls_thermal_remove = false;

	mca_wireless_charger_thermal_handle_limit(&info->wireless_ctrl_info,
			info->wireless_thermal_data, info->wireless_voter);
	mca_log_err("set wireless thermal remove %d\n", info->wireless_ctrl_info.wls_thermal_remove);

	return 0;
}
EXPORT_SYMBOL(mca_set_wls_charger_thermal_remove);

int mca_get_wls_charger_thermal_remove(bool *wls_thermal_remove)
{
	struct mca_thermal_info *info = g_wlscharger_thermal_info;

	if (!info)
		return -1;

	if (info->wireless_ctrl_info.wls_thermal_remove)
		*wls_thermal_remove = true;
	else
		*wls_thermal_remove = false;

	return 0;
}
EXPORT_SYMBOL(mca_get_wls_charger_thermal_remove);

static void mca_charger_thermal_handle_limit(struct mca_thermal_ctrl_info *ctrl,
	struct mca_thermal_data *data, struct mca_votable **voter)
{
	struct mca_thermal_info *info = g_wlscharger_thermal_info;
	int *th_data = NULL;
	int chg_curr[THERMAL_MODE_MAX] = { 0 };
	int i;
	static int thermal_removed;
	struct timespec64 ts;
	int is_zero_speed = 0;
	int thermal_level = ctrl->limit_level;
	bool cancel_iin_limit = false;

	ktime_get_boottime_ts64(&ts);
	if ((u64)ts.tv_sec < 60) {
		// get_smem_battery_info(&is_zero_speed);
		if (is_zero_speed) {
			mca_log_err("is_zero_speed = %d, ignore thermal...\n", is_zero_speed);
			return;
		}
	}

	if (!info) {
		mca_log_err("thermal info is NULL\n");
		return;
	}

	if (!ctrl->voter_ok) {
		mca_log_err("voter init is not ok\n");
		return;
	}

	if (!info->online) {
		mca_log_err("charger is not online\n");
		return;
	}

	if (thermal_level > ctrl->max_level) {
		mca_log_err("thermal level invalid\n");
		return;
	}

	if (!thermal_level) {
		chg_curr[THERMAL_MODE_BUCK_5V_ICH] = ctrl->chg_curr;
		chg_curr[THERMAL_MODE_BUCK_9V_ICH] = ctrl->chg_curr;
		chg_curr[THERMAL_MODE_DIV1_SINGLE_CURR] = ctrl->chg_curr;
		chg_curr[THERMAL_MODE_DIV1_MULTI_CURR] = ctrl->chg_curr;
		chg_curr[THERMAL_MODE_DIV2_SINGLE_CURR] = ctrl->chg_curr;
		chg_curr[THERMAL_MODE_DIV2_MULTI_CURR] = ctrl->chg_curr;
		chg_curr[THERMAL_MODE_DIV4_SINGLE_CURR] = ctrl->chg_curr;
		chg_curr[THERMAL_MODE_DIV4_MULTI_CURR] = ctrl->chg_curr;
	} else {
		th_data = &data[thermal_level - 1].buck_5v_in;
		for (i = THERMAL_MODE_BUCK_5V_ICH; i < THERMAL_MODE_MAX; i++) {
			if (!th_data[i]) {
				if (i > THERMAL_MODE_INLIMIT)
					chg_curr[i] = ctrl->chg_curr;
			} else if (!ctrl->chg_curr) {
				chg_curr[i] = th_data[i];
			} else {
				chg_curr[i] = min(th_data[i], ctrl->chg_curr);
			}
		}
	}

	if (ctrl->wired_thermal_remove) {
		if (!thermal_removed) {
			for (i = 0; i < THERMAL_MODE_MAX; i++)
				mca_vote(voter[i], "mca_thermal", false, chg_curr[i]);
			thermal_removed = true;
			mca_log_err("wired thermal_removed: %d\n", thermal_removed);
		}
	} else {
		if (thermal_removed) {
			thermal_removed = false;
			mca_log_err("wired thermal_removed: %d\n", thermal_removed);
		}

		for (i = THERMAL_MODE_BUCK_5V_ICH; i < THERMAL_MODE_MAX; i++) {
			if (chg_curr[i])
				mca_vote(voter[i], "mca_thermal", true, chg_curr[i]);
			else
				mca_vote(voter[i], "mca_thermal", false, chg_curr[i]);
		}

		switch (info->real_type) {
		case XM_CHARGER_TYPE_HVDCP3_B:
		case XM_CHARGER_TYPE_HVDCP3P5:
		case XM_CHARGER_TYPE_PPS:
		case XM_CHARGER_TYPE_PD_VERIFY:
			cancel_iin_limit = true;
			break;
		default:
			if (thermal_level == 0)
				cancel_iin_limit = true;
			else
				cancel_iin_limit = false;
			break;
		}

		if (!cancel_iin_limit) {
			th_data = &data[thermal_level - 1].buck_5v_in;
			for (i = THERMAL_MODE_BUCK_5V_IN; i <= THERMAL_MODE_INLIMIT; i++) {
				if (th_data[i])
					mca_vote(voter[i], "mca_thermal", true, th_data[i]);
				else
					mca_vote(voter[i], "mca_thermal", false, 0);
			}
		} else {
			mca_log_err("real_type = %d wired remove thermal for buck_in\n", info->real_type);
			for (i = THERMAL_MODE_BUCK_5V_IN; i <= THERMAL_MODE_INLIMIT; i++)
				mca_vote(voter[i], "mca_thermal", false, 0);
		}
	}
}

#ifdef CONFIG_SYSFS
static ssize_t mca_charger_thermal_sysfs_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t mca_charger_thermal_sysfs_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);

static struct mca_sysfs_attr_info mca_charger_thermal_sysfs_field_tbl[] = {
	mca_sysfs_attr_rw(mca_charger_thermal_sysfs, 0664, MCA_THERMAL_WIRED_CHG_CURR, wired_chg_curr),
	mca_sysfs_attr_rw(mca_charger_thermal_sysfs, 0664, MCA_THERMAL_WIRED_CHG_CURR2, wired_chg_curr2),
	mca_sysfs_attr_rw(mca_charger_thermal_sysfs, 0664, MCA_THERMAL_WIRED_CTRL_LIMIT, wired_ctrl_limit),
	mca_sysfs_attr_rw(mca_charger_thermal_sysfs, 0664, MCA_THERMAL_WIRED_THERMAL_REMOVE, wired_thermal_remove),
	mca_sysfs_attr_rw(mca_charger_thermal_sysfs, 0664, MCA_THERMAL_WIRELESS_CHG_CURR, wireless_chg_curr),
	mca_sysfs_attr_rw(mca_charger_thermal_sysfs, 0664, MCA_THERMAL_WIRELESS_CTRL_LIMIT, wireless_ctrl_limit),
	mca_sysfs_attr_rw(mca_charger_thermal_sysfs, 0664, MCA_THERMAL_WIRELESS_QUICK_CHG_CTRL_LIMIT, wls_quick_chg_control_limit),
	mca_sysfs_attr_rw(mca_charger_thermal_sysfs, 0664, MCA_THERMAL_WIRELESS_THERMAL_REMOVE, wireless_thermal_remove),
};

#define MCA_CHARGER_THERMAL_SYSFS_ATTRS_SIZE   ARRAY_SIZE(mca_charger_thermal_sysfs_field_tbl)

static struct attribute *mca_charger_thermal_sysfs_attrs[MCA_CHARGER_THERMAL_SYSFS_ATTRS_SIZE + 1];

static const struct attribute_group mca_charger_thermal_sysfs_attr_group = {
	.attrs = mca_charger_thermal_sysfs_attrs,
};

static ssize_t mca_charger_thermal_sysfs_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mca_sysfs_attr_info *attr_info;
	int len;
	struct mca_thermal_info *info = dev_get_drvdata(dev);

	attr_info = mca_sysfs_lookup_attr(attr->attr.name,
		mca_charger_thermal_sysfs_field_tbl,
		MCA_CHARGER_THERMAL_SYSFS_ATTRS_SIZE);
	if (!attr_info || !info)
		return -1;

	switch (attr_info->sysfs_attr_name) {
	case MCA_THERMAL_WIRED_CHG_CURR:
		len = snprintf(buf, PAGE_SIZE, "%d\n", info->wired_ctrl_info.chg_curr * 1000);
		break;
	case MCA_THERMAL_WIRED_CHG_CURR2:
		len = snprintf(buf, PAGE_SIZE, "%d\n", info->wired_ctrl_info.chg_curr * 1000);
		break;
	case MCA_THERMAL_WIRED_CTRL_LIMIT:
		len = snprintf(buf, PAGE_SIZE, "%d\n", info->wired_ctrl_info.limit_level);
		break;
	case MCA_THERMAL_WIRED_THERMAL_REMOVE:
		len = snprintf(buf, PAGE_SIZE, "%d\n", info->wired_ctrl_info.wired_thermal_remove);
		break;
	case MCA_THERMAL_WIRELESS_CHG_CURR:
		len = snprintf(buf, PAGE_SIZE, "%d\n", info->wireless_ctrl_info.chg_curr);
		break;
	case MCA_THERMAL_WIRELESS_CTRL_LIMIT:
		len = snprintf(buf, PAGE_SIZE, "%d\n", info->wireless_ctrl_info.limit_level);
		break;
	case MCA_THERMAL_WIRELESS_THERMAL_REMOVE:
		len = snprintf(buf, PAGE_SIZE, "%d\n", info->wireless_ctrl_info.wls_thermal_remove);
		break;
	default:
		len = 0;
		break;
	}

	return len;
}

static ssize_t mca_charger_thermal_sysfs_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mca_sysfs_attr_info *attr_info;
	struct mca_thermal_info *info = dev_get_drvdata(dev);
	const char *sic_sellect;

	attr_info = mca_sysfs_lookup_attr(attr->attr.name,
		mca_charger_thermal_sysfs_field_tbl,
		MCA_CHARGER_THERMAL_SYSFS_ATTRS_SIZE);
	if (!attr_info || !info)
		return -1;

	if (!info->batt_cell_name) {
		platform_fg_ops_get_batt_cell_info(FG_IC_MASTER, &info->batt_cell_name);
		mca_log_info("batt_cell_name: %s\n", info->batt_cell_name);
	}

	switch (attr_info->sysfs_attr_name) {
	case MCA_THERMAL_WIRED_CHG_CURR:
		sic_sellect = info->wired_sic_select[0];
		if (info->wired_sic_count > 0 && sic_sellect && info->batt_cell_name &&
			strncmp(sic_sellect, info->batt_cell_name, strlen(sic_sellect)) != 0)
			break;

		if (sscanf(buf, "%d\n", &info->wired_ctrl_info.sic_chg_curr) != 1)
			return -1;
		info->wired_ctrl_info.sic_chg_curr /= 1000;
		mca_log_info("set wired_chg_curr: %d\n", info->wired_ctrl_info.sic_chg_curr);
		mca_charger_thermal_update_chg_curr(info);
		mca_charger_thermal_handle_limit(&info->wired_ctrl_info,
			info->wired_thermal_data, info->wired_voter);
		break;
	case MCA_THERMAL_WIRED_CHG_CURR2:
		// Use WIRED_CHG_CURR as default if batt_cell_name is NULL
		sic_sellect = info->wired_sic_select[1];
		if (info->wired_sic_count <= 1 || !sic_sellect || !info->batt_cell_name ||
			strncmp(sic_sellect, info->batt_cell_name, strlen(sic_sellect)) != 0)
			break;

		if (sscanf(buf, "%d\n", &info->wired_ctrl_info.sic_chg_curr) != 1)
			return -1;
		info->wired_ctrl_info.sic_chg_curr /= 1000;
		mca_log_info("set wired_chg_curr2: %d\n", info->wired_ctrl_info.sic_chg_curr);
		mca_charger_thermal_update_chg_curr(info);
		mca_charger_thermal_handle_limit(&info->wired_ctrl_info,
			info->wired_thermal_data, info->wired_voter);
		break;
	case MCA_THERMAL_WIRED_CTRL_LIMIT:
		if (sscanf(buf, "%d\n", &info->wired_ctrl_info.limit_level) != 1)
			return -1;
		mca_log_info("set wired limit level %d\n", info->wired_ctrl_info.limit_level);
		mca_charger_thermal_handle_limit(&info->wired_ctrl_info,
			info->wired_thermal_data, info->wired_voter);
		break;
	case MCA_THERMAL_WIRED_THERMAL_REMOVE:
		if (sscanf(buf, "%d\n", &info->wired_ctrl_info.wired_thermal_remove) != 1)
			return -1;
		mca_log_err("set wired thermal remove: %d\n", info->wired_ctrl_info.wired_thermal_remove);
		mca_charger_thermal_handle_limit(&info->wired_ctrl_info,
			info->wired_thermal_data, info->wired_voter);
		break;
	case MCA_THERMAL_WIRELESS_CHG_CURR:
		if (sscanf(buf, "%d\n", &info->wireless_ctrl_info.chg_curr) != 1)
			return -1;
		info->wireless_ctrl_info.chg_curr /= 1000;
		mca_log_info("set wireless chg cur %d\n", info->wireless_ctrl_info.chg_curr);
		mca_wireless_charger_thermal_handle_limit(&info->wireless_ctrl_info,
			info->wireless_thermal_data, info->wireless_voter);
		break;
	case MCA_THERMAL_WIRELESS_CTRL_LIMIT:
		if (sscanf(buf, "%d\n", &info->wireless_ctrl_info.limit_level) != 1)
			return -1;
		mca_log_info("set wireless limit level %d\n", info->wireless_ctrl_info.limit_level);
		mca_wireless_charger_thermal_handle_limit(&info->wireless_ctrl_info,
			info->wireless_thermal_data, info->wireless_voter);
		break;
	case MCA_THERMAL_WIRELESS_QUICK_CHG_CTRL_LIMIT:
		if (sscanf(buf, "%d\n", &info->wireless_ctrl_info.quickchg_limit_level) != 1)
			return -1;
		mca_log_info("set wireless limit level %d\n", info->wireless_ctrl_info.quickchg_limit_level);
		mca_wireless_charger_thermal_handle_limit(&info->wireless_ctrl_info,
			info->wireless_thermal_data, info->wireless_voter);
		break;
	case MCA_THERMAL_WIRELESS_THERMAL_REMOVE:
		if (sscanf(buf, "%d\n", &info->wireless_ctrl_info.wls_thermal_remove) != 1)
			return -1;
		mca_log_err("set wireless thermal remove: %d\n", info->wireless_ctrl_info.wls_thermal_remove);
		mca_wireless_charger_thermal_handle_limit(&info->wireless_ctrl_info,
			info->wireless_thermal_data, info->wireless_voter);
		break;
	default:
		break;
	}

	return count;
}

static int mca_charger_thermal_create_group(struct device *dev)
{
	mca_sysfs_init_attrs(mca_charger_thermal_sysfs_attrs,
		mca_charger_thermal_sysfs_field_tbl,
		MCA_CHARGER_THERMAL_SYSFS_ATTRS_SIZE);
	return mca_sysfs_create_link_group("charger", "charger_thermal",
		dev, &mca_charger_thermal_sysfs_attr_group);
}

static void mca_charger_thermal_remove_group(struct device *dev)
{
	mca_sysfs_remove_link_group("charger", "charger_thermal",
		dev, &mca_charger_thermal_sysfs_attr_group);
}

#else
static inline int mca_charger_thermal_create_group(struct device *dev)
{
	return 0;
}

static void mca_charger_thermal_remove_group(struct device *dev)
{
}
#endif /* CONFIG_SYSFS */

static int mca_wireless_charger_thermal_parse_data(const char *name, struct device_node *node,
	struct mca_wireless_thermal_data *data, int *max_level)
{
	int len;
	int i, j;
	int idata[MAX_THERMAL_LEVEL * THERMAL_MODE_WIRELESS_MAX] = { 0 };

	len = mca_parse_dts_u32_count(node, name, MAX_THERMAL_LEVEL,
		THERMAL_MODE_WIRELESS_MAX);
	if (len < 0) {
		mca_log_err("parse %s failed", name);
		return -1;
	}

	if (mca_parse_dts_u32_array(node, name, idata, len)) {
		mca_log_err("parse %s data failed", name);
		return -1;
	}

	for (i = 0; i < len / THERMAL_MODE_WIRELESS_MAX; i++) {
		j = THERMAL_MODE_WIRELESS_MAX * i;
		data[i].wireless_bpp_in = idata[j + THERMAL_MODE_WIRELESS_BPP_IN];
		data[i].wireless_bppqc2_in = idata[j + THERMAL_MODE_WIRELESS_BPPQC2_IN];
		data[i].wireless_bppqc3_in = idata[j + THERMAL_MODE_WIRELESS_BPPQC3_IN];
		data[i].wireless_epp_in = idata[j + THERMAL_MODE_WIRELESS_EPP_IN];
		data[i].wireless_authen_20w = idata[j + THERMAL_MODE_WIRELESS_AUTHEN_20W];
		data[i].wireless_authen_30w = idata[j + THERMAL_MODE_WIRELESS_AUTHEN_30W];
		data[i].wireless_authen_50w = idata[j + THERMAL_MODE_WIRELESS_AUTHEN_50W];
		data[i].wireless_authen_80w = idata[j + THERMAL_MODE_WIRELESS_AUTHEN_80W];
		data[i].wireless_authen_voice_box = idata[j + THERMAL_MODE_WIRELESS_AUTHEN_VOICE_BOX];
		data[i].wireless_authen_magnet_30w = idata[j + THERMAL_MODE_WIRELESS_AUTHEN_MAGNET_30W];
		mca_log_debug("[%s]level-%d %d %d %d %d %d %d %d %d %d %d\n", name, i,
			data[i].wireless_bpp_in, data[i].wireless_bppqc2_in,
			data[i].wireless_bppqc3_in, data[i].wireless_epp_in,
			data[i].wireless_authen_20w, data[i].wireless_authen_30w,
			data[i].wireless_authen_50w, data[i].wireless_authen_80w,
			data[i].wireless_authen_voice_box, data[i].wireless_authen_magnet_30w);
	}

	*max_level = len / THERMAL_MODE_WIRELESS_MAX;
	return 0;
}

static int mca_charger_thermal_parse_data(const char *name, struct device_node *node,
	struct mca_thermal_data *data, int *max_level)
{
	int len;
	int i, j;
	int idata[MAX_THERMAL_LEVEL * THERMAL_MODE_MAX] = { 0 };

	len = mca_parse_dts_u32_count(node, name, MAX_THERMAL_LEVEL,
		THERMAL_MODE_MAX);
	if (len < 0) {
		mca_log_err("parse %s failed", name);
		return -1;
	}

	if (mca_parse_dts_u32_array(node, name, idata, len)) {
		mca_log_err("parse %s data failed", name);
		return -1;
	}

	for (i = 0; i < len / THERMAL_MODE_MAX; i++) {
		j = THERMAL_MODE_MAX * i;
		data[i].buck_5v_in = idata[j + THERMAL_MODE_BUCK_5V_IN];
		data[i].buck_9v_in = idata[j + THERMAL_MODE_BUCK_9V_IN];
		data[i].buck_5v_ich = idata[j + THERMAL_MODE_BUCK_5V_ICH];
		data[i].buck_9v_ich = idata[j + THERMAL_MODE_BUCK_9V_ICH];
		data[i].div1_single_curr = idata[j + THERMAL_MODE_DIV1_SINGLE_CURR];
		data[i].div1_mullti_curr = idata[j + THERMAL_MODE_DIV1_MULTI_CURR];
		data[i].div2_single_curr = idata[j + THERMAL_MODE_DIV2_SINGLE_CURR];
		data[i].div2_mullti_curr = idata[j + THERMAL_MODE_DIV2_MULTI_CURR];
		data[i].div4_single_curr = idata[j + THERMAL_MODE_DIV4_SINGLE_CURR];
		data[i].div4_mullti_curr = idata[j + THERMAL_MODE_DIV4_MULTI_CURR];
		mca_log_debug("[%s]level-%d %d %d %d %d %d %d %d %d %d %d\n", name, i,
			data[i].buck_5v_in, data[i].buck_5v_ich,
			data[i].buck_9v_in, data[i].buck_9v_ich,
			data[i].div1_single_curr, data[i].div1_single_curr,
			data[i].div2_single_curr, data[i].div2_single_curr,
			data[i].div4_single_curr, data[i].div4_single_curr);
	}

	*max_level = len / THERMAL_MODE_MAX;
	return 0;
}

static void mca_charger_thermal_parse_dt(struct mca_thermal_info *info)
{
	struct device_node *node = info->dev->of_node;

	if (!node)
		return;

	mca_parse_dts_u32(node, "support_wireless", &info->support_wireless, 0);
	mca_parse_dts_u32(node, "wireless_phone_level", &info->wireless_phone_level, 12);

	if (mca_charger_thermal_parse_data("wired_thermal", node,
		info->wired_thermal_data, &info->wired_ctrl_info.max_level)) {
		mca_log_err("parse wire thermal failed\n");
		return;
	}

	if (info->support_wireless) {
		if (mca_wireless_charger_thermal_parse_data("wireless_thermal", node,
			info->wireless_thermal_data, &info->wireless_ctrl_info.max_level)) {
			mca_log_err("parse wireless thermal failed\n");
			return;
		}
	}

	info->wired_sic_count = of_property_read_string_array(node, "wired_sic_select",
		info->wired_sic_select, BATT_CELL_SELECT_MAX);
	if (info->wired_sic_count < 0) {
		mca_log_err("get wired_sic_select count failed\n");
		return;
	}
	for (int i = 0; i < info->wired_sic_count; i++) {
		mca_log_err("wired_sic_select[%d] = %s\n", i, info->wired_sic_select[i]);
	}
}

static int mca_charger_thermal_init_wired_voter(struct mca_thermal_info *info)
{
	if (info->wired_ctrl_info.voter_ok)
		return 0;

	info->wired_voter[THERMAL_MODE_BUCK_5V_IN] = mca_find_votable("buck_5v_in");
	if (!info->wired_voter[THERMAL_MODE_BUCK_5V_IN])
		goto out;
	info->wired_voter[THERMAL_MODE_BUCK_5V_ICH] = mca_find_votable("buck_5v_ich");
	if (!info->wired_voter[THERMAL_MODE_BUCK_5V_ICH])
		goto out;
	info->wired_voter[THERMAL_MODE_BUCK_9V_IN] = mca_find_votable("buck_9v_in");
	if (!info->wired_voter[THERMAL_MODE_BUCK_9V_IN])
		goto out;
	info->wired_voter[THERMAL_MODE_BUCK_9V_ICH] = mca_find_votable("buck_9v_ich");
	if (!info->wired_voter[THERMAL_MODE_BUCK_9V_ICH])
		goto out;
	info->wired_voter[THERMAL_MODE_DIV1_SINGLE_CURR] = mca_find_votable("div1_single");
	if (!info->wired_voter[THERMAL_MODE_DIV1_SINGLE_CURR])
		goto out;
	info->wired_voter[THERMAL_MODE_DIV1_MULTI_CURR] = mca_find_votable("div1_multi");
	if (!info->wired_voter[THERMAL_MODE_DIV1_MULTI_CURR])
		goto out;
	info->wired_voter[THERMAL_MODE_DIV2_SINGLE_CURR] = mca_find_votable("div2_single");
	if (!info->wired_voter[THERMAL_MODE_DIV2_SINGLE_CURR])
		goto out;
	info->wired_voter[THERMAL_MODE_DIV2_MULTI_CURR] = mca_find_votable("div2_multi");
	if (!info->wired_voter[THERMAL_MODE_DIV2_MULTI_CURR])
		goto out;
	info->wired_voter[THERMAL_MODE_DIV4_SINGLE_CURR] = mca_find_votable("div4_single");
	if (!info->wired_voter[THERMAL_MODE_DIV4_SINGLE_CURR])
		goto out;
	info->wired_voter[THERMAL_MODE_DIV4_MULTI_CURR] = mca_find_votable("div4_multi");
	if (!info->wired_voter[THERMAL_MODE_DIV4_MULTI_CURR])
		goto out;

	info->wired_ctrl_info.voter_ok = 1;

	return 0;

out:
	mca_log_err("init wire voter failed\n");
	return -1;
}

static int mca_charger_thermal_init_wireless_voter(struct mca_thermal_info *info)
{
	if (info->wireless_ctrl_info.voter_ok)
		return 0;

	info->wireless_voter[THERMAL_MODE_WIRELESS_BPP_IN] = mca_find_votable("wireless_bpp_in");
	if (!info->wireless_voter[THERMAL_MODE_WIRELESS_BPP_IN])
		goto out;
	info->wireless_voter[THERMAL_MODE_WIRELESS_BPPQC2_IN] = mca_find_votable("wireless_bppqc2_in");
	if (!info->wireless_voter[THERMAL_MODE_WIRELESS_BPPQC2_IN])
		goto out;
	info->wireless_voter[THERMAL_MODE_WIRELESS_BPPQC3_IN] = mca_find_votable("wireless_bppqc3_in");
	if (!info->wireless_voter[THERMAL_MODE_WIRELESS_BPPQC3_IN])
		goto out;
	info->wireless_voter[THERMAL_MODE_WIRELESS_EPP_IN] = mca_find_votable("wireless_epp_in");
	if (!info->wireless_voter[THERMAL_MODE_WIRELESS_EPP_IN])
		goto out;
	info->wireless_voter[THERMAL_MODE_WIRELESS_AUTHEN_20W] = mca_find_votable("wireless_auth_20w");
	if (!info->wireless_voter[THERMAL_MODE_WIRELESS_AUTHEN_20W])
		goto out;
	info->wireless_voter[THERMAL_MODE_WIRELESS_AUTHEN_30W] = mca_find_votable("wireless_auth_30w");
	if (!info->wireless_voter[THERMAL_MODE_WIRELESS_AUTHEN_30W])
		goto out;
	info->wireless_voter[THERMAL_MODE_WIRELESS_AUTHEN_50W] = mca_find_votable("wireless_auth_50w");
	if (!info->wireless_voter[THERMAL_MODE_WIRELESS_AUTHEN_50W])
		goto out;
	info->wireless_voter[THERMAL_MODE_WIRELESS_AUTHEN_80W] = mca_find_votable("wireless_auth_80w");
	if (!info->wireless_voter[THERMAL_MODE_WIRELESS_AUTHEN_80W])
		goto out;
	info->wireless_voter[THERMAL_MODE_WIRELESS_AUTHEN_VOICE_BOX] = mca_find_votable("wireless_auth_voice_box");
	if (!info->wireless_voter[THERMAL_MODE_WIRELESS_AUTHEN_VOICE_BOX])
		goto out;
	info->wireless_voter[THERMAL_MODE_WIRELESS_AUTHEN_MAGNET_30W] = mca_find_votable("wireless_auth_magnet_30w");
	if (!info->wireless_voter[THERMAL_MODE_WIRELESS_AUTHEN_MAGNET_30W])
		goto out;

	info->wireless_ctrl_info.voter_ok = 1;

	return 0;

out:
	mca_log_err("init wireless voter failed\n");
	return -1;
}


static void mca_charger_thermal_init_voter_work(struct work_struct *work)
{
	struct mca_thermal_info *info = container_of(work, struct mca_thermal_info, init_voter_work.work);
	int ret = 0;

	if (!info->wired_ctrl_info.voter_ok) {
		ret = mca_charger_thermal_init_wired_voter(info);
		if (!ret)
			mca_charger_thermal_handle_limit(&info->wired_ctrl_info,
				info->wired_thermal_data, info->wired_voter);
	}

	if (info->support_wireless) {
		if (info->wireless_ctrl_info.voter_ok)
			return;
		ret |= mca_charger_thermal_init_wireless_voter(info);
		if (!ret)
			mca_wireless_charger_thermal_handle_limit(&info->wireless_ctrl_info,
				info->wireless_thermal_data, info->wireless_voter);
	}

	if (ret)
		schedule_delayed_work(&info->init_voter_work, msecs_to_jiffies(1000));
}

static int mca_charger_thermal_get_max_level(struct thermal_cooling_device *tcd,
	unsigned long *state)
{
	struct mca_thermal_info *info = tcd->devdata;

	if (!info)
		return -1;

	*state = info->wired_ctrl_info.max_level + 1;

	return 0;
}

static int mca_charger_thermal_get_cur_level(struct thermal_cooling_device *tcd,
	unsigned long *state)
{
	struct mca_thermal_info *info = tcd->devdata;

	if (!info)
		return -1;

	*state = info->wired_ctrl_info.limit_level;

	return 0;
}

static int mca_charger_thermal_set_cur_level(struct thermal_cooling_device *tcd,
	unsigned long state)
{
	struct mca_thermal_info *info = tcd->devdata;

	if (!info)
		return -1;

	info->wired_ctrl_info.limit_level = (int)state;


	mca_charger_thermal_handle_limit(&info->wired_ctrl_info,
			info->wired_thermal_data, info->wired_voter);

	mca_log_info("set wired level %lu\n", state);

	return 0;
}

static const struct thermal_cooling_device_ops g_mca_charger_tcd_ops = {
	.get_max_state = mca_charger_thermal_get_max_level,
	.get_cur_state = mca_charger_thermal_get_cur_level,
	.set_cur_state = mca_charger_thermal_set_cur_level,
};

static int mca_charger_thermal_wls_super_sts_callback(void *data, int effective_result)
{
	struct mca_thermal_info *info = (struct mca_thermal_info *)data;

	if (!data)
		return -1;

	info->smartchg_data.wls_super_sts = effective_result;
	mca_wireless_charger_thermal_handle_limit(&info->wireless_ctrl_info,
			info->wireless_thermal_data, info->wireless_voter);
	mca_log_info("effective_result: %d\n", effective_result);

	return 0;
}

static struct mca_smartchg_if_ops g_thermal_smartchg_if_ops = {
	.type = MCA_SMARTCHG_IF_CHG_TYPE_THERMAL,
	.data = NULL,
	.set_wls_super_sts = mca_charger_thermal_wls_super_sts_callback,
};

static void mca_charger_thermal_update_chg_curr(struct mca_thermal_info *info)
{
	if (!info->wired_ctrl_info.sic_chg_curr && !info->wired_ctrl_info.sic_init_fcc) {
		mca_log_info("invalid chg_curr\n");
		return;
	}

	if (!info->wired_ctrl_info.sic_chg_curr)
		info->wired_ctrl_info.chg_curr = info->wired_ctrl_info.sic_init_fcc;
	else if (!info->wired_ctrl_info.sic_init_fcc)
		info->wired_ctrl_info.chg_curr = info->wired_ctrl_info.sic_chg_curr;
	else
		info->wired_ctrl_info.chg_curr = min(info->wired_ctrl_info.sic_init_fcc, info->wired_ctrl_info.sic_chg_curr);
	mca_log_info("chg_curr = %d, sic_init_fcc = %d sic_chg_curr = %d \n",
		info->wired_ctrl_info.chg_curr, info->wired_ctrl_info.sic_init_fcc, info->wired_ctrl_info.sic_chg_curr);
	return;
}

#define QC_27W_SIC_INIT_FCC 6000
static void mca_charger_thermal_sic_initial_chg_curr(struct mca_thermal_info *info)
{
	int power_max = 0;

	if (info->real_type == XM_CHARGER_TYPE_PPS ||
		info->real_type == XM_CHARGER_TYPE_PD_VERIFY) {
		protocol_class_get_adapter_max_power(ADAPTER_PROTOCOL_PPS, (unsigned int *)(&power_max));
		for (int i = 0; i < POWER_MAX_LIST; i++) {
			if (power_max <= sic_init_list[i].power_max) {
				info->wired_ctrl_info.sic_init_fcc = sic_init_list[i].sic_init_fcc;
				mca_charger_thermal_update_chg_curr(info);
				break;
			}
		}

		if (info->online)
			mca_charger_thermal_handle_limit(&info->wired_ctrl_info,
				info->wired_thermal_data, info->wired_voter);

	} else if (info->real_type == XM_CHARGER_TYPE_HVDCP3P5 ||
			info->real_type == XM_CHARGER_TYPE_HVDCP3_B ||
			info->real_type == XM_CHARGER_TYPE_HVDCP3) {

			info->wired_ctrl_info.sic_init_fcc = QC_27W_SIC_INIT_FCC;
			mca_charger_thermal_update_chg_curr(info);
			if (info->online)
				mca_charger_thermal_handle_limit(&info->wired_ctrl_info,
					info->wired_thermal_data, info->wired_voter);
			mca_log_info("QC3.0 sic_init_fcc %d\n", info->wired_ctrl_info.sic_init_fcc);
	} else {
		mca_log_info("not support type %d\n", info->real_type);
	}

	return;
}

static void mca_charger_thermal_poweroff_clear_voter(struct mca_thermal_info *info)
{
	int i;

	mca_log_info("usb disconnect, clear voter\n");
	if (info->online)
		return;

	info->wired_ctrl_info.chg_curr = 0;
	info->wired_ctrl_info.sic_chg_curr = 0;
	info->wired_ctrl_info.sic_init_fcc = 0;
	info->real_type = XM_CHARGER_TYPE_UNKNOW;

	for (i = 0; i < THERMAL_MODE_MAX; i++)
		mca_vote(info->wired_voter[i], "mca_thermal", false, 0);
}

static void mca_wireless_charger_thermal_poweroff_clear_voter(struct mca_thermal_info *info)
{
	int i;

	mca_log_info("wireless disconnect, clear voter\n");
	if (info->wls_online)
		return;

	for (i = 0; i < THERMAL_MODE_WIRELESS_MAX; i++)
		mca_vote(info->wireless_voter[i], "mca_wireless_thermal", false, 0);
}

static int mca_charger_thermal_process_event(int event, int value, void *data)
{
	struct mca_thermal_info *info = data;

	if (!data)
		return -1;

	mca_log_info("receive event %d, value %d\n", event, value);
	switch (event) {
	case MCA_EVENT_USB_CONNECT:
	case MCA_EVENT_USB_DISCONNECT:
		info->online = value;
		if (info->online)
			mca_charger_thermal_handle_limit(&info->wired_ctrl_info,
				info->wired_thermal_data, info->wired_voter);
		else
			mca_charger_thermal_poweroff_clear_voter(info);
		break;
	case MCA_EVENT_CHARGE_TYPE_CHANGE:
		info->real_type = value;
		mca_charger_thermal_sic_initial_chg_curr(info);
		break;
	case MCA_EVENT_WIRELESS_CONNECT:
	case MCA_EVENT_WIRELESS_DISCONNECT:
		info->wls_online = value;
		if (!info->wls_online)
			mca_wireless_charger_thermal_poweroff_clear_voter(info);
		break;
	case MCA_EVENT_WIRELESS_EPP_MODE:
		if (info->wls_online)
			mca_wireless_charger_thermal_handle_limit(&info->wireless_ctrl_info,
				info->wireless_thermal_data, info->wireless_voter);
		break;
	default:
		break;
	}

	return 0;
}

static int mca_charge_thermal_dump_log_head(void *data, char *buf, int size)
{
	return snprintf(buf, size, "W_TL W_SIC   WL_TL WL_STL WL_SIC   ");
}

static int mca_charge_thermal_dump_log_context(void *data, char *buf, int size)
{
	struct mca_thermal_info *info = (struct mca_thermal_info *)data;

	if (!data)
		return snprintf(buf, size, "%-5d%-8d%-6d%-7d%-9d", -1, -1, -1, -1, -1);

	return snprintf(buf, size, "%-5d%-8d%-6d%-7d%-9d",
		info->wired_ctrl_info.limit_level, info->wired_ctrl_info.chg_curr,
		info->wireless_ctrl_info.limit_level, info->wireless_ctrl_info.quickchg_limit_level,
		info->wireless_ctrl_info.chg_curr);
}

static struct mca_log_charge_log_ops g_mca_thermal_log_ops = {
	.dump_log_head = mca_charge_thermal_dump_log_head,
	.dump_log_context = mca_charge_thermal_dump_log_context,
};

static int mca_charger_thermal_probe(struct platform_device *pdev)
{
	struct mca_thermal_info *info;
	int ret;
	int online = 0;

	mca_log_info("probe begin\n");
	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		mca_log_err("out of memory\n");
		return -ENOMEM;
	}

	info->dev = &pdev->dev;
	platform_set_drvdata(pdev, info);
	mca_charger_thermal_parse_dt(info);
	INIT_DELAYED_WORK(&info->init_voter_work, mca_charger_thermal_init_voter_work);
	mca_charger_thermal_create_group(info->dev);

	(void)mca_strategy_ops_register(STRATEGY_FUNC_TYPE_THERMAL,
		mca_charger_thermal_process_event, NULL, NULL, info);

	ret = mca_charger_thermal_init_wired_voter(info);
	if (info->support_wireless)
		ret |= mca_charger_thermal_init_wireless_voter(info);
	if (ret)
		schedule_delayed_work(&info->init_voter_work, msecs_to_jiffies(1000));

	info->tcd = devm_thermal_of_cooling_device_register(info->dev, info->dev->of_node, "battery",
		info, &g_mca_charger_tcd_ops);
	if (IS_ERR_OR_NULL(info->tcd)) {
		mca_log_err("register cooling device failed\n");
		return -1;
	}
	g_thermal_smartchg_if_ops.data = info;
	(void)mca_smartchg_if_ops_register(&g_thermal_smartchg_if_ops);
	g_wlscharger_thermal_info = info;
	mca_log_charge_log_register(MCA_CHARGE_LOG_ID_THERMAL,
		&g_mca_thermal_log_ops, info);

	platform_class_buckchg_ops_get_online(MAIN_BUCK_CHARGER, &online);
	if (online) {
		mca_log_info("avoid missing first usb connect event\n");
		mca_event_block_notify(MCA_EVENT_TYPE_CHARGER_CONNECT, MCA_EVENT_USB_CONNECT, NULL);
	}
	mca_log_err("probe end\n");

	return 0;
}

static int mca_charger_thermal_remove(struct platform_device *pdev)
{
	mca_charger_thermal_remove_group(&pdev->dev);
	return 0;
}

static const struct of_device_id match_table[] = {
	{.compatible = "mca_charger_thermal"},
	{},
};

static struct platform_driver mca_charger_thermal_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "mca_charger_thermal",
		.of_match_table = match_table,
	},
	.probe = mca_charger_thermal_probe,
	.remove = mca_charger_thermal_remove,
};

static int __init mca_charger_thermal_init(void)
{
	return platform_driver_register(&mca_charger_thermal_driver);
}
module_init(mca_charger_thermal_init);

static void __exit mca_charger_thermal_exit(void)
{
	platform_driver_unregister(&mca_charger_thermal_driver);
}
module_exit(mca_charger_thermal_exit);

MODULE_DESCRIPTION("charge thermal");
MODULE_AUTHOR("liyuze1@xiaomi.com");
MODULE_LICENSE("GPL v2");

