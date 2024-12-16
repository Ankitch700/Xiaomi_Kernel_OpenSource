// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
 */

#ifndef __LINUX_SOUTHCHIP_CHARGER_CLASS_H__
#define __LINUX_SOUTHCHIP_CHARGER_CLASS_H__

#include "../charger_policy/adc_channel_def.h"

enum vbus_type {
	VBUS_TYPE_NONE = 0,
	VBUS_TYPE_SDP,
	VBUS_TYPE_CDP,
	VBUS_TYPE_DCP,
	VBUS_TYPE_HVDCP,
	VBUS_TYPE_UNKNOWN,
	VBUS_TYPE_NON_STAND,
	VBUS_TYPE_HVDCP_3,
	VBUS_TYPE_HVDCP_3P5,
	VBUS_TYPE_PD,
};

enum charger_notifer {
	CHARGER_NOTIFER_INT = 0,
};

struct charger_dev;
struct charger_ops {
	int (*get_adc)(struct charger_dev *, enum sc_adc_channel, uint32_t *);
	int (*get_vbus_type)(struct charger_dev *, enum vbus_type *);
	int (*get_online)(struct charger_dev *, bool *);
	int (*is_charge_done)(struct charger_dev *, bool *);
	int (*get_hiz_status)(struct charger_dev *, bool *);
	int (*get_chg_status1)(struct charger_dev *,int *);
	int (*get_input_volt_lmt)(struct charger_dev *, uint32_t *);
	int (*get_input_curr_lmt)(struct charger_dev *, uint32_t *);
	int (*get_chg_status)(struct charger_dev *, uint32_t *);
    int (*get_chg_en)(struct charger_dev *, bool *);
	int (*get_otg_status)(struct charger_dev *, bool *);
	int (*get_term_curr)(struct charger_dev *, uint32_t *);
	int (*get_term_volt)(struct charger_dev *, uint32_t *);
	int (*set_hiz)(struct charger_dev *, bool);
	int (*set_input_curr_lmt)(struct charger_dev *, int);
	int (*set_input_volt_lmt)(struct charger_dev *, int);
	int (*set_ichg)(struct charger_dev *, int);
	int (*get_ichg)(struct charger_dev *);
	int (*set_chg)(struct charger_dev *, bool);
	int (*set_otg)(struct charger_dev *, bool);
	int (*set_otg_curr)(struct charger_dev *, int);
	int (*set_otg_volt)(struct charger_dev *, int);
	int (*set_term)(struct charger_dev *, bool);
	int (*set_term_curr)(struct charger_dev *, int);
	int (*set_term_volt)(struct charger_dev *, int);
	int (*adc_enable)(struct charger_dev *, bool);
	int (*set_prechg_volt)(struct charger_dev *, int);
	int (*set_prechg_curr)(struct charger_dev *, int);
    int (*set_safety_time_en)(struct charger_dev *, bool);
    int (*is_safety_time_en)(struct charger_dev *, bool *);
	int (*force_dpdm)(struct charger_dev *);
	int (*reset)(struct charger_dev *);
	int (*request_dpdm)(struct charger_dev *, bool);
	int (*set_wd_timeout)(struct charger_dev *, int);
	int (*kick_wd)(struct charger_dev *);
	int (*set_shipmode)(struct charger_dev *, bool);
	int (*set_rechg_vol)(struct charger_dev *, int);
	int (*qc_identify)(struct charger_dev * charger);
	int (*is_sy6976)(struct charger_dev * charger);
};

struct charger_dev {
	struct device dev;
	char *name;
	void *private;
	struct charger_ops *ops;

	bool changed;
	struct mutex changed_lock;
	struct work_struct changed_work;
};

struct charger_dev *charger_find_dev_by_name(const char *name);
struct charger_dev *charger_register(char *name, struct device *parent,
							struct charger_ops *ops, void *private);
void *charger_get_private(struct charger_dev *charger);
int charger_unregister(struct charger_dev *charger);
int charger_register_notifier(struct notifier_block *nb);
void charger_unregister_notifier(struct notifier_block *nb);
void charger_changed(struct charger_dev *charger);

int charger_get_adc(struct charger_dev *charger, enum sc_adc_channel channel, uint32_t *value);
int charger_get_vbus_type(struct charger_dev *charger, enum vbus_type *vbus_type);
int charger_get_online(struct charger_dev *charger, bool *en);
int charger_is_charge_done(struct charger_dev *charger, bool *en);
int charger_get_hiz_status(struct charger_dev *charger, bool *en);
int charger_get_chg_status1(struct charger_dev * charger);
int charger_get_input_volt_lmt(struct charger_dev *charger, uint32_t *mv);
int charger_get_input_curr_lmt(struct charger_dev *charger, uint32_t *ma);
int charger_get_chg_status(struct charger_dev *charger, uint32_t *chg_status);
int charger_get_chg_en(struct charger_dev *charger, bool *en);
int charger_get_otg_status(struct charger_dev *charger, bool *en);
int charger_get_term_curr(struct charger_dev *charger, uint32_t *ma);
int charger_get_term_volt(struct charger_dev *charger, uint32_t *mv);
int charger_set_hiz(struct charger_dev * charger, bool en);
int charger_set_input_curr_lmt(struct charger_dev * charger, int ma);
int charger_set_input_volt_lmt(struct charger_dev * charger, int mv);
int charger_set_ichg(struct charger_dev * charger, int ma);
int charger_get_ichg(struct charger_dev * charger);
int charger_set_chg(struct charger_dev * charger, bool en);
int charger_set_otg(struct charger_dev * charger, bool en);
int charger_set_otg_curr(struct charger_dev * charger, int ma);
int charger_set_otg_volt(struct charger_dev * charger, int mv);
int charger_set_term(struct charger_dev * charger, bool en);
int charger_set_term_curr(struct charger_dev * charger, int ma);
int charger_set_term_volt(struct charger_dev * charger, int mv);
int charger_adc_enable(struct charger_dev * charger, bool en);
int charger_set_prechg_volt(struct charger_dev * charger, int mv);
int charger_set_prechg_curr(struct charger_dev * charger, int ma);
int charger_set_safety_time_en(struct charger_dev *charger, bool en);
int charger_is_safety_time_en(struct charger_dev *charger, bool *en);
int charger_force_dpdm(struct charger_dev * charger);
int charger_reset(struct charger_dev * charger);
int charger_set_wd_timeout(struct charger_dev * charger, int ms);
int charger_kick_wd(struct charger_dev * charger);
int charger_request_qc20(struct charger_dev *charger, int mv);
int charger_qc_identify(struct charger_dev * charger);
int charger_set_shipmode(struct charger_dev * charger, bool en);
int charger_is_sy6976(struct charger_dev * charger);
#endif /* __LINUX_SOUTHCHIP_CHARGER_CLASS_H__ */
