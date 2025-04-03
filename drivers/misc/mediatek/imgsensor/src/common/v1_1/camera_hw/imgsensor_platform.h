/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __IMGSENSOR_PLATFORM_H__
#define __IMGSENSOR_PLATFORM_H__


enum IMGSENSOR_HW_ID {
	IMGSENSOR_HW_ID_MCLK,
	IMGSENSOR_HW_ID_REGULATOR,
	IMGSENSOR_HW_ID_GPIO,
/* N6 code for HQ-309545 by huabinchen at 2023/07/05 start */
	IMGSENSOR_HW_ID_SMARTLDO,
/* N6 code for HQ-309545 by huabinchen at 2023/07/05 end */
	IMGSENSOR_HW_ID_MAX_NUM,
	IMGSENSOR_HW_ID_NONE = -1
};
#endif
