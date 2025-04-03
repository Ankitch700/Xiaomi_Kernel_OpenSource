// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2023 Huaqin Technology(Shanghai) Co., Ltd.
 */

#include "hq_charger_manager.h"
#include "../hq_printk.h"
#include <linux/of.h>
#ifdef TAG
#undef TAG
#define  TAG "[HQ_CHG][CM]"
#endif
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
#include "xm_smart_chg.h"
#include "tcpci.h"

BLOCKING_NOTIFIER_HEAD(shipmode_shutdown_notifier_list);
EXPORT_SYMBOL_GPL(shipmode_shutdown_notifier_list);
int shipmode_shutdown_notifier_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&shipmode_shutdown_notifier_list, nb);
}
EXPORT_SYMBOL(shipmode_shutdown_notifier_register_client);

int shipmode_shutdown_notifier_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&shipmode_shutdown_notifier_list, nb);
}
EXPORT_SYMBOL(shipmode_shutdown_notifier_unregister_client);

int shipmode_shutdown_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&shipmode_shutdown_notifier_list, val, v);
}
EXPORT_SYMBOL(shipmode_shutdown_notifier_call_chain);

/*thermal board temp*/
SRCU_NOTIFIER_HEAD(charger_thermal_notifier);
EXPORT_SYMBOL_GPL(charger_thermal_notifier);

int charger_thermal_reg_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_register(&charger_thermal_notifier, nb);
}
EXPORT_SYMBOL_GPL(charger_thermal_reg_notifier);

int charger_thermal_unreg_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&charger_thermal_notifier, nb);
}
EXPORT_SYMBOL_GPL(charger_thermal_unreg_notifier);

int charger_thermal_notifier_call_chain(unsigned long event, int val)
{
	return srcu_notifier_call_chain(&charger_thermal_notifier, event, &val);
}
EXPORT_SYMBOL_GPL(charger_thermal_notifier_call_chain);
#endif

static int charger_manager_wake_thread(struct charger_manager *manager)
{
	manager->run_thread = true;
	wake_up(&manager->wait_queue);
	return 0;
}

static void hrtime_otg_work(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
					struct charger_manager, hrtime_otg_work.work);
	if(manager != NULL && manager->tcpc != NULL)
		tcpm_typec_change_role(manager->tcpc, TYPEC_ROLE_SNK);
	hq_info("hrtime_otg_work enter\n");
}

static enum alarmtimer_restart rust_det_work_timer_handler(struct alarm *alarm, ktime_t now)
{
	struct charger_manager *manager = container_of(alarm,
				struct charger_manager, rust_det_work_timer);
	if(manager != NULL)
	{
		manager->ui_cc_toggle = false;
		schedule_delayed_work(&manager->hrtime_otg_work, 0);
	}
	hq_info("rust_det_work_timer_handler enter\n");
    return ALARMTIMER_NORESTART;
}

int charger_manager_get_current(struct charger_manager *manager, int *curr)
{
	int val;
	int ret = 0;
	union power_supply_propval pval;

	*curr = 0;

	ret = charger_get_adc(manager->charger, ADC_GET_IBUS, &val);
	if (ret < 0) {
		hq_err("Couldn't read input curr ret=%d\n", ret);
	} else
		*curr += val;

	if(IS_ERR_OR_NULL(manager->cp_master_psy))
		hq_err("cp_master_psy is NULL.\n");
	else {
		ret = power_supply_get_property(manager->cp_master_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
		if (ret < 0)
			hq_err("Couldn't get cp curr  by power supply ret=%d\n", ret);
		else
			*curr += pval.intval;
	}

	if(IS_ERR_OR_NULL(manager->cp_slave_psy))
		hq_err("cp_slave_psy is NULL.\n");
	else {
		ret = power_supply_get_property(manager->cp_slave_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
		if (ret < 0)
			hq_err("Couldn't get cp curr  by power supply ret=%d\n", ret);
		else
			*curr += pval.intval;
	}

	return 0;
}
EXPORT_SYMBOL(charger_manager_get_current);

void hq_set_prop_system_temp_level(struct charger_manager *manager,  char *voter_name)
{
	int rc;

	if (manager->system_temp_level <= 0)
		goto err;

	if (manager->pd_active == CHARGE_PD_PPS_ACTIVE || !strcmp(voter_name, CALL_THERMAL_DAEMON_VOTER)) {
		if (manager->thermal_parse_flags & PD_THERM_PARSE_ERROR) {
			hq_err("pd thermal dtsi parse error\n");
			goto err;
		}
		if (manager->system_temp_level > manager->pd_thermal_levels) {
			hq_err("system_temp_level is invalid\n");
			goto err;
		}
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
                if(manager->pps_fast_mode && (manager->low_fast_ffc >= 2150)){
                        vote(manager->total_fcc_votable, voter_name, true, manager->low_fast_ffc);
                }else{
                        vote(manager->total_fcc_votable, voter_name, true,
                                manager->pd_thermal_mitigation[manager->system_temp_level]);
                }
#else
                vote(manager->total_fcc_votable, voter_name, true,
                        manager->pd_thermal_mitigation[manager->system_temp_level]);
#endif
	} else {
		if (manager->thermal_parse_flags & QC2_THERM_PARSE_ERROR) {
			hq_err("qc thermal dtsi parse error\n");
			goto err;
		}
		if (manager->system_temp_level > manager->qc2_thermal_levels) {
			hq_err("system_temp_level is invalid\n");
			goto err;
		}
		vote(manager->total_fcc_votable, voter_name, true,
			manager->qc2_thermal_mitigation[manager->system_temp_level]);
	}

	rc = get_client_vote_locked(manager->total_fcc_votable, voter_name);
	hq_info("%s: thermal vote susessful val = %d, current = %d\n", voter_name, manager->system_temp_level, rc);

	return;
err:
	vote(manager->total_fcc_votable, voter_name, false, 0);
	return;
}
EXPORT_SYMBOL(hq_set_prop_system_temp_level);

static int of_property_get_array(struct device *dev, char *name, int *size, int **data)
{
	struct device_node *node = dev->of_node;
	int byte_len, rc;
	int *out_value;
	hq_info("get array out_value!\n");
	if (of_find_property(node, name, &byte_len)) {
		out_value = devm_kzalloc(dev, byte_len, GFP_KERNEL);
		*data = out_value;
		if (IS_ERR_OR_NULL(out_value)) {
			hq_err("out_value kzalloc error\n");
			return -ENOMEM;
		} else {
			*size = byte_len / sizeof(u32);
			rc = of_property_read_u32_array(node, name, out_value, *size);
			if (rc < 0){
				hq_err("parse error\n");
				return -ENOMEM;
			}
		}
	}else{
		hq_err("node not found\n");
		return -ENOMEM;
	}
	return 0;
}

static int charge_manager_thermal_init(struct charger_manager *manager)
{
	int byte_len, rc, ret = 0;
	struct device_node *node = manager->dev->of_node;

	manager->thermal_enable = of_property_read_bool(node, "hq,thermal-enable");
	#ifdef KERNEL_FACTORY_BUILD
	manager->thermal_enable = false;
	#endif
	if (manager->thermal_enable == false) {
		hq_err("thermal ibat limit is disable\n");
		return -EINVAL;
	}

	if (of_find_property(node, "hq,pd-thermal-mitigation", &byte_len)) {
		manager->pd_thermal_mitigation = devm_kzalloc(manager->dev, byte_len, GFP_KERNEL);
		if (IS_ERR_OR_NULL(manager->pd_thermal_mitigation)) {
			ret |= PD_THERM_PARSE_ERROR;
			hq_err("pd_thermal_mitigation kzalloc error\n");
		} else {
			manager->pd_thermal_levels = byte_len / sizeof(u32);
			rc = of_property_read_u32_array(node, "hq,pd-thermal-mitigation",
				manager->pd_thermal_mitigation, manager->pd_thermal_levels);
			if (rc < 0) {
				ret |= PD_THERM_PARSE_ERROR;
				hq_err("pd_thermal_mitigation parse error\n");
			}
		}
	} else {
		ret |= PD_THERM_PARSE_ERROR;
		hq_err("pd_thermal_mitigation not found\n");
	}

	if (of_find_property(node, "hq,qc2-thermal-mitigation", &byte_len)) {
		manager->qc2_thermal_mitigation = devm_kzalloc(manager->dev, byte_len, GFP_KERNEL);
		if (IS_ERR_OR_NULL(manager->qc2_thermal_mitigation)) {
			ret |= QC2_THERM_PARSE_ERROR;
			hq_err("qc2_thermal_mitigation kzalloc error\n");
		} else {
			manager->qc2_thermal_levels = byte_len / sizeof(u32);
			rc = of_property_read_u32_array(node, "hq,qc2-thermal-mitigation",
				manager->qc2_thermal_mitigation, manager->qc2_thermal_levels);
			if (rc < 0) {
				ret |= QC2_THERM_PARSE_ERROR;
				hq_err("qc2_thermal_mitigation parse error\n");
			}
		}
	} else {
		ret |= QC2_THERM_PARSE_ERROR;
		hq_err("qc2_thermal_mitigation not found\n");
	}
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
        if (of_find_property(node, "hq,pd-thermal-mitigation-fast", &byte_len)) {
		manager->pd_thermal_mitigation_fast = devm_kzalloc(manager->dev, byte_len, GFP_KERNEL);
		if (IS_ERR_OR_NULL(manager->pd_thermal_mitigation_fast)) {
			ret |= PD_THERM_PARSE_ERROR;
			hq_err("pd_thermal_mitigation_fast kzalloc error\n");
		} else {
			manager->pd_thermal_levels = byte_len / sizeof(u32);
			rc = of_property_read_u32_array(node, "hq,pd-thermal-mitigation-fast",
				manager->pd_thermal_mitigation_fast, manager->pd_thermal_levels);
			if (rc < 0) {
				ret |= PD_THERM_PARSE_ERROR;
				hq_err("pd_thermal_mitigation_fast parse error\n");
			}
		}
	} else {
		ret |= PD_THERM_PARSE_ERROR;
		hq_err("pd_thermal_mitigation_fast not found\n");
	}
#endif
	manager->thermal_parse_flags = ret;
	if (ret == (QC2_THERM_PARSE_ERROR | PD_THERM_PARSE_ERROR)) {
		manager->thermal_enable = false;
		ret = -EINVAL;
	}

	return ret;
}

static int main_chg_fcc_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct charger_manager *manager = data;
	int ret = 0;

	if (value < 0) {
		hq_err("the value of main fcc is error.\n");
		return value;
	}

	ret = charger_set_ichg(manager->charger, value);
	if (ret < 0) {
		hq_err("charger set ichg fail.\n");
	}
	return ret;
}

static int main_chg_fv_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct charger_manager *manager = data;
	int ret = 0;

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
        pr_err("%s old value = %d\n", __func__, value);
        value = value - manager->smart_batt - manager->fv_againg;
        pr_err("new value = %d, smart_batt = %d, fv_againg = %d\n", value, manager->smart_batt, manager->fv_againg);
#endif
	ret = charger_set_term_volt(manager->charger, value);
	if (ret < 0) {
		hq_err("charger set term volt fail.\n");
	}
	return ret;
}

