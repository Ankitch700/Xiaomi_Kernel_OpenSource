/* SPDX-License-Identifier: GPL-2.0 */
/*
 * strategy_fg.h
 *
 * fg stategy interface driver
 *
 * Copyright (c) 2024-2024 Xiaomi Technologies Co., Ltd.
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
#ifndef __STRATEGY_FG_H__
#define __STRATEGY_FG_H__

#include <linux/iio/consumer.h>
#include <linux/power_supply.h>
#include <linux/ktime.h>
#include "mca_battery_shutdown.h"
#include "mca_battery_full.h"
#include <mca/platform/platform_fg_ic_ops.h>

#define STRATEGY_FG_FAKE_SOC_NONE -1
#define STRATEGY_FG_FAKE_TEMP_NONE -999
#define STRATEGY_FG_FAKE_BAP_MATCH_NONE -1

#define SOC_DECIMAL_MAX_LEVEL 16
#define MAX_CC_ZONE 3
#define MAX_DOD_ZONE 3
#define SAFE_ITERM_CYCLE_LEVEL 5
#define SAFE_ITERM_TEMP_LEVEL 7
#define CYCLECOUNT 3
#define VOLT_THRESHOLD 2
enum weight_count_level_elem {
	WEIGHT_LEVEL_1 = 0,
	WEIGHT_LEVEL_2,
	WEIGHT_LEVEL_3,
	WEIGHT_LEVEL_MAX,
};

enum soc_decimal_iterm {
	SOC_DECIMAL_SOC = 0,
	SOC_DECIMAL_RATE,
	SOC_DECIMAL_MAX,
};

enum strategy_fg_type {
	MCA_FG_TYPE_SINGLE = 0,
	MCA_FG_TYPE_SINGLE_SERIES,   /* single fuelgauge, two cells in series */
	MCA_FG_TYPE_SINGLE_NUM_MAX = MCA_FG_TYPE_SINGLE_SERIES,
	MCA_FG_TYPE_PARALLEL,
	MCA_FG_TYPE_SERIES,
	MCA_FG_TYPE_MAX = MCA_FG_TYPE_SERIES,
};

struct strategy_fg_soc_decimal {
	int soc;
	int rate;
};

struct lossless_rechg_cfg {
	int temp_low;
	int temp_high;
	int rest_time;
};

#define FG_EXTREME_COLD_MAX_GROUP 10
enum fg_extreme_cold_para_ele {
	FG_EXTREME_COLD_CYCLE_COUNT_MAX = 0,
	FG_EXTREME_COLD_TEMP_MAX,
	FG_EXTREME_COLD_COMPENSATION,
	FG_EXTREME_COLD_SIZE,
};

struct fg_extreme_cold_para {
	int cycle_count_max;
	int temp_max;
	int compensation;
};

struct term_curr_data {
	int batt_temp;
	int batt_temp_offset;
	int normal_iterm;
	int ffc_iterm;
};

struct term_curr_para {
	int iterm_para_size;
	int index;
	struct term_curr_data *iterm_data;
};

struct strategy_fg_cfg {
	int report_full_raw_soc;
	int fg_type;
	int soc_proportion;
	int soc_proportion_c;
	int extreme_batt_temp_cool_thr;
	int extreme_batt_temp_cold_thr;
	int extreme_vbat_low_normal_thr;
	int extreme_vbat_low_cool_thr;
	int extreme_vbat_low_cold_thr;
	int soc_decimal_cnt;
	int adapt_power;
	int support_dtpt;
	int design_capacity;
	int terminated_by_cp;
	int support_full_curr_monitor;

	const char *model_name;
	const char *model_name_gl;
	struct strategy_fg_soc_decimal soc_decimal[SOC_DECIMAL_MAX_LEVEL];
	struct lossless_rechg_cfg rechg_cfg;
	int support_lossless_rechg;
	int extreme_cold_para_size;
	struct fg_extreme_cold_para extreme_cold_para[FG_EXTREME_COLD_MAX_GROUP];
	int support_base_flip;
	struct term_curr_para term_curr_data[FG_IC_MAX];
	int fg_hightemp_vterm;
	int support_fl4p0;
	int ffc_safe_item[SAFE_ITERM_CYCLE_LEVEL][SAFE_ITERM_TEMP_LEVEL];
	int full_volt_monitor[CYCLECOUNT][VOLT_THRESHOLD];
	int support_global;
};

struct term_volt_cfg {
	int max;
	int value;
};

enum term_volt_para {
	RANGE_HI,
	VALUE,
};

struct fg_batt_info {
	int curr;
	int volt;
	int vcell_max;
	int temp;
	int rsoc;
	int rm;
	int fcc;
	int cycle_count;
	int soh;
};

