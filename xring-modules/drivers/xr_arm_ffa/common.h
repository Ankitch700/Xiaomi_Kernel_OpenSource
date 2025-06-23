/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 ARM Ltd.
 */

#ifndef _FFA_COMMON_H
#define _FFA_COMMON_H

#include "arm_ffa.h"
#include <linux/arm-smccc.h>
#include <linux/err.h>

typedef struct arm_smccc_1_2_regs ffa_value_t;

typedef void (ffa_fn)(ffa_value_t, ffa_value_t *);

int arm_ffa_bus_init(void);
void arm_ffa_bus_exit(void);
bool ffa_device_is_valid(struct ffa_device *ffa_dev);
void ffa_device_match_uuid(struct ffa_device *ffa_dev, const uuid_t *uuid);
int __init ffa_transport_init(ffa_fn **invoke_ffa_fn);

#endif /* _FFA_COMMON_H */