static int main_chg_icl_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct charger_manager *manager = data;
	int ret = 0;

	if (value < 0) {
		hq_err("the value of main chg icl is error.\n");
		return value;
	}

	ret = charger_set_input_curr_lmt(manager->charger, value);
	if (ret < 0) {
		hq_err("charger set icl fail.\n");
	}
	return ret;
}

static int main_chg_iterm_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct charger_manager *manager = data;
	int ret = 0;

	ret = charger_set_term_curr(manager->charger, value);
	if (ret < 0) {
		hq_err("charger set iterm fail.\n");
	}
	return ret;
}

static int total_fcc_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct charger_manager *manager = data;

	if (value >= FASTCHARGE_MIN_CURR && (g_policy->state == POLICY_RUNNING)) {
		if (IS_ERR_OR_NULL(manager->main_icl_votable)) {
			hq_err("main_icl_votable not found\n");
			return PTR_ERR(manager->main_icl_votable);
		} else
			vote(manager->main_icl_votable, MAIN_FCC_MAX_VOTER, true, CP_EN_MAIN_CHG_CURR);
		if (IS_ERR_OR_NULL(manager->main_fcc_votable)) {
			hq_err("main_fcc_votable not found\n");
			return PTR_ERR(manager->main_fcc_votable);
		} else
			vote(manager->main_fcc_votable, MAIN_FCC_MAX_VOTER, true, CP_EN_MAIN_CHG_CURR);
	} else {
		if (IS_ERR_OR_NULL(manager->main_icl_votable)) {
			hq_err("-->main_icl_votable2 not found\n");
			return PTR_ERR(manager->main_icl_votable);
		} else
			vote(manager->main_icl_votable, MAIN_FCC_MAX_VOTER, false, 0);
		if (IS_ERR_OR_NULL(manager->main_fcc_votable)) {
			hq_err("-->main_fcc_votable not found\n");
			return PTR_ERR(manager->main_fcc_votable);
		} else {
			if (value >= 0)
				vote(manager->main_fcc_votable, MAIN_FCC_MAX_VOTER, true, value);
		}
	}

	return 0;
}

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
static int is_full_vote_callback(struct votable *votable,
			void *data, int is_full_flag, const char *client)
{
	struct charger_manager *manager = data;

	manager->is_full_flag = is_full_flag;
        pr_err("%s client: %s is_full_flag: %d\n", __func__, client, is_full_flag);
	return 0;
}
#endif

static int main_chg_disable_vote_callback(struct votable *votable, void *data, int enable, const char *client)
{
	struct charger_manager *manager = data;
	int ret = 0;

	ret = charger_disable_power_path(manager->charger, enable);
	if (ret < 0) {
		hq_err("charger disable_power_path fail.\n");
	}
	return ret;
}

static int cp_disable_vote_callback(struct votable *votable, void *data, int enable, const char *client)
{
	struct charger_manager *manager = data;
	struct chargerpump_dev *master_cp_chg = manager->master_cp_chg;
	struct chargerpump_dev *slave_cp_chg = manager->slave_cp_chg;
	int ret = 0;
	if(manager->cp_master_use){
		ret = chargerpump_set_enable(master_cp_chg, enable);
		if (ret < 0) {
			hq_err("master_cp_chg set chg fail.\n");
		}
	}
	if(manager->cp_slave_use){
		ret = chargerpump_set_enable(slave_cp_chg, enable);
		if (ret < 0) {
			hq_err("slave_cp_chg set chg fail.\n");
		}
	}
	return ret;
}