#define VOL_SMOOTH_LEN		5
#define CURR_SMOOTH_LEN		4
#define TEMP_SMOOTH_LEN		2
struct strategy_fg_smooth {
	int suppot_RSOC_0_smooth;
	int smooth_curr_unit[CURR_SMOOTH_LEN];
	int smooth_vol_offset[VOL_SMOOTH_LEN];
	int smooth_temp_unit[TEMP_SMOOTH_LEN];
	int dischg_smooth_low_temp_unit[VOL_SMOOTH_LEN][CURR_SMOOTH_LEN];
	int dischg_smooth_high_temp_unit[VOL_SMOOTH_LEN][CURR_SMOOTH_LEN];
};

struct strategy_fg {
	struct device *dev;

	struct delayed_work monitor_work;
	struct delayed_work get_strategy_fg_ops_work;
	struct delayed_work delay_reset_full_flag_work;
	struct delayed_work dtpt_monitor_work;
	struct delayed_work fl4p0_calibration_work;
	struct delayed_work full_current_monitor_work;
	struct delayed_work screen_and_audio_status_work;
	struct delayed_work force_report_full_work;

	struct notifier_block panel_nb;
	struct notifier_block thermal_board_nb;
	struct strategy_fg_cfg cfg;
	struct strategy_fg_smooth smooth;
	bool fg_init_flag;
	int chg_status;
	ktime_t suspend_time;
	bool fg_error;
	bool charging_done;
	bool recharging;
	bool lossless_recharge;
	bool is_eu_model;
	bool en_smooth_full;
	int real_type;

	bool voter_ok;
	struct mca_votable *en_voter;
	struct mca_votable *vterm_voter;
	struct mca_votable *iterm_voter;
	struct mca_votable *charge_limit_voter;

	int batt_current;
	int batt_current_mean;
	int batt_voltage;
	int batt_voltage_mean;
	int batt_vcell_max;
	int batt_rsoc;
	int batt_cyclecount;
	int batt_temperature;
	int batt_status;
	int batt_soh;
	int batt_rm;
	int batt_fcc;
	int batt_raw_soc;
	int batt_mapped_soc;
	int batt_mapped_soc_decimal;
	int batt_ui_soc;
	int batt_ui_soc_decimal;
	int batt_index;
	int batt_dc;
	int batt_health;
	int thermal_board_temp;

	time64_t plugin_time;
	bool keep_full_flag;

	struct power_supply *batt_psy;
	bool fast_charge;
	bool screen_status;
	enum battery_full_status battery_full_status;
	int audio_state;
	int dod_count;
	int vcutoff_fw;
	int vcutoff_shutdown_delay;
	int vcutoff_sw;
	bool vbatt_empty;
	bool support_cc_vcutoff;
	bool support_dod_vcutoff;
	bool support_vpack_low_shutdown;
	bool support_full_design_gl;
	int weight[WEIGHT_LEVEL_MAX];
	struct mca_battery_shutdown_para_info batt_spec_para;
	struct mca_battery_shutdown_para_info batt_health_para;
	int fake_bap_match;
	int fake_soc;
	int fake_temp;
	int fake_dod_count;
	bool enable_rollback;
	int batt_auth;
	int batt_slave_auth;
	struct fg_batt_info batt_info;
	struct fg_batt_info master_batt_info;
	struct fg_batt_info slave_batt_info;
	int update_period;
	int update_interval;
	int pvbat;
	int pibat;
	int cyclecount_hundred;
	int calibration_temp;
	bool start_force_full;
	bool power_present;
	bool resume_update_soc;
	bool boot_update_soc;
	bool fg1_batt_ctr_enabled;
	bool fg2_batt_ctr_enabled;
	bool fg_lock_flag;
	bool dual_force_full[FG_IC_MAX];
	int dual_iterm[FG_IC_MAX];
	bool monitor_soc_flag;
	bool first_termination;
	bool near_vterm;
	bool ffc_continue_charge;
	bool self_equal_flag[FG_IC_MAX];
	bool self_equal_count[FG_IC_MAX];
	int full_current_count;
	int sa_status_last;
	int sa_status;
	};

enum fg_auth_attr_list	{
	FG_PROP_MAIN_AUTH = 0,
	FG_PROP_SLAVE_AUTH,
	FG_PROP_BATT_INDEX,
	FG_PROP_VERIFY_DIGEST,
	FG_PROP_BATT_POWER_MATCH,
	FG_PROP_FAST_CHARGE,
	FG_PROP_SOC_DECIMAL,
	FG_PROP_SOC_DECIMAL_RATE,
	FG_PROP_FAKE_SOC,
	FG_PROP_FAKE_TEMP,
	FG_PROP_BATTERY_NUM,
	FG_PROP_UPDATE_PERIOD,
	FG_PROP_PACK_VOLTAGE,
	FG_PROP_DOD_COUNT,
	FG_PROP_ENABLE_ROLLBACK,
	FG_PROP_AUDIO_STATE,
	FG_PROP_SOH_NEW,
	FG_PROP_BCL_MAX_POWERCAP,
	FG_PROP_CALC_RVALUE,
};

void strategy_fg_record_volt_mean(struct strategy_fg *fg);

#endif /* __STRATEGY_FG_H__ */
