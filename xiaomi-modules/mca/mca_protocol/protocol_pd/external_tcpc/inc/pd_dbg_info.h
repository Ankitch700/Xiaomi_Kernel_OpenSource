// SPDX-License-Identifier: GPL-2.0
/*
 *pd_dbg_info.h
 *
 * pd driver
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

#ifndef PD_DBG_INFO_H_INCLUDED
#define PD_DBG_INFO_H_INCLUDED

#include <linux/kernel.h>
#include <linux/module.h>
#include "tcpci_config.h"

#if IS_ENABLED(CONFIG_PD_DBG_INFO)
int pd_dbg_info(const char *fmt, ...);
void pd_dbg_info_lock(void);
void pd_dbg_info_unlock(void);
#else
static inline int pd_dbg_info(const char *fmt, ...)
{
	return 0;
}
static inline void pd_dbg_info_lock(void) {}
static inline void pd_dbg_info_unlock(void) {}
#endif	/* CONFIG_PD_DBG_INFO */

#endif /* PD_DBG_INFO_H_INCLUDED */