static int charger_manager_create_votable(struct charger_manager *manager)
{
	int ret = 0;

	if (manager->charger) {
		manager->main_fcc_votable = create_votable("MAIN_FCC", VOTE_MIN, main_chg_fcc_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->main_fcc_votable)) {
			hq_err("fail create MAIN_FCC voter.\n");
			return PTR_ERR(manager->main_fcc_votable);
		}

		manager->fv_votable = create_votable("MAIN_FV", VOTE_MIN, main_chg_fv_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->fv_votable)) {
			hq_err("fail create MAIN_FV voter.\n");
			return PTR_ERR(manager->fv_votable);
		}

		manager->main_icl_votable = create_votable("MAIN_ICL", VOTE_MIN, main_chg_icl_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->main_icl_votable)) {
			hq_err("fail create MAIN_ICL voter.\n");
			return PTR_ERR(manager->main_icl_votable);
		}

		manager->iterm_votable = create_votable("MAIN_ITERM", VOTE_MIN, main_chg_iterm_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->iterm_votable)) {
			hq_err("fail create MAIN_ICL voter.\n");
			return PTR_ERR(manager->iterm_votable);
		}

		manager->main_chg_disable_votable = create_votable("MAIN_CHG_DISABLE", VOTE_SET_ANY, main_chg_disable_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->main_chg_disable_votable)) {
			hq_err("fail create MAIN_CHG_DISABLE voter.\n");
			return PTR_ERR(manager->main_chg_disable_votable);
		}

		manager->total_fcc_votable = create_votable("TOTAL_FCC", VOTE_MIN, total_fcc_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->total_fcc_votable)) {
			hq_err("fail create TOTAL_FCC voter.\n");
			return PTR_ERR(manager->total_fcc_votable);
		}
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
                manager->is_full_votable = create_votable("IS_FULL", VOTE_SET_ANY, is_full_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->is_full_votable)) {
			hq_err("fail create IS_FULL voter.\n");
			return PTR_ERR(manager->is_full_votable);
		}
#endif
	}

	if (manager->cp_master_use || manager->cp_slave_use) {
		manager->cp_disable_votable = create_votable("CP_DISABLE", VOTE_SET_ANY, cp_disable_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->cp_disable_votable)) {
			hq_err("fail create CP_DISABLE voter.\n");
			return PTR_ERR(manager->cp_disable_votable);
		}
	}
	return ret;
}

#if IS_ENABLED(CONFIG_TCPC_CLASS)

static void set_cc_drp_work(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
				struct charger_manager, set_cc_drp_work.work);
	if(manager != NULL && manager->tcpc != NULL)
		tcpm_typec_change_role(manager->tcpc, TYPEC_ROLE_DRP);
	hq_info("set_cc_drp_work_func enter\n");
}

static int charger_manager_tcpc_notifier_call(struct notifier_block *nb,
					unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct charger_manager *manager =
		container_of(nb, struct charger_manager, pd_nb);

	uint8_t old_state = TYPEC_UNATTACHED, new_state = TYPEC_UNATTACHED;
	struct timespec64 end_time, time_now;
	ktime_t ktime, ktime_now;
	int ret = 0;

	hq_info("noti event: %d %d\n", (int)event, (int)noti->pd_state.connected);
	switch (event) {
	case TCP_NOTIFY_SINK_VBUS:
/* N6R code for HQ-429228 at 2024/11/08 by p-mazhuang3 start */
			if (IS_ERR_OR_NULL(manager->main_icl_votable)) {
  			hq_err("main_icl_votable not found\n");
  			break;
  		}
/* N6R code for HQ-429228 at 2024/11/08 by p-mazhuang3 end */
		if (noti->vbus_state.type & TCP_VBUS_CTRL_PD_DETECT) {
			manager->pd_curr_max = noti->vbus_state.ma;
			manager->pd_volt_max = noti->vbus_state.mv;

			vote(manager->main_icl_votable, PD_SINK_VBUS_VOTER, true, manager->pd_curr_max);
			hq_info("TCP_NOTIFY_SINK_VBUS pd_curr_max = %d\n", manager->pd_curr_max);
/* N6R code for HQ-429228 at 2024/11/08 by p-mazhuang3 start */
			if (manager->pd_curr_max == 0) {
  				chargerpump_set_enable(manager->master_cp_chg, false);
				chargerpump_set_enable(manager->slave_cp_chg, false);
  				hq_info("PD notify pd_curr_max = 0, disable cp !\n");
  			}
			
/* N6R code for HQ-429228 at 2024/11/08 by p-mazhuang3 end */

		}
		break;
	case TCP_NOTIFY_SOURCE_VBUS:
		break;
	case TCP_NOTIFY_TYPEC_STATE:
		if (noti->typec_state.new_state == TYPEC_UNATTACHED)
			manager->pd_active = CHARGE_PD_INVALID;

		old_state = noti->typec_state.old_state;
		new_state = noti->typec_state.new_state;

		if (IS_ERR_OR_NULL(manager) || IS_ERR_OR_NULL(manager->tcpc)) {
			hq_err("manager or tcpc is nullptr\n");
			break;
		}

		if (old_state == TYPEC_UNATTACHED &&
				new_state != TYPEC_UNATTACHED &&
				!manager->typec_attach) {
			hq_info("typec plug in, polarity = %d\n", noti->typec_state.polarity);
			manager->typec_attach = true;
			manager->cid_status = true;
			if(manager->ui_cc_toggle){
				ret = alarm_try_to_cancel(&manager->rust_det_work_timer);
				if (ret < 0) {
					hq_err("callback was running, skip timer\n");
				}
				hq_info("OTG ON:typec plug in, cancel hrtimer\n");
			}
		} else if (old_state != TYPEC_UNATTACHED &&
						new_state == TYPEC_UNATTACHED &&
						manager->typec_attach) {
			hq_info("typec plug out\n");
			manager->typec_attach = false;
			manager->cid_status = false;
			if(manager->ui_cc_toggle) {
				if(manager->tcpc != NULL) {
					hq_err("OTG ON:typec plug out, ui set cc toggle\n");
					schedule_delayed_work(&manager->set_cc_drp_work, msecs_to_jiffies(500));
				}
				ret = alarm_try_to_cancel(&manager->rust_det_work_timer);
				if (ret < 0) {
					hq_err("callback was running, skip timer\n");
				}
				ktime_now = ktime_get_boottime();
				time_now = ktime_to_timespec64(ktime_now);
				end_time.tv_sec = time_now.tv_sec + 600;
				end_time.tv_nsec = time_now.tv_nsec + 0;
				ktime = ktime_set(end_time.tv_sec,end_time.tv_nsec);
				hq_info("OTG ON:alarm timer start:%d, %lld %ld\n", ret,
						end_time.tv_sec, end_time.tv_nsec);
				alarm_start(&manager->rust_det_work_timer, ktime);

			}
		}
		break;
	case TCP_NOTIFY_PR_SWAP:
		manager->is_pr_swap = true;
		if (noti->swap_state.new_role == PD_ROLE_SINK)
			manager->pd_active = 10;
		break;
	case TCP_NOTIFY_PD_STATE:
		switch (noti->pd_state.connected) {
		case PD_CONNECT_NONE:
			manager->pd_curr_max = 0;
			manager->pd_active = CHARGE_PD_INVALID;
			manager->is_pr_swap = false;
			manager->pd_contract_update = false;
			break;
		case PD_CONNECT_PE_READY_SNK_APDO:
			manager->pd_contract_update = true;
			manager->pd_active = noti->pd_state.connected = CHARGE_PD_PPS_ACTIVE;
			hq_set_prop_system_temp_level(manager, TEMP_THERMAL_DAEMON_VOTER);
		#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
			xm_uevent_report(manager);
		#endif
			break;
		case PD_CONNECT_PE_READY_SNK:
		case PD_CONNECT_PE_READY_SNK_PD30:
			manager->pd_active = noti->pd_state.connected = CHARGE_PD_ACTIVE;
			break;
		default:
			break;
		}
		charger_manager_wake_thread(manager);
		break;

	default:
		break;
	}
	return NOTIFY_OK;
}
#endif

