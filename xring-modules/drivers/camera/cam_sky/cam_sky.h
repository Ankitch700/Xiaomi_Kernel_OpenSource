/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */
// MIUI ADD: Camera_CameraSkyNet
#ifndef _CAM_SKYLOG_H_
#define _CAM_SKYLOG_H_
#include <linux/types.h>
#include <linux/kfifo.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>

#define MESSAGE_MAX 1024
#define KFIFO_COUNT 6
struct camlog_dev {
	uid_t writer_uid;					 // UID of the log writer
	pid_t writer_pid;					 // PID of the log writer
	char m_camlog_message[MESSAGE_MAX];   // Log message buffer
};

void camlog_send_message(void);
#endif /* _CAM_LOG_H_ */
// END Camera_CameraSkyNet
