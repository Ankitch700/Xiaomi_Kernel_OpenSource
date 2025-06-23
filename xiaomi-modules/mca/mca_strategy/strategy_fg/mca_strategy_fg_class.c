// SPDX-License-Identifier: GPL-2.0
/*
 *mca_strategy_fg_class.c
 *
 * mca fuelgauge strategy class driver
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
#include <linux/module.h>
#include <linux/platform_device.h>
#include <mca/strategy/strategy_fg_class.h>

static struct strategy_fg_class_info g_stg_fg_class_info;

#define is_invalid_ops(name) (!g_stg_fg_class_info.ops || !(g_stg_fg_class_info.ops->name))
#define strategy_fg_class_ops_no_para(name) (g_stg_fg_class_info.ops->name(g_stg_fg_class_info.data))
#define strategy_fg_class_ops_with_one_para(name, para1) (g_stg_fg_class_info.ops->name(g_stg_fg_class_info.data, para1))

int strategy_class_fg_ops_register(void *data, struct strategy_fg_class_ops *ops)
{
	if (!ops)
		return -1;

	g_stg_fg_class_info.data = data;
	g_stg_fg_class_info.ops = ops;

	return 0;
}
EXPORT_SYMBOL(strategy_class_fg_ops_register);

int strategy_class_fg_ops_is_init_ok(void)
{
	if (is_invalid_ops(strategy_fg_is_init_ok))
		return -1;

	return strategy_fg_class_ops_no_para(strategy_fg_is_init_ok);
}
EXPORT_SYMBOL(strategy_class_fg_ops_is_init_ok);

int strategy_class_fg_ops_get_rawsoc(int *rawsoc)
{
	if (is_invalid_ops(strategy_fg_get_rawsoc))
		return -1;

	return strategy_fg_class_ops_with_one_para(strategy_fg_get_rawsoc, rawsoc);
}
EXPORT_SYMBOL(strategy_class_fg_ops_get_rawsoc);

int strategy_class_fg_ops_get_rsoc(int *rsoc)
{
	if (is_invalid_ops(strategy_fg_get_rsoc))
		return -1;

	return strategy_fg_class_ops_with_one_para(strategy_fg_get_rsoc, rsoc);
}
EXPORT_SYMBOL(strategy_class_fg_ops_get_rsoc);

int strategy_class_fg_ops_get_soc(void)
{
	if (is_invalid_ops(strategy_fg_get_soc))
		return -1;

	return strategy_fg_class_ops_no_para(strategy_fg_get_soc);
}
EXPORT_SYMBOL(strategy_class_fg_ops_get_soc);

int strategy_class_fg_ops_get_temperature(int *temp)
{
	if (is_invalid_ops(strategy_fg_get_temp))
		return -1;

	return strategy_fg_class_ops_with_one_para(strategy_fg_get_temp, temp);
}
EXPORT_SYMBOL(strategy_class_fg_ops_get_temperature);

int strategy_class_fg_ops_get_current(int *curr)
{
	if (is_invalid_ops(strategy_fg_get_current))
		return -1;

	return strategy_fg_class_ops_with_one_para(strategy_fg_get_current, curr);
}
EXPORT_SYMBOL(strategy_class_fg_ops_get_current);

int strategy_class_fg_ops_get_voltage(int *volt)
{
	if (is_invalid_ops(strategy_fg_get_voltage))
		return -1;

	return strategy_fg_class_ops_with_one_para(strategy_fg_get_voltage, volt);
}
EXPORT_SYMBOL(strategy_class_fg_ops_get_voltage);

int strategy_class_fg_ops_get_cyclecount(int *cycle)
{
	if (is_invalid_ops(strategy_fg_get_cycle))
		return -1;

	return strategy_fg_class_ops_with_one_para(strategy_fg_get_cycle, cycle);
}
EXPORT_SYMBOL(strategy_class_fg_ops_get_cyclecount);

int strategy_class_fg_ops_get_recharge(int *if_rechging)
{
	if (is_invalid_ops(strategy_fg_get_recharge))
		return -1;

	return strategy_fg_class_ops_with_one_para(strategy_fg_get_recharge, if_rechging);
}
EXPORT_SYMBOL(strategy_class_fg_ops_get_recharge);

int strategy_class_fg_get_voltage_mean(int *vol_mean)
{
	if (is_invalid_ops(strategy_fg_get_voltage_mean))
		return -1;

	return strategy_fg_class_ops_with_one_para(strategy_fg_get_voltage_mean, vol_mean);
}
EXPORT_SYMBOL(strategy_class_fg_get_voltage_mean);

int strategy_class_fg_ops_get_soc_decimal(int *soc_decimal, int *rate)
{
	if (is_invalid_ops(strategy_fg_get_soc_decimal_info))
		return -1;

	return g_stg_fg_class_info.ops->strategy_fg_get_soc_decimal_info(g_stg_fg_class_info.data,
		soc_decimal, rate);
}
EXPORT_SYMBOL(strategy_class_fg_ops_get_soc_decimal);

bool strategy_class_fg_ops_get_charging_done(void)
{
	if (is_invalid_ops(strategy_fg_get_charging_done))
		return -1;

	return strategy_fg_class_ops_no_para(strategy_fg_get_charging_done);
}
EXPORT_SYMBOL(strategy_class_fg_ops_get_charging_done);

int strategy_class_fg_ops_set_charging_done(bool charging_done)
{
	if (is_invalid_ops(strategy_fg_set_charging_done))
		return -1;

	return strategy_fg_class_ops_with_one_para(strategy_fg_set_charging_done, charging_done);
}
EXPORT_SYMBOL(strategy_class_fg_ops_set_charging_done);

int strategy_class_fg_get_model_name(const char **model_name)
{

	if (is_invalid_ops(strategy_fg_get_model_name))
		return -1;

	return strategy_fg_class_ops_with_one_para(strategy_fg_get_model_name, model_name);
}
EXPORT_SYMBOL(strategy_class_fg_get_model_name);

int strategy_class_fg_set_fastcharge(bool en)
{

	if (is_invalid_ops(strategy_fg_set_fastcharge))
		return -1;

	return strategy_fg_class_ops_with_one_para(strategy_fg_set_fastcharge, en);
}
EXPORT_SYMBOL(strategy_class_fg_set_fastcharge);

int strategy_class_fg_get_fastcharge(void)
{

	if (is_invalid_ops(strategy_fg_get_fastcharge))
		return -1;

	return strategy_fg_class_ops_no_para(strategy_fg_get_fastcharge);
}
EXPORT_SYMBOL(strategy_class_fg_get_fastcharge);

int strategy_class_fg_get_authentic(bool *authentic)
{

	if (is_invalid_ops(strategy_fg_get_authentic))
		return -1;

	return strategy_fg_class_ops_with_one_para(strategy_fg_get_authentic, authentic);
}
EXPORT_SYMBOL(strategy_class_fg_get_authentic);

int strategy_class_fg_get_dc(int *dc)
{

	if (is_invalid_ops(strategy_fg_get_dc))
		return -1;

	return strategy_fg_class_ops_with_one_para(strategy_fg_get_dc, dc);
}
EXPORT_SYMBOL(strategy_class_fg_get_dc);

int strategy_class_fg_get_rm(int *rm)
{

	if (is_invalid_ops(strategy_fg_get_rm))
		return -1;

	return strategy_fg_class_ops_with_one_para(strategy_fg_get_rm, rm);
}
EXPORT_SYMBOL(strategy_class_fg_get_rm);

int strategy_class_fg_get_fcc(int *fcc)
{

	if (is_invalid_ops(strategy_fg_get_fcc))
		return -1;

	return strategy_fg_class_ops_with_one_para(strategy_fg_get_fcc, fcc);
}
EXPORT_SYMBOL(strategy_class_fg_get_fcc);

int strategy_class_fg_is_chip_ok(void)
{

	if (is_invalid_ops(strategy_fg_is_chip_ok))
		return -1;

	return strategy_fg_class_ops_no_para(strategy_fg_is_chip_ok);
}
EXPORT_SYMBOL(strategy_class_fg_is_chip_ok);

int strategy_class_fg_get_health(int *health)
{
	if (is_invalid_ops(strategy_fg_get_health))
		return -1;

	return strategy_fg_class_ops_with_one_para(strategy_fg_get_health, health);
}
EXPORT_SYMBOL(strategy_class_fg_get_health);

int strategy_class_fg_get_high_temp_vterm(void)
{
	if (is_invalid_ops(strategy_fg_get_high_temp_vterm))
		return -1;

	return strategy_fg_class_ops_no_para(strategy_fg_get_high_temp_vterm);
}
EXPORT_SYMBOL(strategy_class_fg_get_high_temp_vterm);

static struct platform_driver strategy_fg_class_driver = {
	.driver	= {
		.name = "strategy_fg_class",
	},
};

module_platform_driver(strategy_fg_class_driver);
MODULE_DESCRIPTION("platform fg class");
MODULE_AUTHOR("liyuze1@xiaomi.com");
MODULE_LICENSE("GPL v2");