#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
static int charger_monitor_fg_i2c_status(struct charger_manager *manager) {
	int ret = 0;
	int vbus_volt = 0;

	ret = fuel_gauge_check_i2c_function(manager->fuel_gauge);
	if (!manager->vbus_type)
		return ret;
	if (ret) {
		charger_get_adc(manager->charger, ADC_GET_VBUS, &vbus_volt);
		if (vbus_volt > FG_I2C_ERR_VBUS) {
			vote(manager->main_fcc_votable, FG_I2C_ERR, true, 300);
			vote(manager->main_icl_votable, FG_I2C_ERR, true, 300);
		} else {
			vote(manager->main_fcc_votable, FG_I2C_ERR, true, 500);
			vote(manager->main_icl_votable, FG_I2C_ERR, true, 500);
		}
	} else {
		vote(manager->main_fcc_votable, FG_I2C_ERR, false, 0);
		vote(manager->main_icl_votable, FG_I2C_ERR, false, 0);
	}
	return ret;
}
#endif

static void charger_manager_keep_uisoc(struct charger_manager *manager)
{
	int rsoc = 0;
	static int old_rsoc;
	bool is_full_flag = false;
	static bool rechg_flag;
	struct bq_fg_chip* bq;
	struct votable	*is_full_votable = NULL;

	is_full_flag = manager->is_full_flag;

	bq = (struct bq_fg_chip *)power_supply_get_drvdata(manager->fg_psy);
	rsoc = bq->raw_soc;

	if(old_rsoc > rsoc){
		rechg_flag = true;
	}
	old_rsoc = rsoc;
	hq_info("debug_set_rechg rsoc=%d is_full_flag=%d rechg_flag=%d\n",rsoc,is_full_flag,rechg_flag);

	if (rsoc < 9730 && is_full_flag && rechg_flag) {
		fuel_gauge_set_fastcharge_mode(manager->fuel_gauge, false);
		charger_set_term(manager->charger, false);
		charger_set_chg(manager->charger, false);
		msleep(300);
		charger_set_chg(manager->charger, true);
		msleep(300);
		charger_set_term(manager->charger, true);

		rechg_flag = false;
		hq_info("debug_set_rechg rechg_flag = %d \n",rechg_flag);

		is_full_votable = find_votable("IS_FULL");
		if (!is_full_votable) {
			pr_err("%s failed to get is_full_votable\n", __func__);
		} else {
			vote(is_full_votable, SMOOTH_NEW_VOTER, false, 0);
		}
	}
}

static void charger_manager_monitor(struct charger_manager *manager)
{
	union power_supply_propval pval = {0,};
	int ret = 0;
	uint32_t adc_buf_len = 0;
	uint8_t i = 0;
	char adc_buf[MIAN_CHG_ADC_LENGTH + 1];
	uint32_t iterm = 0;
	uint32_t fv = 0;
	uint32_t ibus = 0;
	int ichg = 0;
	bool charge_en = 0;

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret < 0)
		hq_err("get battery soc error.\n");
	else
		manager->soc = pval.intval;

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	if (ret < 0)
		hq_err("get battery volt error.\n");
	else
		manager->vbat = pval.intval / 1000;

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	if (ret < 0)
		hq_err("get battery current error.\n");
	else
		manager->ibat = pval.intval;

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret < 0)
		pr_err("get battery current error.\n");
	else
		manager->tbat = pval.intval;

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_STATUS, &pval);
	if (ret < 0)
		hq_err("get charge status error.\n");
	else
		manager->chg_status = pval.intval;

	charger_get_term_curr(manager->charger, &iterm);

	charger_get_term_volt(manager->charger, &fv);

	charge_en = charger_get_chg(manager->charger);

	charger_get_ichg(manager->charger, &ichg);

	charger_get_input_curr_lmt(manager->charger, &ibus);

	hq_info("[Battery] soc= %d, ibat = %d, vbat = %d, tbat = %d\n",
				manager->soc, manager->ibat, manager->vbat, manager->tbat);
	hq_info("[Chg_reg] ibus = %d, ichg = %d, charge_en = %d, iterm = %d, fv = %d\n",
				ibus, ichg, charge_en, iterm, fv);

	power_supply_changed(manager->usb_psy);
	power_supply_changed(manager->batt_psy);

	charger_manager_keep_uisoc(manager);

	for (i = 0; i < ADC_GET_MAX; i++) {
		ret = charger_get_adc(manager->charger, i, &manager->chg_adc[i]);
		if (ret < 0) {
			hq_info("get adc failed\n");
			continue;
		}
		adc_buf_len += sprintf(adc_buf + adc_buf_len,
						"%s : %d,", adc_name[i], manager->chg_adc[i]);
	}

	if (adc_buf_len > MIAN_CHG_ADC_LENGTH)
		adc_buf[MIAN_CHG_ADC_LENGTH] = '\0';
	hq_info("%s\n", adc_buf);
}

/* N6R code for HQ-429228 at 2024/11/08 by p-mazhuang3 start */
static void low_vbat_power_off(struct charger_manager *manager)
{
  	int rc = 0;
  	static int count = 0;
  
  	if (!manager->charger) {
  		hq_err("failed to master_charge device\n");
  		return;
  	}
  
  	if ((manager->vbat < (SHUTDOWN_DELAY_VOL_LOW - 50)) && manager->vbat > 2700) {
 		if (count < 3) {
  			count ++;
  			hq_info("count is =%d\n", count);
  		} else {
  			rc = charger_reset(manager->charger);
  			if (rc < 0)
  				hq_err("main chg reset failed.\n");
  			msleep(1000);
  			hq_info("vbat under 3.25V, poweroff. vbat=%d\n", manager->vbat);
  			kernel_power_off();
  		}
 	} else {
  		count = 0;
  	}
}
/* N6R code for HQ-429228 at 2024/11/08 by p-mazhuang3 end */

