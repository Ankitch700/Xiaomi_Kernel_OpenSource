// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "flashlight-core.h"

/*N6 code for HQ-305606 by xiexinli at 20230710 start*/
const struct flashlight_device_id flashlight_id[] = {
        {0, 0, 0, "flashlights-mt6789", 0, 1},
};
/*N6 code for HQ-305606 by xiexinli at 20230710 end*/
const int flashlight_device_num =
	sizeof(flashlight_id) / sizeof(struct flashlight_device_id);

