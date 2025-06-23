// SPDX-License-Identifier: GPL-2.0
/*
 * mca_battery_feature.c
 *
 * mca battery feature driver
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
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/kthread.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>
#include <mca/common/mca_log.h>
#include <mca/common/mca_event.h>
#include <mca/common/mca_parse_dts.h>
#include <mca/strategy/strategy_fg_class.h>
#include "inc/strategy_fg.h"

#ifndef MCA_LOG_TAG
#define MCA_LOG_TAG "mca_battery_shutdown"
#endif

#define MCA_BATTERY_SHUTDOWN_VCUTTOFF_FW 3050
#define	MCA_BATTERY_SHUTDOWN_VCUTTOFF_SHUTDOWN_DELAY 3050
#define	MCA_BATTERY_SHUTDOWN_VCUTTOFF_SW 2900
#define MCA_BATTERY_SHUTDOWN_VCUTTOFF_MIN 2500
#define MCA_BATTERY_SHUTDOWN_VOLTAGE_HYST 50
#define MCA_HEAVY_LOAD_DISCHARGING_CURRENT 2000000
#define MCA_HEAVY_LOAD_DISCHARGING_COUNT 2

enum shutdown_strategy_type {
	SHUTDOWN_STRATEGY_BATTERY_HEALTH,
	SHUTDOWN_STRATEGY_BATTERY_SPEC,
	SHUTDOWN_STRATEGY_MAX,
};

static int mca_battery_shutdown_parse_vcutoff_para(struct device_node *node,
	const char *name, struct mca_battery_shutdown_vcutoff_para_info *vcutoff_info)
{
	struct mca_battery_shutdown_vcutoff_para *vcutoff_para;
	int array_len, row, col, i;
	const char *tmp_string = NULL;

	if (strcmp(name, "null") == 0) {
		mca_log_info("no need parse volt para\n");
		return 0;
	}
	array_len = mca_parse_dts_count_strings(node, name,
		VCUTOFF_PARA_MAX_GROUP,
		MCA_BATTERY_SHUTDOWN_VCUTOFF_PARA_MAX);
	if (array_len < 0) {
		mca_log_err("parse %s failed\n", name);
		return -1;
	}

	vcutoff_info->vcutoff_para_size = array_len / MCA_BATTERY_SHUTDOWN_VCUTOFF_PARA_MAX;
	vcutoff_info->vcutoff_para = kcalloc(vcutoff_info->vcutoff_para_size, sizeof(*vcutoff_para), GFP_KERNEL);
	if (!vcutoff_info->vcutoff_para) {
		mca_log_err("vcutoff para no mem\n");
		return -ENOMEM;
	}
	vcutoff_para = vcutoff_info->vcutoff_para;
	for (i = 0; i < array_len; i++) {
		if (mca_parse_dts_string_index(node, name, i, &tmp_string))
			return -1;

		row = i / MCA_BATTERY_SHUTDOWN_VCUTOFF_PARA_MAX;
		col = i % MCA_BATTERY_SHUTDOWN_VCUTOFF_PARA_MAX;

		switch (col) {
		case MCA_BATTERY_SHUTDOWN_RANGE_UPPER:
			if (kstrtoint(tmp_string, 10, &vcutoff_para[row].range_upper))
				goto error;
			break;
		case MCA_BATTERY_SHUTDOWN_VCUTOFF_FOR_FW:
			if (kstrtoint(tmp_string, 10, &vcutoff_para[row].vcutoff_fw))
				goto error;
			break;
		case MCA_BATTERY_SHUTDOWN_VCUTOFF_FOR_SHUTDOWN_DELAY:
			if (kstrtoint(tmp_string, 10, &vcutoff_para[row].vcutoff_shutdown_delay))
				goto error;
			break;
		case MCA_BATTERY_SHUTDOWN_VCUTOFF_FOR_SW:
			if (kstrtoint(tmp_string, 10, &vcutoff_para[row].vcutoff_sw))
				goto error;
			break;
		default:
			break;
		}
	}

	return 0;
error:
	kfree(vcutoff_para);
	vcutoff_para = NULL;
	return -1;
}

int mca_battery_shutdown_parse_dt(void *data)
{
	struct strategy_fg *fg = (struct strategy_fg *)data;
	struct device_node *node = fg->dev->of_node;
	int rc;
	struct mca_battery_shutdown_para_info *batt_spec_para_info = &(fg->batt_spec_para);
	struct mca_battery_shutdown_para_info *batt_health_para_info = &(fg->batt_health_para);
	int array_len, row, col;
	const char *tmp_string = NULL;
	struct mca_battery_shutdown_temp_para_info *temp_info;

	rc = mca_parse_dts_u32(node, "vcutoff_shutdown_delay",
		&fg->vcutoff_shutdown_delay, MCA_BATTERY_SHUTDOWN_VCUTTOFF_SHUTDOWN_DELAY);
	rc |= mca_parse_dts_u32(node, "vcutoff_sw",
		&fg->vcutoff_sw, MCA_BATTERY_SHUTDOWN_VCUTTOFF_SW);
	rc |= mca_parse_dts_u32(node, "vcutoff_fw",
		&fg->vcutoff_fw, MCA_BATTERY_SHUTDOWN_VCUTTOFF_FW);
	if (rc) {
		mca_log_err(" normal vcutoff parse failed ret=%d\n", rc);
		return rc;
	}

	fg->support_cc_vcutoff = of_property_read_bool(node, "support-cc-vcutoff");
	if (fg->support_cc_vcutoff) {
		array_len = mca_parse_dts_count_strings(node, "cc_vcutoff_cfg",
			VCUTOFF_PARA_MAX_GROUP,
			MCA_BATTERY_SHUTDOWN_TEMP_PARA_MAX);
		if (array_len < 0) {
			mca_log_err("parse cc_vcutoff_cfg failed\n");
			return -1;
		}

		batt_health_para_info->temp_para_size = array_len / MCA_BATTERY_SHUTDOWN_TEMP_PARA_MAX;
		batt_health_para_info->temp_info = kcalloc(batt_health_para_info->temp_para_size, sizeof(*temp_info), GFP_KERNEL);
		if (!batt_health_para_info->temp_info) {
			mca_log_err("vcutoff_para_info->temp_para no mem\n");
			return -ENOMEM;
		}

		temp_info = batt_health_para_info->temp_info;
		for (int i = 0; i < array_len; i++) {
			if (mca_parse_dts_string_index(node, "cc_vcutoff_cfg", i, &tmp_string))
				return -1;
			row = i / MCA_BATTERY_SHUTDOWN_TEMP_PARA_MAX;
			col = i % MCA_BATTERY_SHUTDOWN_TEMP_PARA_MAX;
			switch (col) {
			case MCA_BATTERY_SHUTDOWN_TEMP_LOW:
				if (kstrtoint(tmp_string, 10, &temp_info[row].temp_para.temp_low))
					goto error;
				break;
			case MCA_BATTERY_SHUTDOWN_TEMP_HIGH:
				if (kstrtoint(tmp_string, 10, &temp_info[row].temp_para.temp_high))
					goto error;
				break;
			case MCA_BATTERY_SHUTDOWN_LOW_TEMP_HYS:
				if (kstrtoint(tmp_string, 10, &temp_info[row].temp_para.low_temp_hysteresis))
					goto error;
				break;
			case MCA_BATTERY_SHUTDOWN_HIGH_TEMP_HYS:
				if (kstrtoint(tmp_string, 10, &temp_info[row].temp_para.high_temp_hysteresis))
					goto error;
				break;
			case MCA_BATTERY_SHUTDOWN_VCUTOFF_PARA_NAME:
				if (mca_battery_shutdown_parse_vcutoff_para(node, tmp_string,
					&temp_info[row].vcutoff_info))
					goto error;
				break;
			default:
				break;
			}
		}

	}

	fg->support_dod_vcutoff = of_property_read_bool(node, "support-dod-vcutoff");
	if (fg->support_dod_vcutoff) {
		array_len = mca_parse_dts_count_strings(node, "dod_vcutoff_cfg",
			BATTERY_TEMP_PARA_MAX_GROUP,
			MCA_BATTERY_SHUTDOWN_TEMP_PARA_MAX);
		if (array_len < 0) {
			mca_log_err("parse dod_vcutoff_cfg failed\n");
			return -1;
		}
		batt_spec_para_info->temp_para_size = array_len / MCA_BATTERY_SHUTDOWN_TEMP_PARA_MAX;
		batt_spec_para_info->temp_info = kcalloc(batt_spec_para_info->temp_para_size, sizeof(*temp_info), GFP_KERNEL);
		if (!batt_spec_para_info->temp_info) {
			mca_log_err("batt_para_info->temp_para no mem\n");
			return -ENOMEM;
		}

		temp_info = batt_spec_para_info->temp_info;
		for (int i = 0; i < array_len; i++) {
			if (mca_parse_dts_string_index(node, "dod_vcutoff_cfg", i, &tmp_string))
				return -1;

			row = i / MCA_BATTERY_SHUTDOWN_TEMP_PARA_MAX;
			col = i % MCA_BATTERY_SHUTDOWN_TEMP_PARA_MAX;
			switch (col) {
			case MCA_BATTERY_SHUTDOWN_TEMP_LOW:
				if (kstrtoint(tmp_string, 10, &temp_info[row].temp_para.temp_low))
					goto error;
				break;
			case MCA_BATTERY_SHUTDOWN_TEMP_HIGH:
				if (kstrtoint(tmp_string, 10, &temp_info[row].temp_para.temp_high))
					goto error;
				break;
			case MCA_BATTERY_SHUTDOWN_LOW_TEMP_HYS:
				if (kstrtoint(tmp_string, 10, &temp_info[row].temp_para.low_temp_hysteresis))
					goto error;
				break;
			case MCA_BATTERY_SHUTDOWN_HIGH_TEMP_HYS:
				if (kstrtoint(tmp_string, 10, &temp_info[row].temp_para.high_temp_hysteresis))
					goto error;
				break;
			case MCA_BATTERY_SHUTDOWN_VCUTOFF_PARA_NAME:
				if (mca_battery_shutdown_parse_vcutoff_para(node, tmp_string,
					&temp_info[row].vcutoff_info))
					goto error;
				break;
			default:
				break;
			}
		}

		mca_parse_dts_u32_array(node, "weight", fg->weight, WEIGHT_LEVEL_MAX);

	}

	fg->support_vpack_low_shutdown = of_property_read_bool(node, "support-vpack-low-shutdown");
	return 0;

error:
	kfree(temp_info);
	temp_info = NULL;
	return -1;
}

#define ROLLBACK_SHUTDOWN_VOLT_MAX 3000
int mca_battery_shutdown_update_vcutoff_para(void *data)
{
	struct strategy_fg *fg = (struct strategy_fg *)data;
	int vcutoff_fw[SHUTDOWN_STRATEGY_MAX] = {0};
	int vcutoff_shutdown_delay[SHUTDOWN_STRATEGY_MAX] = {0};
	int vcutoff_sw[SHUTDOWN_STRATEGY_MAX] = {0};
	int vcutoff_shutdown_volt = 0, index = 0;
	int temp = fg->batt_temperature / 10;

	if (!fg->support_cc_vcutoff && !fg->support_dod_vcutoff)
		return -1;

	if (fg->support_cc_vcutoff) {
		int cur_temp_index = -1;
		int cur_vcuttoff_index = -1;
		struct mca_battery_shutdown_vcutoff_para_info *vcutoff_info;
		struct mca_battery_shutdown_temp_para temp_para;

		for (int i = 0; i < fg->batt_health_para.temp_para_size; i++) {
			if (temp < fg->batt_health_para.temp_info[i].temp_para.temp_high &&
				temp >= fg->batt_health_para.temp_info[i].temp_para.temp_low) {
				cur_temp_index = i;
				break;
			}
		}

		if (cur_temp_index < 0 || cur_temp_index >= fg->batt_health_para.temp_para_size) {
			mca_log_err("temp over range invalid\n");
			return -1;
		}

		temp_para = fg->batt_health_para.temp_info[fg->batt_health_para.cur_temp_index].temp_para;
		if (cur_temp_index > fg->batt_health_para.cur_temp_index) {
			if (temp >= temp_para.temp_high + temp_para.high_temp_hysteresis)
				fg->batt_health_para.cur_temp_index = cur_temp_index;
		} else if (cur_temp_index < fg->batt_health_para.cur_temp_index) {
			if (temp < temp_para.temp_low - temp_para.low_temp_hysteresis)
				fg->batt_health_para.cur_temp_index = cur_temp_index;
		}

		vcutoff_info = &(fg->batt_health_para.temp_info[fg->batt_health_para.cur_temp_index].vcutoff_info);

		for (int i = 0; i < vcutoff_info->vcutoff_para_size; i++) {
			if (fg->batt_cyclecount < vcutoff_info->vcutoff_para[i].range_upper) {
				cur_vcuttoff_index = i;
				break;
			}
		}

		if (cur_vcuttoff_index < 0 || cur_vcuttoff_index > vcutoff_info->vcutoff_para_size) {
			mca_log_err("cyclecount over range invalid\n");
			return -1;
		}

		vcutoff_fw[SHUTDOWN_STRATEGY_BATTERY_HEALTH] =
			vcutoff_info->vcutoff_para[cur_vcuttoff_index].vcutoff_fw;
		vcutoff_shutdown_delay[SHUTDOWN_STRATEGY_BATTERY_HEALTH] =
			vcutoff_info->vcutoff_para[cur_vcuttoff_index].vcutoff_shutdown_delay;
		vcutoff_sw[SHUTDOWN_STRATEGY_BATTERY_HEALTH] =
			vcutoff_info->vcutoff_para[cur_vcuttoff_index].vcutoff_sw;

		mca_log_info("BatteryHealth:cur_temp_index[%d], last_temp_index[%d] cur_vcuttoff_index[%d]\n",
			cur_temp_index, fg->batt_health_para.cur_temp_index, cur_vcuttoff_index);

	}

	if (fg->support_dod_vcutoff) {
		int cur_temp_index = -1;
		int cur_vcuttoff_index = -1;
		struct mca_battery_shutdown_vcutoff_para_info *vcutoff_info;
		struct mca_battery_shutdown_temp_para temp_para;

		for (int i = 0; i < fg->batt_spec_para.temp_para_size; i++) {
			if (temp < fg->batt_spec_para.temp_info[i].temp_para.temp_high &&
				temp >= fg->batt_spec_para.temp_info[i].temp_para.temp_low) {
				cur_temp_index = i;
				break;
			}
		}

		if (cur_temp_index < 0 || cur_temp_index >= fg->batt_spec_para.temp_para_size) {
			mca_log_err("temp over range invalid\n");
			return -1;
		}

		temp_para = fg->batt_spec_para.temp_info[fg->batt_spec_para.cur_temp_index].temp_para;
		if (cur_temp_index > fg->batt_spec_para.cur_temp_index) {
			if (temp >= temp_para.temp_high + temp_para.high_temp_hysteresis)
				fg->batt_spec_para.cur_temp_index = cur_temp_index;
		} else if (cur_temp_index < fg->batt_spec_para.cur_temp_index) {
			if (temp < temp_para.temp_low - temp_para.low_temp_hysteresis)
				fg->batt_spec_para.cur_temp_index = cur_temp_index;
		}

		vcutoff_info = &(fg->batt_spec_para.temp_info[fg->batt_spec_para.cur_temp_index].vcutoff_info);
		for (int i = 0; i < vcutoff_info->vcutoff_para_size; i++) {
			if (fg->dod_count <= vcutoff_info->vcutoff_para[i].range_upper) {
				cur_vcuttoff_index = i;
				break;
			}
		}

		if (cur_vcuttoff_index < 0 || cur_vcuttoff_index > vcutoff_info->vcutoff_para_size) {
			mca_log_err("dod_count over range invalid\n");
			return -1;
		}

		vcutoff_fw[SHUTDOWN_STRATEGY_BATTERY_SPEC] =
			vcutoff_info->vcutoff_para[cur_vcuttoff_index].vcutoff_fw;
		vcutoff_shutdown_delay[SHUTDOWN_STRATEGY_BATTERY_SPEC] =
			vcutoff_info->vcutoff_para[cur_vcuttoff_index].vcutoff_shutdown_delay;
		vcutoff_sw[SHUTDOWN_STRATEGY_BATTERY_SPEC] =
			vcutoff_info->vcutoff_para[cur_vcuttoff_index].vcutoff_sw;

		mca_log_info("BatterySpec:cur_temp_index[%d], last_temp_index[%d] cur_vcuttoff_index[%d]\n",
			cur_temp_index, fg->batt_spec_para.cur_temp_index, cur_vcuttoff_index);
	}


	for (int i = 0; i < SHUTDOWN_STRATEGY_MAX; i++) {
		if (vcutoff_shutdown_volt < vcutoff_shutdown_delay[i]) {
			vcutoff_shutdown_volt = vcutoff_shutdown_delay[i];
			index = i;
		}
		mca_log_info("Max-Strategy[%d], vcutoff_fw %d, vcutoff_shutdown_delay %d vcutoff_sw %d\n",
		i, vcutoff_fw[i], vcutoff_shutdown_delay[i], vcutoff_sw[i]);
	}


	if (index  < 0 || index >= SHUTDOWN_STRATEGY_MAX) {
		mca_log_info("Strategy Optimla Select Invalid\n");
		return 0;
	}

	if (fg->enable_rollback) {
		if (vcutoff_shutdown_delay[index] < ROLLBACK_SHUTDOWN_VOLT_MAX) {
			fg->vcutoff_fw = MCA_BATTERY_SHUTDOWN_VCUTTOFF_FW;
			fg->vcutoff_shutdown_delay = MCA_BATTERY_SHUTDOWN_VCUTTOFF_SHUTDOWN_DELAY;
			fg->vcutoff_sw = MCA_BATTERY_SHUTDOWN_VCUTTOFF_SW;
			mca_log_info("[Enable_Rollback][%d] vcutoff_fw[%d], vcutoff_shutdown_delay[%d], vcutoff_sw[%d]\n",
				index, fg->vcutoff_fw, fg->vcutoff_shutdown_delay, fg->vcutoff_sw);
			return 0;
		}
	}

	fg->vcutoff_fw = vcutoff_fw[index] > MCA_BATTERY_SHUTDOWN_VCUTTOFF_MIN ?
		vcutoff_fw[index] : MCA_BATTERY_SHUTDOWN_VCUTTOFF_FW;
	fg->vcutoff_shutdown_delay = vcutoff_shutdown_delay[index] > MCA_BATTERY_SHUTDOWN_VCUTTOFF_MIN ?
		vcutoff_shutdown_delay[index] : MCA_BATTERY_SHUTDOWN_VCUTTOFF_SHUTDOWN_DELAY;
	fg->vcutoff_sw = vcutoff_sw[index] > MCA_BATTERY_SHUTDOWN_VCUTTOFF_MIN ?
		vcutoff_sw[index] : MCA_BATTERY_SHUTDOWN_VCUTTOFF_SW;
	mca_log_info("Strategy[%d]:vcutoff_fw[%d], vcutoff_shutdown_delay[%d], vcutoff_sw[%d]\n",
		index, fg->vcutoff_fw, fg->vcutoff_shutdown_delay, fg->vcutoff_sw);
	return 0;
}

static bool mca_battery_shutdown_heavy_load_check(void *data)
{
	struct strategy_fg *fg = (struct strategy_fg *)data;
	static int heavy_load_count;

	if (fg->batt_voltage_mean < (fg->vcutoff_shutdown_delay + MCA_BATTERY_SHUTDOWN_VOLTAGE_HYST) &&
		fg->batt_current > MCA_HEAVY_LOAD_DISCHARGING_CURRENT)
		heavy_load_count++;
	else
		heavy_load_count = 0;

	mca_log_info("heavy_load_count: %d\n", heavy_load_count);

	if (heavy_load_count > MCA_HEAVY_LOAD_DISCHARGING_COUNT)
		return true;
	else
		return false;
}

static bool mca_shutdown_due_to_vpack_low_of_vpack_inside_fg_absent(void *data)
{
	struct strategy_fg *fg = (struct strategy_fg *)data;
	int vpack_vol_now_mv = 0, chip_ok = 0;
	static int count = 0;

#ifdef CONFIG_FACTORY_BUILD
	return false;
#endif

	if (!fg->support_vpack_low_shutdown)
		return false;

	if (fg->cfg.fg_type == MCA_FG_TYPE_SINGLE_SERIES)
		vpack_vol_now_mv = fg->pvbat * 2;
	else
		vpack_vol_now_mv = fg->pvbat;

	chip_ok = strategy_class_fg_is_chip_ok();
	mca_log_info("chip_ok:%d vpack_vol_now_mv:%d fg->vcutoff_sw:%d",
			chip_ok, vpack_vol_now_mv, fg->vcutoff_sw);
	if (!chip_ok && count < 10 && vpack_vol_now_mv <= fg->vcutoff_sw + 100) {
		count++;
	} else {
		count = 0;
	}

	if (count >= 10)
		return true;
	else
		return false;
}

int mca_battery_shutdown_check_shutdown_delay(void *data, int *ui_soc)
{
	struct strategy_fg *fg = (struct strategy_fg *)data;
	int soc = *ui_soc;
	int charging_status = POWER_SUPPLY_STATUS_UNKNOWN;
	static bool shutdown_dealy, last_shutdown_delay;
	static bool shutdown_delay_cancel;
	int len;
	char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
	union power_supply_propval pval = { 0, };
	struct mca_event_notify_data n_data = { 0 };
	bool heavy_load_off = false;

	if (fg->batt_psy) {
		power_supply_get_property(fg->batt_psy, POWER_SUPPLY_PROP_STATUS, &pval);
		charging_status = pval.intval;
	}

	if (mca_shutdown_due_to_vpack_low_of_vpack_inside_fg_absent(fg)
		&& charging_status != POWER_SUPPLY_STATUS_CHARGING) {
		shutdown_dealy = true;
		shutdown_delay_cancel = false;
		soc = 1;
		goto report_shutdown;
	}

	if (soc <= 1) {
		heavy_load_off = mca_battery_shutdown_heavy_load_check(fg);

		if ((fg->batt_voltage_mean <= fg->vcutoff_shutdown_delay || heavy_load_off) &&
			charging_status != POWER_SUPPLY_STATUS_CHARGING) {
			shutdown_dealy = true;
			shutdown_delay_cancel = false;
			soc = 1;
		} else if (charging_status == POWER_SUPPLY_STATUS_CHARGING) {
			if (shutdown_dealy) {
				shutdown_dealy = false;
				shutdown_delay_cancel = true;
			}
			soc = 1;
		} else {
			shutdown_dealy = false;
			soc = 1;
		}
	} else {
		shutdown_dealy = false;
		shutdown_delay_cancel = false;
	}

report_shutdown:
	mca_log_info("ui_soc: %d, soc: %d, last_shutdown_delay: %d, shutdown_dealy: %d\n",
		*ui_soc, soc, last_shutdown_delay, shutdown_dealy);

	*ui_soc = soc;

	if (last_shutdown_delay != shutdown_dealy && fg->fake_soc == STRATEGY_FG_FAKE_SOC_NONE) {
		mca_log_err("shutdown_dealy changed: %d => %d\n", last_shutdown_delay, shutdown_dealy);
		len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
			"POWER_SUPPLY_SHUTDOWN_DELAY=%u\n", shutdown_dealy);
		n_data.event = event;
		n_data.event_len = len;
		mca_event_report_uevent(&n_data);
		last_shutdown_delay = shutdown_dealy;
	}

	return 0;
}