static void power_off_check_work(struct charger_manager *manager)
{
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	int rc = 0;
#endif

	static char uevent_string[][MAX_UEVENT_LENGTH + 1] = {
		"POWER_SUPPLY_SHUTDOWN_DELAY=\n", //28
	};

	static char *envp[] = {
		uevent_string[0],
		NULL,
	};
	
/* N6R code for HQ-429228 at 2024/11/08 by p-mazhuang3 start */
	low_vbat_power_off(manager);
/* N6R code for HQ-429228 at 2024/11/08 by p-mazhuang3 end */

#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	rc = fuel_gauge_check_i2c_function(manager->fuel_gauge);
	if (manager->soc == 1 || (rc && manager->vbat)) {
#else
	if (manager->soc == 1) {
#endif
		if ((manager->vbat >= SHUTDOWN_DELAY_VOL_LOW && manager->vbat < SHUTDOWN_DELAY_VOL_HIGH)
			&& manager->chg_status != POWER_SUPPLY_STATUS_CHARGING){
				manager->shutdown_delay = true;
		} else if (manager->chg_status == POWER_SUPPLY_STATUS_CHARGING
						&& manager->shutdown_delay) {
				manager->shutdown_delay = false;
		} else {
			manager->shutdown_delay = false;
		}
	} else {
		manager->shutdown_delay = false;
	}

	if (manager->last_shutdown_delay != manager->shutdown_delay) {
		manager->last_shutdown_delay = manager->shutdown_delay;
		power_supply_changed(manager->usb_psy);
		power_supply_changed(manager->batt_psy);
		if (manager->shutdown_delay == true)
			strncpy(uevent_string[0] + 28, "1", MAX_UEVENT_LENGTH - 28);
		else
			strncpy(uevent_string[0] + 28, "0", MAX_UEVENT_LENGTH - 28);
		mdelay(1000);
		hq_err("envp[0] = %s\n", envp[0]);
		kobject_uevent_env(&manager->dev->kobj, KOBJ_CHANGE, envp);
	}

}

static int charger_manager_check_vindpm(struct charger_manager *manager, uint32_t vbat)
{
	struct charger_dev *charger = manager->charger;
	int ret = 0;
#if CHARGER_VINDPM_USE_DYNAMIC
	if (vbat < CHARGER_VINDPM_DYNAMIC_BY_VBAT1) {
		ret = charger_set_input_volt_lmt(charger, CHARGER_VINDPM_DYNAMIC_VALUE1);
	} else if (vbat < CHARGER_VINDPM_DYNAMIC_BY_VBAT2) {
		ret = charger_set_input_volt_lmt(charger, CHARGER_VINDPM_DYNAMIC_VALUE2);
	} else {
		ret = charger_set_input_volt_lmt(charger, CHARGER_VINDPM_DYNAMIC_VALUE3);
	}
#else
	ret = charger_set_input_volt_lmt(charger, CHARGER_VINDPM_DYNAMIC_VALUE3);
#endif

	if (ret < 0){
		hq_err("Failed to set vindpm, ret = %d\n", ret);
		return ret;
	}
	return 0;
}

static int charger_manager_check_iindpm(struct charger_manager *manager, uint32_t vbus_type)
{
	int ret = 0;
	int ichg_ma = 0;
	int icl_ma = 0;

	switch (vbus_type) {
	case VBUS_TYPE_FLOAT:
		ichg_ma = manager->float_current;
		icl_ma = manager->float_current;
		break;
	case VBUS_TYPE_NONE:
		ichg_ma = 100;
		icl_ma = 100;
		break;
	case VBUS_TYPE_SDP:
		ichg_ma = manager->usb_current;
		icl_ma = manager->usb_current;
		break;
	case VBUS_TYPE_NON_STAND:
		ichg_ma = manager->float_current;
		icl_ma = manager->float_current;
		break;
	case VBUS_TYPE_CDP:
		ichg_ma = manager->cdp_current;
		icl_ma = manager->cdp_current;
		break;
	case VBUS_TYPE_DCP:
		ichg_ma = manager->dcp_current;
		icl_ma = manager->dcp_current;
		break;
	case VBUS_TYPE_HVDCP:
		ichg_ma = manager->hvdcp_charge_current;
		icl_ma = manager->hvdcp_input_current;
		break;
	case VBUS_TYPE_HVDCP_3:
	case VBUS_TYPE_HVDCP_3P5:
		ichg_ma = manager->hvdcp3_charge_current;
		icl_ma = manager->hvdcp3_input_current;
		break;
	default:
		ichg_ma = manager->usb_current;
		icl_ma = manager->usb_current;
		break;
	}

	if ((manager->pd_active == CHARGE_PD_ACTIVE && vbus_type) || manager->pd_active == 10) {
		hq_err("manager->pd_active == 10\n");
		if (manager->pd_volt_max == 5000) {  //C-to-C
			ichg_ma = manager->pd_curr_max;
			icl_ma = manager->pd_curr_max;
			hq_err("c-to-c ichg_ma = %d\n", ichg_ma);		
		} else {  //PD2.0
			ichg_ma = manager->pd_curr_max * PD20_ICHG_MULTIPLE / 1000;  //1.8 of fixed current
		/* N6R code for HQ-429228 at 2024/11/07 by p-mazhuang3 start */                  
                  	manager->pd_curr_max = min(manager->pd_curr_max, 2000);
                /* N6R code for HQ-429228 at 2024/11/07 by p-mazhuang3 start */
			icl_ma = manager->pd_curr_max;
			hq_err("fixed current ichg_ma = %d\n", ichg_ma);
		}
	}

	if (is_mtbf_mode_func() && (vbus_type == VBUS_TYPE_SDP || vbus_type == VBUS_TYPE_CDP)) {
		ichg_ma = MTBF_CURRENT;
		icl_ma = MTBF_CURRENT;
		hq_info("is_mtbf_mode=%d icl=%d ichg=%d\n", is_mtbf_mode_func(), icl_ma, ichg_ma);
	}

	if (IS_ERR_OR_NULL(manager->main_icl_votable)) {
		hq_err("main_icl_votable not found\n");
		return PTR_ERR(manager->main_icl_votable);
	} else
		vote(manager->main_icl_votable, CHARGER_TYPE_VOTER, true, icl_ma);

	if (IS_ERR_OR_NULL(manager->main_fcc_votable)) {
		hq_err("main_fcc_votable not found\n");
		return PTR_ERR(manager->main_fcc_votable);
	} else
		vote(manager->main_fcc_votable, CHARGER_TYPE_VOTER, true, ichg_ma);

	return ret;
}

static void charger_manager_timer_func(struct timer_list *timer)
{
	struct charger_manager *manager = container_of(timer,
							struct charger_manager, charger_timer);
	charger_manager_wake_thread(manager);
}

int charger_manager_start_timer(struct charger_manager *manager, uint32_t ms)
{
	del_timer(&manager->charger_timer);
	manager->charger_timer.expires = jiffies + msecs_to_jiffies(ms);
	manager->charger_timer.function = charger_manager_timer_func;
	add_timer(&manager->charger_timer);
	return 0;
}
EXPORT_SYMBOL(charger_manager_start_timer);

static int reset_vote(struct charger_manager *manager)
{
	vote(manager->main_fcc_votable, MAIN_FCC_MAX_VOTER, false, 0);
	vote(manager->main_icl_votable, MAIN_FCC_MAX_VOTER, false, 0);
	vote(manager->total_fcc_votable, JEITA_VOTER, false, 0);
	vote(manager->total_fcc_votable, NORMAL_PPS_VOTER, false, 0);
/* N6R code for HQ-429228 at 2024/11/08 by p-mazhuang3 start */
	vote(manager->main_icl_votable, PD_SINK_VBUS_VOTER, false, 0);
/* N6R code for HQ-429228 at 2024/11/08 by p-mazhuang3 end */
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	vote(manager->main_fcc_votable, FG_I2C_ERR, false, 0);
	vote(manager->main_icl_votable, FG_I2C_ERR, false, 0);
#endif
	return 0;
}

static int rerun_vote(struct charger_manager *manager)
{
	rerun_election(manager->main_chg_disable_votable);
	rerun_election(manager->total_fcc_votable);
	rerun_election(manager->main_fcc_votable);
	rerun_election(manager->main_icl_votable);
	return 0;
}

static void apsd_second_detect_work(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
					struct charger_manager, second_detect_work.work);
	hq_info("apsd enter!\n");
	charger_force_dpdm(manager->charger);
}

#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
static bool get_usb_ready(struct charger_manager *manager)
{
	bool ready = true;

	if (IS_ERR_OR_NULL(manager->usb_node))
		manager->usb_node = of_parse_phandle(manager->dev->of_node, "usb", 0);
	if (!IS_ERR_OR_NULL(manager->usb_node)) {
		ready = !of_property_read_bool(manager->usb_node, "cdp-block");
		if (ready || manager->get_usb_rdy_cnt % 10 == 0)
			hq_info("usb ready = %d\n", ready);
	} else
		hq_err("usb node missing or invalid\n");

	if (ready == false && (manager->get_usb_rdy_cnt >= WAIT_USB_RDY_MAX_CNT || manager->pd_active)) {
		if (manager->pd_active)
			manager->get_usb_rdy_cnt = 0;
		hq_info("cdp-block timeout or pd adapter\n");
		return true;
	}

	return ready;
}

static void wait_usb_ready_work(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
					struct charger_manager, wait_usb_ready_work.work);

	if (get_usb_ready(manager) || manager->get_usb_rdy_cnt >= WAIT_USB_RDY_MAX_CNT)
		charger_force_dpdm(manager->charger);
	else {
		manager->get_usb_rdy_cnt++;
		schedule_delayed_work(&manager->wait_usb_ready_work, msecs_to_jiffies(WAIT_USB_RDY_TIME));
	}
}
#endif

