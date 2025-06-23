/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mca_battery_shutdown.h
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
#ifndef __BATTERY_SHUTDOWN_H__
#define __BATTERY_SHUTDOWN_H__

int mca_battery_shutdown_update_vcutoff_para(void *data);
int mca_battery_shutdown_parse_dt(void *data);
int mca_battery_shutdown_check_shutdown_delay(void *data, int *ui_soc);

/* temp para */
#define BATTERY_TEMP_PARA_MAX_GROUP 8
enum mca_battery_shutdown_temp_para_ele {
	MCA_BATTERY_SHUTDOWN_TEMP_LOW = 0,
	MCA_BATTERY_SHUTDOWN_TEMP_HIGH,
	MCA_BATTERY_SHUTDOWN_LOW_TEMP_HYS,
	MCA_BATTERY_SHUTDOWN_HIGH_TEMP_HYS,
	MCA_BATTERY_SHUTDOWN_VCUTOFF_PARA_NAME,
	MCA_BATTERY_SHUTDOWN_TEMP_PARA_MAX,
};

struct mca_battery_shutdown_temp_para {
	int temp_low;
	int temp_high;
	int low_temp_hysteresis;
	int high_temp_hysteresis;
};

/* volt para */
#define VCUTOFF_PARA_MAX_GROUP 8
enum mca_battery_shutdown_vcutoff_para_ele {
	MCA_BATTERY_SHUTDOWN_RANGE_UPPER = 0,
	MCA_BATTERY_SHUTDOWN_VCUTOFF_FOR_FW,
	MCA_BATTERY_SHUTDOWN_VCUTOFF_FOR_SHUTDOWN_DELAY,
	MCA_BATTERY_SHUTDOWN_VCUTOFF_FOR_SW,
	MCA_BATTERY_SHUTDOWN_VCUTOFF_PARA_MAX,
};
struct mca_battery_shutdown_vcutoff_para {
	int range_upper;
	int vcutoff_fw;
	int vcutoff_shutdown_delay;
	int vcutoff_sw;
};

struct mca_battery_shutdown_vcutoff_para_info {
	int vcutoff_para_size;
	struct mca_battery_shutdown_vcutoff_para *vcutoff_para;
};

struct mca_battery_shutdown_temp_para_info {
	struct mca_battery_shutdown_temp_para temp_para;
	struct mca_battery_shutdown_vcutoff_para_info vcutoff_info;
};

struct mca_battery_shutdown_para_info {
	int temp_para_size;
	int cur_temp_index;
	struct mca_battery_shutdown_temp_para_info *temp_info;
};

#endif /* __BATTERY_SHUTDOWN_H__ */
