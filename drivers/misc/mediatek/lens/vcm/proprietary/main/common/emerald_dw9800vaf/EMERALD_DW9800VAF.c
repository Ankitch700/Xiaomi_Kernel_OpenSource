// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
/*
 * DW9814AF voice coil motor driver
 *
 *
 */
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include "lens_info.h"
#define AF_DRVNAME "EMERALD_DW9800VAF_DRV"
#define AF_I2C_SLAVE_ADDR        0x18
#define AF_DEBUG
#ifdef AF_DEBUG
#define LOG_INF(format, args...) \
	pr_info(AF_DRVNAME " [%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif
static struct i2c_client *g_pstAF_I2Cclient;
static int *g_pAF_Opened;
static spinlock_t *g_pAF_SpinLock;
static unsigned long g_u4AF_INF;
static unsigned long g_u4AF_MACRO = 1023;
static unsigned long g_u4TargetPosition;
static unsigned long g_u4CurrPosition;
static int i2c_read(u8 a_u2Addr, u8 *a_puBuff)
{
	int i4RetValue = 0;
	char puReadCmd[1] = { (char)(a_u2Addr) };
	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;
	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puReadCmd, 1);
	LOG_INF(" I2C write i4RetValue = %d g_pstAF_I2Cclient->addr = 0x%x a_u2Addr = 0x%x\n", i4RetValue, g_pstAF_I2Cclient->addr, a_u2Addr);
	if (i4RetValue != 1) {
		LOG_INF(" I2C write failed!!\n");
		return -1;
	}
	i4RetValue = i2c_master_recv(g_pstAF_I2Cclient, (char *)a_puBuff, 1);
	if (i4RetValue != 1) {
		LOG_INF(" I2C read failed!!\n");
		return -1;
	}
	return 0;
}
static u8 read_data(u8 addr)
{
	u8 get_byte = 0;
	i2c_read(addr, &get_byte);
	return get_byte;
}
static int s4EMERALD_DW9800VAF_ReadReg(unsigned short *a_pu2Result)
{
	*a_pu2Result = (read_data(0x03) << 8) + (read_data(0x04) & 0xff);
	return 0;
}
static int s4AF_WriteReg(u16 a_u2Data)
{
	int i4RetValue = 0;
	char puSendCmd[3] = { 0x03, (char)(a_u2Data >> 8),
		(char)(a_u2Data & 0xFF) };
	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;
	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 3);
	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!\n");
		return -1;
	}
	return 0;
}
static inline int getAFInfo(__user struct stAF_MotorInfo *pstMotorInfo)
{
	struct stAF_MotorInfo stMotorInfo;
	stMotorInfo.u4MacroPosition = g_u4AF_MACRO;
	stMotorInfo.u4InfPosition = g_u4AF_INF;
	stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
	stMotorInfo.bIsSupportSR = 1;
	stMotorInfo.bIsMotorMoving = 1;
	if (*g_pAF_Opened >= 1)
		stMotorInfo.bIsMotorOpen = 1;
	else
		stMotorInfo.bIsMotorOpen = 0;
	if (copy_to_user(pstMotorInfo, &stMotorInfo,
		sizeof(struct stAF_MotorInfo)))
		LOG_INF("copy to user failed when getting motor information\n");
	return 0;
}
static int initdrv(void)
{
	int i4RetValue = 0;
/*N6 code for HQ-309856 by HQ camera at 20230731 start*/
	char puSendCmdArray[8][2] = {
	{0x02, 0x01}, {0x02, 0x00}, {0xFE, 0xFE},
	{0x02, 0x02}, {0x06, 0x40}, {0x07, 0x0F}, {0x10, 0x01}, {0xFE, 0xFE},
	};
/*N6 code for HQ-309856 by HQ camera at 20230731 end*/
	unsigned char cmd_number;
	LOG_INF("InitDrv[1] %p, %p\n", &(puSendCmdArray[1][0]),
		puSendCmdArray[1]);
	LOG_INF("InitDrv[2] %p, %p\n", &(puSendCmdArray[2][0]),
		puSendCmdArray[2]);
/*N6 code for HQ-314061 by HQ camera at 20230812 start*/
	for (cmd_number = 0; cmd_number < 8; cmd_number++) {
/*N6 code for HQ-314061 by HQ camera at 20230812 end*/
		if (puSendCmdArray[cmd_number][0] != 0xFE) {
			i4RetValue = i2c_master_send(g_pstAF_I2Cclient,
					puSendCmdArray[cmd_number], 2);
			/*N6 code for HQ-313435 by HQ camera at 20230811 start*/
			if (i4RetValue < 0) {
				pr_err("DW9800VAF i2c write init reg [0x%x] value [0x%x] failed \n",
						puSendCmdArray[cmd_number][0], puSendCmdArray[cmd_number][1]);
				return -1;
			}
			/*N6 code for HQ-313435 by HQ camera at 20230811 end*/
		} else {
			udelay(2000);
		}
	}
	return i4RetValue;
}
static inline int moveAF(unsigned long a_u4Position)
{
	int ret = 0;
	/*N6 code for HQ-319499 by HQ camera at 20230817 start*/
	unsigned short RegPos = 0;
	int retry = 1;
	/*N6 code for HQ-319499 by HQ camera at 20230817 end*/
	if ((a_u4Position > g_u4AF_MACRO) || (a_u4Position < g_u4AF_INF)) {
		LOG_INF("out of range\n");
		return -EINVAL;
	}
	if (g_u4CurrPosition == a_u4Position)
		return 0;
	spin_lock(g_pAF_SpinLock);
	g_u4TargetPosition = a_u4Position;
	spin_unlock(g_pAF_SpinLock);
	if (s4AF_WriteReg((unsigned short)g_u4TargetPosition) == 0) {
		// spin_lock(g_pAF_SpinLock);
		// g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
		// spin_unlock(g_pAF_SpinLock);
	} else {
		LOG_INF("set I2C failed when moving the motor\n");
		ret = -1;
	}
	/*N6 code for HQ-319499 by HQ camera at 20230817 start*/
	s4EMERALD_DW9800VAF_ReadReg(&RegPos);
	LOG_INF("Reg Pos %6d\n", RegPos);
	if (RegPos != g_u4TargetPosition) {
		mdelay(5);
		while (retry < 3) {
			LOG_INF("Reg Pos %6d != %6d,start retry=%d\n", RegPos, g_u4TargetPosition, retry);
			if (s4AF_WriteReg((unsigned short)g_u4TargetPosition) == 0) {
				s4EMERALD_DW9800VAF_ReadReg(&RegPos);
				if (RegPos == g_u4TargetPosition) {
					LOG_INF("Reg Pos %6d == %6d,retry=%d\n", RegPos, g_u4TargetPosition, retry);
					spin_lock(g_pAF_SpinLock);
					g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
					spin_unlock(g_pAF_SpinLock);
					break;
				}
				mdelay(10);
				retry++;
			} else {
				LOG_INF("set I2C failed when moving the motor\n");
				ret = -1;
				break;
			}
		}
		if (retry == 3) {
			LOG_INF("Reg Pos %6d != %6d,start retry=%d > 3 error\n", RegPos, g_u4TargetPosition, retry);
			ret = -1;
			return ret;
		}
	} else {
		spin_lock(g_pAF_SpinLock);
		g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
		spin_unlock(g_pAF_SpinLock);
	}
	/*N6 code for HQ-319499 by HQ camera at 20230817 end*/
	return ret;
}
static inline int setAFInf(unsigned long a_u4Position)
{
	spin_lock(g_pAF_SpinLock);
	g_u4AF_INF = a_u4Position;
	spin_unlock(g_pAF_SpinLock);
	return 0;
}
static inline int setAFMacro(unsigned long a_u4Position)
{
	spin_lock(g_pAF_SpinLock);
	g_u4AF_MACRO = a_u4Position;
	spin_unlock(g_pAF_SpinLock);
	return 0;
}
/* ////////////////////////////////////////////////////////////// */
long EMERALD_DW9800VAF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
		unsigned long a_u4Param)
{
	long i4RetValue = 0;
	switch (a_u4Command) {
	case AFIOC_G_MOTORINFO:
		i4RetValue = getAFInfo((__user struct stAF_MotorInfo *)
				(a_u4Param));
		break;
	case AFIOC_T_MOVETO:
		LOG_INF("moveAF %d\n", a_u4Param);
		i4RetValue = moveAF(a_u4Param);
		break;
	case AFIOC_T_SETINFPOS:
		i4RetValue = setAFInf(a_u4Param);
		break;
	case AFIOC_T_SETMACROPOS:
		i4RetValue = setAFMacro(a_u4Param);
		break;
	default:
		LOG_INF("No CMD\n");
		i4RetValue = -EPERM;
		break;
	}
	return i4RetValue;
}
/* Main jobs: */
/* 1.Deallocate anything that "open" allocated in private_data. */
/* 2.Shut down the device on last close. */
/* 3.Only called once on last time. */
/* Q1 : Try release multiple times. */
int EMERALD_DW9800VAF_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	LOG_INF("Start\n");
	if (*g_pAF_Opened == 2)
		LOG_INF("Wait\n");
	if (*g_pAF_Opened) {
		LOG_INF("Free\n");
		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 0;
		spin_unlock(g_pAF_SpinLock);
	}
	LOG_INF("End\n");
	return 0;
}
int EMERALD_DW9800VAF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
	spinlock_t *pAF_SpinLock, int *pAF_Opened)
{
	int ret = 0;
	unsigned short InitPos = 0;
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_SpinLock = pAF_SpinLock;
	g_pAF_Opened = pAF_Opened;
/*N6 code for HQ-313435 by HQ camera at 20230811 start*/
	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;
	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;
/*N6 code for HQ-313435 by HQ camera at 20230811 end*/
	if (*g_pAF_Opened == 1) {
		initdrv();
		ret = s4EMERALD_DW9800VAF_ReadReg(&InitPos);
		if (ret == 0) {
			LOG_INF("Init Pos %6d\n", InitPos);
			spin_lock(g_pAF_SpinLock);
			g_u4CurrPosition = (unsigned long)InitPos;
			spin_unlock(g_pAF_SpinLock);
		} else {
			spin_lock(g_pAF_SpinLock);
			g_u4CurrPosition = 0;
			spin_unlock(g_pAF_SpinLock);
		}
		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 2;
		spin_unlock(g_pAF_SpinLock);
	}
	return 1;
}
int EMERALD_DW9800VAF_GetFileName(unsigned char *pFileName)
{
	#if SUPPORT_GETTING_LENS_FOLDER_NAME
	char FilePath[256];
	char *FileString;
	sprintf(FilePath, "%s", __FILE__);
	FileString = strrchr(FilePath, '/');
	*FileString = '\0';
	FileString = (strrchr(FilePath, '/') + 1);
	strncpy(pFileName, FileString, AF_MOTOR_NAME);
	LOG_INF("FileName : %s\n", pFileName);
	#else
	pFileName[0] = '\0';
	#endif
	return 1;
}