static void charger_manager_charger_type_detect(struct charger_manager *manager)
{
	struct charger_dev *charger = manager->charger;
	struct chargerpump_dev *master_cp_chg = manager->master_cp_chg;
	struct chargerpump_dev *slave_cp_chg = manager->slave_cp_chg;

	static int float_count = 0;

	charger_get_online(manager->charger, &manager->usb_online);
	charger_get_vbus_type(manager->charger, &manager->vbus_type);

	if (manager->usb_online != manager->adapter_plug_in) {
		manager->adapter_plug_in = manager->usb_online;
		if (manager->adapter_plug_in) {
			pm_stay_awake(manager->dev);
			hq_info("adapter plug in\n");
			if (IS_ERR_OR_NULL(manager->main_icl_votable)) {
				hq_info("main_icl_votable is NULL\n");
			} else {
				vote(manager->main_icl_votable, CHARGER_TYPE_VOTER, true, 100);
				hq_info("main_icl_votable icl 100\n");
			}
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
                        if ((manager->soc <= 20) && (manager->thermal_board_temp <= 390)) {
                                manager->low_fast_plugin_flag = true;
                        }
			schedule_delayed_work(&manager->xm_charge_work, msecs_to_jiffies(3000));
#endif
#ifdef PRODUCT_EU_50W
			schedule_delayed_work(&manager->normal_adapter_detect_plugin_lower_fcc_work, msecs_to_jiffies(180000));
			hq_info("static EU country normal adapter end\n");
#else
			if (manager->charger_region) {
				schedule_delayed_work(&manager->normal_adapter_detect_plugin_lower_fcc_work, msecs_to_jiffies(180000));
				hq_info("auto 9 country normal adapter end\n");
			}
#endif

			manager->qc_detected = false;
			charger_adc_enable(charger, true);
			chargerpump_set_enable_adc(master_cp_chg, true);
			chargerpump_set_enable_adc(slave_cp_chg, true);
			charger_set_term(charger, true);
			rerun_vote(manager);
		} else {
			chargerpump_set_enable_adc(master_cp_chg, false);
			chargerpump_set_enable_adc(slave_cp_chg, false);
			charger_adc_enable(charger, false);
			
			hq_info("adapter plug out\n");			
			if (IS_ERR_OR_NULL(manager->main_icl_votable)) {
				hq_info("main_icl_votable is NULL\n");
			} else {
				vote(manager->main_icl_votable, CHARGER_TYPE_VOTER, true, 100);
				hq_info("main_icl_votable icl 100\n");
			}
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
			manager->smart_ctrl_en = false;
			cancel_delayed_work(&manager->xm_charge_work);
                        manager->low_fast_plugin_flag = false;
                        manager->pps_fast_mode = false;
                        manager->b_flag = NORMAL;
#endif
#ifdef PRODUCT_EU_50W
			cancel_delayed_work_sync(&manager->normal_adapter_detect_plugin_lower_fcc_work);
			manager->charge_limit = false;
#else
			if (manager->charger_region) {
				cancel_delayed_work_sync(&manager->normal_adapter_detect_plugin_lower_fcc_work);
				manager->charge_limit = false;
			}
#endif
			cancel_delayed_work_sync(&manager->second_detect_work);
			float_count = 0;
			fuel_gauge_set_fastcharge_mode(manager->fuel_gauge, false);
			chargerpump_policy_stop(g_policy);
			reset_vote(manager);
			pm_relax(manager->dev);
		}
	}

	hq_info("usb_online= %d, bc_type = %s, input_suspend = %d, pd_active = %d\n",
				manager->usb_online, bc12_result[manager->vbus_type], manager->input_suspend, manager->pd_active);

	if (!manager->adapter_plug_in)
		return;

	if (!manager->is_pr_swap) {
		switch (manager->vbus_type) {
			case VBUS_TYPE_NONE:
				charger_force_dpdm(charger);
				break;
			case VBUS_TYPE_NON_STAND:
			case VBUS_TYPE_FLOAT:
				if (float_count <= 3 && manager->pd_active != 1) {
					hq_info("float type!\n");
					schedule_delayed_work(&manager->second_detect_work, msecs_to_jiffies(FLOAT_DELAY_TIME));
					float_count++;
				}
				rerun_election(manager->main_icl_votable);
				break;
			case VBUS_TYPE_SDP:
				#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
				if (!get_usb_ready(manager)) {
					if (manager->get_usb_rdy_cnt == 0)
						schedule_delayed_work(&manager->wait_usb_ready_work, msecs_to_jiffies(0));
				}
				#endif
				break;
			default:
				break;
		}
	} else
		manager->vbus_type = VBUS_TYPE_FLOAT;

	if (manager->vbus_type == VBUS_TYPE_SDP || manager->vbus_type == VBUS_TYPE_CDP)
		manager->usb_psy_desc.type = POWER_SUPPLY_TYPE_USB;
	else
		manager->usb_psy_desc.type = POWER_SUPPLY_TYPE_USB_PD;

#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	if (charger_monitor_fg_i2c_status(manager)) {
		hq_info("fg i2c error\n");
}
#endif

	if (manager->pd_contract_update) {
		manager->pd_contract_update = false;
		if (g_policy->state != POLICY_RUNNING)
			chargerpump_policy_stop(g_policy);
	}

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	if ((manager->pd_active == CHARGE_PD_PPS_ACTIVE) && (g_policy->state == POLICY_NO_START) && (!manager->smart_ctrl_en)) {
		chargerpump_policy_start(g_policy);
	}
#else
	if ((manager->pd_active == CHARGE_PD_PPS_ACTIVE) && (g_policy->state == POLICY_NO_START))
		chargerpump_policy_start(g_policy);
#endif

	if (g_policy->state == POLICY_RUNNING)
		return;

	if (manager->vbus_type == VBUS_TYPE_DCP && !manager->qc_detected && !manager->pd_active) {
		manager->qc_detected = true;
		charger_qc_identify(charger, manager->qc3_mode);
	}

	charger_manager_check_vindpm(manager, manager->chg_adc[ADC_GET_VBAT]);
	charger_manager_check_iindpm(manager, manager->vbus_type);

	hq_info("end!\n");
}

static int charger_manager_thread_fn(void *data)
{
	struct charger_manager *manager = data;
	int ret = 0;

	while (true) {
		ret = wait_event_interruptible(manager->wait_queue,
							manager->run_thread);
		if (kthread_should_stop() || ret) {
			hq_err("exits(%d)\n", ret);
			break;
		}

		manager->run_thread = false;

		charger_manager_monitor(manager);

		charger_manager_charger_type_detect(manager);

		power_off_check_work(manager);

		if (!manager->adapter_plug_in)
			charger_manager_start_timer(manager, CHARGER_MANAGER_LOOP_TIME_OUT);
		else
			charger_manager_start_timer(manager, CHARGER_MANAGER_LOOP_TIME);
	}
	return 0;
}

static int charger_manager_notifer_call(struct notifier_block *nb, unsigned long event, void *data)
{
	struct charger_manager *manager = container_of(nb,
							struct charger_manager, charger_nb);
	charger_manager_wake_thread(manager);

	return NOTIFY_OK;
}

static void charger_manager_check_dev(struct charger_manager *manager)
{

	if(IS_ERR_OR_NULL(manager)) {
		hq_err("manager is err or null\n");
	}

	manager->charger = charger_find_dev_by_name("primary_chg");
	if (!manager->charger)
		hq_err("failed to master_charge device\n");

	manager->master_cp_chg = chargerpump_find_dev_by_name("master_cp_chg");
	if (!manager->master_cp_chg)
		hq_err("failed to master_cp_chg device\n");

	manager->slave_cp_chg = chargerpump_find_dev_by_name("slave_cp_chg");
	if (!manager->slave_cp_chg)
		hq_err("failed to slave_cp_chg device\n");

	manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
	if (!manager->fuel_gauge)
		hq_err("failed to fuel_gauge device\n");

	manager->cp_master_psy = power_supply_get_by_name("sc-cp-master");
	if (!manager->cp_master_psy)
		hq_err("failed to cp_master_psy\n");

	manager->cp_slave_psy = power_supply_get_by_name("sc-cp-slave");
	if (!manager->cp_slave_psy)
		hq_err("failed to cp_slave_psy\n");

	manager->fg_psy = power_supply_get_by_name("bms");
	if (IS_ERR_OR_NULL(manager->fg_psy))
		hq_err("failed to get bms psy\n");

#if IS_ENABLED(CONFIG_TCPC_CLASS)
	manager->tcpc = tcpc_dev_get_by_name("type_c_port0");
	if (!manager->tcpc)
		hq_err("get tcpc dev failed\n");
#endif

#if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
	manager->pd_adapter = get_adapter_by_name("pd_adapter");
	if (!manager->pd_adapter)
		hq_err("failed to pd_adapter\n");
#endif
}

static int charger_manager_parse_dts(struct charger_manager *manager)
{
	struct device_node *node = manager->dev->of_node;
	struct device_node *of_chosen;
	char *bootargs;

	int ret = false;
	int size, i;
	int batt_temp[TEMP_LEVEL_MAX] = { 250, };
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
        int retv = false;
#endif

	ret = of_property_get_array(manager->dev, "hq_chg_manager,battery_temp_level", &size, &manager->battery_temp);
	if(ret < 0){
		manager->battery_temp = batt_temp;
		hq_err("battery_temp user default, ret = %d\n", ret);
	}else{
		if(size > TEMP_LEVEL_MAX) {
			manager->battery_temp = batt_temp;
			hq_err("battery_temp user default, size = %d\n", size);
		}else{
			for(i = 0; i < TEMP_LEVEL_MAX; i++){
				pr_info("battery_temp level%d = %d\n", i, manager->battery_temp[i]);
			}
		}
	}

	of_chosen = of_find_node_by_path("/chosen");
	if (of_chosen) {
		bootargs = (char *)of_get_property(of_chosen, "bootargs", NULL);
		if (bootargs) {
			if (strstr(bootargs, "charger.region=1")) {
				manager->charger_region = true;
			} else {
				manager->charger_region = false; 
			}
		}
	}
	pr_info("charger_region = %d\n",manager->charger_region);
	manager->cp_master_use = of_property_read_bool(node, "chargerpump,master");
	manager->cp_slave_use = of_property_read_bool(node, "chargerpump,slave");

	manager->en_floatgnd = of_property_read_bool(node, "hq_chg_manager,en_floatgnd");
	hq_info("cp master:slave %d:%d, manager en_floatgnd is %d\n", manager->cp_master_use, manager->cp_slave_use, manager->en_floatgnd);

	ret |= of_property_read_u32(node, "hq_chg_manager,QC3_mode", &manager->qc3_mode);
	ret |= of_property_read_u32(node, "hq_chg_manager,usb_charger_current", &manager->usb_current);
	ret |= of_property_read_u32(node, "hq_chg_manager,float_charger_current", &manager->float_current);
	ret |= of_property_read_u32(node, "hq_chg_manager,ac_charger_current", &manager->dcp_current);
	ret |= of_property_read_u32(node, "hq_chg_manager,cdp_charger_current", &manager->cdp_current);
	ret |= of_property_read_u32(node, "hq_chg_manager,hvdcp_charger_current", &manager->hvdcp_charge_current);
	ret |= of_property_read_u32(node, "hq_chg_manager,hvdcp_input_current", &manager->hvdcp_input_current);
	ret |= of_property_read_u32(node, "hq_chg_manager,hvdcp3_charger_current", &manager->hvdcp3_charge_current);
	ret |= of_property_read_u32(node, "hq_chg_manager,hvdcp3_input_current", &manager->hvdcp3_input_current);
	ret |= of_property_read_u32(node, "hq_chg_manager,pd2_charger_current", &manager->pd2_charge_current);
	ret |= of_property_read_u32(node, "hq_chg_manager,pd2_input_current", &manager->pd2_input_current);
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
        retv = of_property_read_u32_array(node, "hq_chg_manager,cyclecount", manager->cyclecount, CYCLE_COUNT_MAX);
        if(retv){
                pr_err("use default CYCLE_COUNT: 0\n");
                for(i = 0; i < CYCLE_COUNT_MAX; i++)
                        manager->cyclecount[i] = 0;
        }
        ret |= retv;
        retv = of_property_read_u32_array(node, "hq_chg_manager,dropfv", manager->dropfv, CYCLE_COUNT_MAX);
        if(retv){
                pr_err("use default DROP_FV: 0\n");
                for(i = 0; i < CYCLE_COUNT_MAX; i++)
                        manager->dropfv[i] = 0;
        }
        ret |= retv;
#endif

	if (ret)
		return false;
	else
		return true;
}

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
static int charger_thermal_notifier_event(struct notifier_block *notifier,
			unsigned long chg_event, void *val)
{
	struct charger_manager *manager = container_of(notifier,
							struct charger_manager, charger_thermal_nb);

	switch (chg_event) {
	case THERMAL_BOARD_TEMP:
		manager->thermal_board_temp = *(int *)val;
		pr_info("%s: get thermal_board_temp: %d\n", __func__, manager->thermal_board_temp);
		break;
	default:
		pr_err("%s: not supported charger notifier event: %d\n", __func__, chg_event);
		break;
	}

	return NOTIFY_DONE;
}

static int screen_state_for_charger_callback(struct notifier_block *nb,
                                            unsigned long val, void *v)
{
        int blank = *(int *)v;
        struct charger_manager *manager = container_of(nb, struct charger_manager, sm.charger_panel_notifier);

        if (!(val == MTK_DISP_EARLY_EVENT_BLANK|| val == MTK_DISP_EVENT_BLANK)) {
                pr_err("%s event(%lu) do not need process\n", __func__, val);
                return NOTIFY_OK;
        }

        switch (blank) {
        case MTK_DISP_BLANK_UNBLANK: //power on
                manager->sm.screen_state = 0;
                pr_info("%s screen_state = %d\n", __func__, manager->sm.screen_state);
                break;
        case MTK_DISP_BLANK_POWERDOWN: //power off
                manager->sm.screen_state = 1;
                pr_info("%s screen_state = %d\n", __func__, manager->sm.screen_state);
                break;
        }
        return NOTIFY_OK;
}
#endif

static int charger_manager_probe(struct platform_device *pdev)
{
	struct charger_manager *manager;
	int ret = 0;

	hq_info("running (%s)\n", CHARGER_MANAGER_VERSION);

	manager = devm_kzalloc(&pdev->dev, sizeof(*manager), GFP_KERNEL);
	if (!manager)
		return -ENOMEM;

	manager->dev = &pdev->dev;
	platform_set_drvdata(pdev, manager);

	charger_manager_check_dev(manager);

	ret = charger_manager_parse_dts(manager);
	if (!ret)
		hq_err("charger_manager_parse_dts failed\n");

	charger_manager_create_votable(manager);

	charger_manager_usb_psy_register(manager);

	charger_manager_batt_psy_register(manager);

	hq_jeita_init(manager->dev);

	if (!IS_ERR_OR_NULL(manager->usb_psy)) {
		ret = hq_usb_sysfs_create_group(manager);
		if (ret < 0)
			hq_err("create some usb nodes failed\n");
	}
	ret = charge_manager_thermal_init(manager);
	if (ret < 0)
		hq_err("charge_manager_thermal_init failed, ret = %d\n", ret);

	if (!IS_ERR_OR_NULL(manager->batt_psy)) {
		ret = hq_batt_sysfs_create_group(manager);
		if (ret < 0)
			hq_err("create some batt nodes failed\n");
	}

	init_waitqueue_head(&manager->wait_queue);
	manager->charger_nb.notifier_call = charger_manager_notifer_call;
	charger_register_notifier(&manager->charger_nb);

	INIT_DELAYED_WORK(&manager->normal_adapter_detect_plugin_lower_fcc_work, normal_adapter_detect_plugin_lower_fcc_work);

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	INIT_DELAYED_WORK(&manager->xm_charge_work, xm_charge_work);
	manager->smart_ctrl_en = false;
#endif

	INIT_DELAYED_WORK(&manager->second_detect_work, apsd_second_detect_work);
#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
	INIT_DELAYED_WORK(&manager->wait_usb_ready_work, wait_usb_ready_work);
#endif

#if IS_ENABLED(CONFIG_TCPC_CLASS)
	manager->pd_nb.notifier_call = charger_manager_tcpc_notifier_call;
	if (IS_ERR_OR_NULL(manager->tcpc)) {
		hq_err("manager->tcpc is null\n");
	} else {
		ret = register_tcp_dev_notifier(manager->tcpc, &manager->pd_nb,
								TCP_NOTIFY_TYPE_ALL);
		if (ret < 0) {
			hq_err("register tcpc notifier fail(%d)\n", ret);
			return ret;
		}
	}
	
	alarm_init(&manager->rust_det_work_timer, ALARM_BOOTTIME,rust_det_work_timer_handler);
	INIT_DELAYED_WORK(&manager->hrtime_otg_work, hrtime_otg_work);
	INIT_DELAYED_WORK(&manager->set_cc_drp_work, set_cc_drp_work);
	manager->cid_status = false;
#endif

	device_init_wakeup(manager->dev, true);

	manager->run_thread = true;
	manager->thread = kthread_run(charger_manager_thread_fn, manager,
								"charger_manager_thread");

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
        manager->thermal_board_temp = 250;
        manager->charger_thermal_nb.notifier_call = charger_thermal_notifier_event;
	charger_thermal_reg_notifier(&manager->charger_thermal_nb);
        manager->sm.charger_panel_notifier.notifier_call = screen_state_for_charger_callback;
        ret = mtk_disp_notifier_register("screen state", &manager->sm.charger_panel_notifier);
        if (ret) {
               hq_err("register screen state callback fail(%d)\n", ret);
               return ret;
        }
        manager->smart_batt = 0;
        manager->night_charging = false;
        manager->night_charging_flag = false;
        manager->fv_againg = 0;
        manager->low_fast_plugin_flag = false;
        manager->pps_fast_mode = false;
        manager->b_flag = NORMAL;
#endif
	hq_info("success\n");
	return 0;
}

static int charger_manager_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct charger_manager *manager = platform_get_drvdata(pdev);

	hq_jeita_deinit();
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	cancel_delayed_work(&manager->xm_charge_work);
#endif

#if IS_ENABLED(CONFIG_TCPC_CLASS)
	ret = unregister_tcp_dev_notifier(manager->tcpc, &manager->pd_nb,
					  TCP_NOTIFY_TYPE_ALL);
	if (ret < 0)
		hq_err("unregister tcpc notifier fail(%d)\n", ret);
#endif
	charger_unregister_notifier(&manager->charger_nb);
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
        charger_thermal_unreg_notifier(&manager->charger_thermal_nb);
        ret = mtk_disp_notifier_unregister(&manager->sm.charger_panel_notifier);
        if (ret < 0)
		hq_err("unregister screen state notifier fail(%d)\n", ret);
#endif
	return 0;
}

static const struct of_device_id charger_manager_match[] = {
	{.compatible = "huaqin,hq_chg_manager",},
	{},
};
MODULE_DEVICE_TABLE(of, charger_manager_match);

static void charger_manager_shutdown(struct platform_device *pdev)
{
	int ret = 0;
	struct charger_manager *manager = platform_get_drvdata(pdev);
	struct charger_dev *charger = charger_find_dev_by_name("primary_chg");
	struct chargerpump_dev *master_cp_chg = chargerpump_find_dev_by_name("master_cp_chg");
	struct chargerpump_dev *slave_cp_chg = chargerpump_find_dev_by_name("slave_cp_chg");
	struct tcpc_device *tcpc = tcpc_dev_get_by_name("type_c_port0");

	charger_adc_enable(charger, false);
	if (charger->shipmode_flag) {
		shipmode_shutdown_notifier_call_chain(0x1, NULL);
		charger_reset(charger);
		tcpci_pd_reset(tcpc);
		chargerpump_set_enable_adc(master_cp_chg, false);
		chargerpump_set_enable_adc(slave_cp_chg, false);
		charger->shipmode_flag = false;
		if (manager->tcpc->ops->set_dis_vconn_ov) {
			manager->tcpc->ops->set_dis_vconn_ov(manager->tcpc,true);
			pr_info("set ship mode TCPC_V10_REG_FAULT_CTRL_DIS_VCONN_OV \n");
		}

		ret = charger_set_shipmode(charger, true);
		if (ret < 0)
			pr_err("set ship mode fail\n");
		else
			pr_err("set ship mode success\n");
	}
}

static struct platform_driver charger_manager_driver = {
	.probe = charger_manager_probe,
	.remove = charger_manager_remove,
	.shutdown	= charger_manager_shutdown,
	.driver = {
		.name = "hq_chg_manager",
		.of_match_table = charger_manager_match,
	},
};

static int __init charger_manager_init(void)
{
	hq_err("---->\n");
	return platform_driver_register(&charger_manager_driver);
}

late_initcall(charger_manager_init);

MODULE_DESCRIPTION("Huaqin Charger Manager Core");
MODULE_LICENSE("GPL v2");
/*
 * HQ main_chg Release Note
 *
 *
 * 1.1.1
 * (1)Add logic to not handle negative values at main_fcc, main_icl and total_fcc
 *
 * 1.1.0
 * (1)Modify usb_psy register：replace usb_psy_desc with manager->usb_psy_desc to register usb_psy
 *
 * 1.0.9
 * (1)encapsulate two APIs : charger_manager_monitor and charger_manager_charger_type_detect
 * (2)Add low_vbat_power_off_check
 *
 * 1.0.8
 * (1) Separate sysfs[psy and other]: creat sysfs in hq_charge_sysfs.c
 *
 * 1.0.7
 * (1) Add hq_thermal func: charge_manager_thermal_init
 *
 * 1.0.6
 * (1) Add hq_jeita func: hq_jeita_init
 *
 * 1.0.5
 * (1) Do not let the system sleep during charging:pm_stay_awake/pm_relax
 * (2) Start qc detection, it must PD not active
 * (3) Start cp policy sm, need PD active=2
 * (4) Add new tcpc notifier: handle tcpc event
 *
 * 1.0.4
 * (1) Add usb psy new node: real_type, typec_orientation, otg and input_suspend
 *
 * 1.0.3
 * (1) Add usb psy new node: real_type, typec_orientation, otg and input_suspend
 *
 * 1.0.2
 * (1) Add hq voter policy: charger_manager_create_votable
 *
 * 1.0.1
 * (1) Add battery psy node: charger_manager_batt_psy_register
 *
 * 1.0.0
 * (1) Add driver for hq charger manager
 * (2) charger_manager_thread_fn todo : 1)BC12 detect 2)cp policy entry 3)power off check .etc
 */
