/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/string.h>

#include "../imgsensor_cfg_table.h"
#include "../imgsensor_platform.h"
#include "smartldo_wl2866d.h"

#define LDO_CAMERA_STATUS "ldo_camera_status"
#define LDO_CAMERA_DEBUG
#define WL2866_LDO_EN_REG  0x0E
#define WL2866_LDO_EN_MASK 0x0F

#define CUST_PINCTRL_STATE_EN_HIGH "smartldo_en_high"
#define CUST_PINCTRL_STATE_EN_LOW  "smartldo_en_low"
#define CUST_PINCTRL_STATE_CLK_HIGH "i2c7_clk_high"
#define CUST_PINCTRL_STATE_CLK_LOW  "i2c7_clk_low"
#define CUST_PINCTRL_STATE_CLK      "i2c7_clk_func"
#define CUST_PINCTRL_STATE_SDA_HIGH "i2c7_sda_high"
#define CUST_PINCTRL_STATE_SDA_LOW  "i2c7_sda_low"
#define CUST_PINCTRL_STATE_SDA      "i2c7_sda_func"

static struct pinctrl *cust_pinctrl;
static struct pinctrl_state *cust_en_high;
static struct pinctrl_state *cust_en_low;
static struct pinctrl_state *cust_sck_high;
static struct pinctrl_state *cust_sck_low;
static struct pinctrl_state *cust_sck;
static struct pinctrl_state *cust_sda_high;
static struct pinctrl_state *cust_sda_low;
static struct pinctrl_state *cust_sda;

bool ldo_check_flag = 0;
static void smartldo_i2c_reset(void);

struct wl2866_data {
	struct mutex lock;
	struct i2c_client *client;
};

typedef enum cam_cell_id {
	WL2866_LDO_DVDD1,
	WL2866_LDO_DVDD2,
	WL2866_LDO_AVDD1,
	WL2866_LDO_AVDD2,
	WL2866_LDO_NUM,
} cam_cell_id_t;

typedef struct wl2866_ldo_ctrl {
	uint32_t status_bit;
	uint8_t  reg_addr;
	bool status;
} wl2866_ldo_ctrl_t;

wl2866_ldo_ctrl_t wl2866_map[WL2866_LDO_NUM] = {
	{ 0x00, 0x03, false}, //dvdd1
	{ 0x00, 0x04, false}, //dvdd2
	{ 0x00, 0x05, false}, //avdd1
	{ 0x00, 0x06, false}  //avdd2
};

typedef struct wl2866_ldo_pin_map {
	enum IMGSENSOR_SENSOR_IDX   sensor_idx;
	enum IMGSENSOR_HW_PIN       pin;
	enum cam_cell_id cell_id;
} wl2866_ldo_pin_map_t;

wl2866_ldo_pin_map_t wl2866_ldo_pin_map[] = {
	{ IMGSENSOR_SENSOR_IDX_MAIN, IMGSENSOR_HW_PIN_AVDD, WL2866_LDO_AVDD2},//2.8V
	{ IMGSENSOR_SENSOR_IDX_MAIN, IMGSENSOR_HW_PIN_DVDD, WL2866_LDO_DVDD2},//1.2V
	{ IMGSENSOR_SENSOR_IDX_SUB, IMGSENSOR_HW_PIN_DVDD, WL2866_LDO_DVDD1}, //1.2V
	{ IMGSENSOR_SENSOR_IDX_SUB, IMGSENSOR_HW_PIN_AVDD, WL2866_LDO_AVDD1}, //2.8V
	{ IMGSENSOR_SENSOR_IDX_MAIN2, IMGSENSOR_HW_PIN_AVDD, WL2866_LDO_AVDD1}, //2.8V, Depth_Ultra
	{ IMGSENSOR_SENSOR_IDX_MAIN2, IMGSENSOR_HW_PIN_DVDD, WL2866_LDO_DVDD1}, //1.2V, Depth_Ultra
	{ IMGSENSOR_SENSOR_IDX_SUB2, IMGSENSOR_HW_PIN_AVDD, WL2866_LDO_AVDD1}, //2.8V, Macro
};

typedef struct {
	enum IMGSENSOR_HW_PIN_STATE pin_state;
	uint32_t votage;
} wl2866_ldo_votage_map_t;

wl2866_ldo_votage_map_t wl2866_ldo_avdd_votage_map[] = {
	{ IMGSENSOR_HW_PIN_STATE_LEVEL_2200, 0x50},
	{ IMGSENSOR_HW_PIN_STATE_LEVEL_2800, 0x80},

};

wl2866_ldo_votage_map_t wl2866_ldo_dvdd_votage_map[] = {
/* L19 code for 190848 by caihongfei at 2022.03.09 start */
	{ IMGSENSOR_HW_PIN_STATE_LEVEL_1000, 0x4B},
/* L19 code for 190848 by caihongfei at 2022.03.09 end */
	{ IMGSENSOR_HW_PIN_STATE_LEVEL_1100, 0x53},
	{ IMGSENSOR_HW_PIN_STATE_LEVEL_1200, 0x64},

};

static struct wl2866_data *wl2866_data;

static int cam_smart_ldo_wl2866_control(enum IMGSENSOR_SENSOR_IDX   sensor_idx, cam_cell_id_t ldo_id, uint32_t value, bool enable);


static struct SMARTLDO ldo_instance;


static enum IMGSENSOR_RETURN smartldo_init(
	void *pinstance,
	struct IMGSENSOR_HW_DEVICE_COMMON *pcommon)
{
	return IMGSENSOR_RETURN_SUCCESS;
}

static enum IMGSENSOR_RETURN smartldo_release(void *pinstance)
{
	return IMGSENSOR_RETURN_SUCCESS;
}


static enum IMGSENSOR_RETURN smartldo_dump(void *pinstance)
{
	return IMGSENSOR_RETURN_SUCCESS;
}


static cam_cell_id_t smartldo_get_map_ldo_id(enum IMGSENSOR_SENSOR_IDX   sensor_idx, enum IMGSENSOR_HW_PIN       pin)
{
	uint8_t i = 0;
	uint8_t size = sizeof(wl2866_ldo_pin_map)/sizeof(wl2866_ldo_pin_map_t);

	for (i = 0; i < size; i++) {
		if ((sensor_idx == wl2866_ldo_pin_map[i].sensor_idx) && (pin == wl2866_ldo_pin_map[i].pin))
			return wl2866_ldo_pin_map[i].cell_id;
	}

	return WL2866_LDO_NUM;
}



static int32_t smartldo_get_ldo_value(enum IMGSENSOR_HW_PIN_STATE pin_state, cam_cell_id_t cell_id)
{
	uint8_t i = 0;
	uint8_t size = 0;
	wl2866_ldo_votage_map_t *votage_map = NULL;


	if (cell_id == WL2866_LDO_DVDD1 ||
		cell_id == WL2866_LDO_DVDD2) {
			votage_map = wl2866_ldo_dvdd_votage_map;
			size = ARRAY_SIZE(wl2866_ldo_dvdd_votage_map);
	} else if (cell_id == WL2866_LDO_AVDD1 ||
		cell_id == WL2866_LDO_AVDD2) {
			votage_map = wl2866_ldo_avdd_votage_map;
			size = ARRAY_SIZE(wl2866_ldo_avdd_votage_map);
	} else {
		printk("%s:error cell_id = %d", __func__, cell_id);
		return -1;
	}

	for (i = 0; i < size; i++) {
		if (pin_state == votage_map[i].pin_state)
			return votage_map[i].votage;
	}

	return -1;
}

static int smartldo_check_init(void)
{
	struct i2c_client *client;
	int i = 0, value = 0, ret = 1, chip_id = 0;

	client = wl2866_data->client;
	/*
		 for  1 supplier of smart ldo,you need soft reset register,
		otherwise may cause smart ldo power on fail
	*/

	for (i = 1; i <= 0x0F; i++) {
		ret = i2c_smbus_write_byte_data(wl2866_data->client, i, value);
		if (ret != 0) {
			printk("%s: write value(%x) error, ret = %d\n", __func__, value, ret);
		}
		ret = i2c_smbus_read_byte_data(wl2866_data->client, i);
		if (i==0x0A || i==0x0B ){
			if (ret !=0){
				printk("%s: read register[%x] = %x,  register value != 0, ERROR device !\n", __func__, i, ret);
			}
		}
		printk("%s: read register[%x] = %x\n", __func__, i, ret);
	}

	for(i=0;i<3;i++)
	{
		ret = i2c_smbus_read_byte_data(client, 0x19);
		printk("[%s][smart_ldo_chip_id addr 0x19:0x%x][i:%d]\n", __func__,ret,i);
		if(ret == 0x04 || ret == 0x00)
		{
			break;
		}else{
			smartldo_i2c_reset();
			mdelay(5);
		}
	}
	ldo_check_flag = 1;

	if (ret == 0x04) {
		printk("[%s] wl2866d read chip rev successed! ldo_num_2866\n", __func__);
		chip_id = 2866;
	}else if (ret == 0x00) {
		printk("[%s] et5904 read chip rev successed! ldo_num_5904\n", __func__);
		chip_id = 5904;
	} else{
		printk("[%s] read chip rev failed! ldo_num_err\n", __func__);
		return -1;
	}

	if (chip_id == 2866) {
		ret = i2c_smbus_write_byte_data(wl2866_data->client, 0x01, 0x04);
		/*REG 0X01	VALE AVDD2[7,6] AVDD1[5,4] DVDD2[3,2] DVDD1[1,0]
		  00:1300MA	01:1440MA	10:1580MA	11:1720
		  AVDD2:480MA	AVDD1:480MA	DVDD2:1440MA	DVDD1:1300ma   */
		printk("[%s] wl2866d set current !\n", __func__);
	}else if (chip_id == 5904) {
		ret = i2c_smbus_write_byte_data(wl2866_data->client, 0x10, 0x00);
		ret = i2c_smbus_write_byte_data(wl2866_data->client, 0x01, 0x03);
		/* AVDD2:450MA	AVDD1:450MA	DVDD2:1450MA	DVDD1:1450ma   */
		printk("[%s] et5904  set current !\n", __func__);
	}else{
		printk("[%s] ldo  set current failed!\n", __func__);
	}

	return 0;
}

static enum IMGSENSOR_RETURN smartldo_set(
	void *pinstance,
	enum IMGSENSOR_SENSOR_IDX   sensor_idx,
	enum IMGSENSOR_HW_PIN       pin,
	enum IMGSENSOR_HW_PIN_STATE pin_state)
{
	cam_cell_id_t ldo_id = WL2866_LDO_NUM;
	uint32_t value = 0;
	uint32_t discharge_value=0;
	bool enable = false;
	int err=0;
	enum IMGSENSOR_RETURN ret = IMGSENSOR_RETURN_ERROR;


	if (pin > IMGSENSOR_HW_PIN_DOVDD   ||
	    pin < IMGSENSOR_HW_PIN_AVDD    ||
	    pin_state < IMGSENSOR_HW_PIN_STATE_LEVEL_0 ||
	    pin_state >  IMGSENSOR_HW_PIN_STATE_LEVEL_2900 ||
	    sensor_idx < 0)
		return ret;

	if (wl2866_data == NULL || wl2866_data->client == NULL)
		return ret;

	if(!ldo_check_flag)
	{
		err = smartldo_check_init();
		if(err < 0)
		printk("[%s] smartldo_check_init failed!\n", __func__);
	}

	//mutex
	mutex_lock(&wl2866_data->lock);
	ldo_id = smartldo_get_map_ldo_id(sensor_idx, pin);
	printk("%s: ldo id = %d", __func__, ldo_id);
	if (ldo_id == WL2866_LDO_NUM) {
		printk("%s: get ldo id failed\n", __func__);
		goto exit;
	} else
		printk("%s: ldo id = %d", __func__, ldo_id);

	if (pin_state == IMGSENSOR_HW_PIN_STATE_LEVEL_0)
		enable = false;
	else {
		enable = true;
		value = smartldo_get_ldo_value(pin_state, ldo_id);
		if (value < 0) {
			printk("%s: get ldo votage failed\n", __func__);
			goto exit;
		} else
			printk("%s: votage value = 0x%xH", __func__, value);
	}
	 i2c_smbus_write_byte_data(wl2866_data->client, 0x02, 0x8F);
	 discharge_value = i2c_smbus_read_byte_data(wl2866_data->client, 0x02);
	 printk("discharge_value = 0x%02x",discharge_value);
	if (cam_smart_ldo_wl2866_control(sensor_idx, ldo_id, value, enable) == 0)
		ret = IMGSENSOR_RETURN_SUCCESS;

exit:
	//release mutex
	mutex_unlock(&wl2866_data->lock);
	return ret;
}



static struct IMGSENSOR_HW_DEVICE device = {
	.id        = IMGSENSOR_HW_ID_SMARTLDO,
	.pinstance = (void *)&ldo_instance,
	.init      = smartldo_init,
	.set       = smartldo_set,
	.release   = smartldo_release,
	.dump      = smartldo_dump
};

enum IMGSENSOR_RETURN imgsensor_hw_smartldo_open(
	struct IMGSENSOR_HW_DEVICE **pdevice)
{
	*pdevice = &device;
	return IMGSENSOR_RETURN_SUCCESS;
}
EXPORT_SYMBOL(imgsensor_hw_smartldo_open);

//static uint16_t s_enable_reg_state = 0;
static int wl2866_ldo_enable(uint16_t ldo_id, bool enable)
{
	uint16_t val = 0;
	int ret = -1;

	if (wl2866_data == NULL || wl2866_data->client == NULL)
		return -1;
	val = i2c_smbus_read_byte_data(wl2866_data->client, WL2866_LDO_EN_REG);

	if(enable)
		val |= (1<<ldo_id);
	else
		val &= ~(1<<ldo_id);
	printk("%s: need to set enable value = %d\n", __func__, val);

/*
for smartldo debug write WL2866_LDO_EN_REG 0xf
*/
	// if(enable){
	// 	ret = i2c_smbus_write_byte_data(wl2866_data->client, WL2866_LDO_EN_REG,
	// 			0xf);
	// }else{
	// 	ret = i2c_smbus_write_byte_data(wl2866_data->client, WL2866_LDO_EN_REG,
	// 			0);
	// }
	ret = i2c_smbus_write_byte_data(wl2866_data->client, WL2866_LDO_EN_REG, val);
	if (ret == 0) {
		//val = i2c_smbus_read_byte_data(wl2866_data->client, WL2866_LDO_EN_REG);
		printk("%s: read enable value = %d success\n", __func__, val);
	}
	return ret;
}


static int wl2866_ldo_set_value(cam_cell_id_t ldo_id, uint32_t value)
{
	int ret = -1;

	if (wl2866_data == NULL || wl2866_data->client == NULL)
		return -1;
	printk("%s: need to write lod_id = %d, value = %d", __func__, ldo_id, value);
	ret = i2c_smbus_write_byte_data(wl2866_data->client, wl2866_map[ldo_id].reg_addr, value);
	if (ret == 0) {
		//value = i2c_smbus_read_byte_data(wl2866_data->client, wl2866_map[ldo_id].reg_addr);
		printk("%s: read ldo_id =%d register success \n", __func__, value);
	}
	return ret;
}

static int cam_smart_ldo_wl2866_control(enum IMGSENSOR_SENSOR_IDX sensor_idx, cam_cell_id_t ldo_id, uint32_t value, bool enable)
{
	int ret = 0;
	uint32_t status_bit = 0;

	printk("%s: entry\n", __func__);
	if (ldo_id >= WL2866_LDO_NUM) {
		printk("%s: error ldo_id = %d\n", __func__, ldo_id);
		return -EFAULT;
	}

	if (enable) {
		status_bit = wl2866_map[ldo_id].status_bit | (1<<sensor_idx);
		if (wl2866_map[ldo_id].status == enable) {
			wl2866_map[ldo_id].status_bit = status_bit;
			printk("%s: no need to enable pin=%d again", __func__, ldo_id);
			return 0;
		}
	} else {
		status_bit = wl2866_map[ldo_id].status_bit & (~(1<<sensor_idx));
		if (status_bit > 0) {
			wl2866_map[ldo_id].status_bit = status_bit;
			printk("%s: no need to disable pin=%d again", __func__, ldo_id);
			return 0;
		}
	}
	if (enable)
		ret = wl2866_ldo_set_value(ldo_id, value);

	if (ret < 0) {
		printk("%s: wl2866_ldo_set_value is fail\n", __func__);
		return ret;
	}
	ret = wl2866_ldo_enable(ldo_id, enable);
	if (ret != 0)
		printk("%s: wl2866_ldo_enable is fail, ret = %d\n", __func__, ret);
	else {
		wl2866_map[ldo_id].status = enable;
		wl2866_map[ldo_id].status_bit = status_bit;
		printk("%s: wl2866_ldo_enable is success\n", __func__);
	}
	return ret;

}

#define WRITE_BUFF_SIZE 32


// MAIN AVDD -> enable: echo " 0 3 9" > /proc/ ldo_camera_status     disable:echo " 0 3 0" > /proc/ ldo_camera_status     2.8V  avdd1
// MAIN DVDD ->  enable: echo " 0 4 2" > /proc/ ldo_camera_status     disable:echo " 0 4 0" > /proc/ ldo_camera_status    1.1V   dvdd1
// MAIN DVDD ->  enable: echo " 0 4 3" > /proc/ ldo_camera_status     disable:echo " 0 4 0" > /proc/ ldo_camera_status    1.2V   dvdd1
// SUB DVDD -> enable: echo " 1 4 3" > /proc/ ldo_camera_status     disable:echo " 1 4 0" > /proc/ ldo_camera_status     1.2V dvdd2
// SUB AVDD ->  enable: echo " 1 3 9" > /proc/ ldo_camera_status     disable:echo " 1 3 0" > /proc/ ldo_camera_status    2.8V   avdd2
static ssize_t ldo_camera_proc_write(struct file *file, const char __user *buf,
			     size_t len, loff_t *ppos)
{
	int res = 0;
	char write_buf[WRITE_BUFF_SIZE] = { 0 };
	enum IMGSENSOR_SENSOR_IDX   sensor_idx;
	enum IMGSENSOR_HW_PIN       pin;
	enum IMGSENSOR_HW_PIN_STATE pin_state;

	if (len >= WRITE_BUFF_SIZE) {
		return -EFAULT;
	}
	if (copy_from_user(write_buf, buf, len)) {
		return -EFAULT;
	}

	res = sscanf(write_buf, "%d %d %d", &sensor_idx, &pin, &pin_state);

	printk("%s:res=%d, sensor_idx=%d,pin=%d,pin_state=%d\n", __func__, res, sensor_idx, pin, pin_state);

	if (res != 3) {
		return -EINVAL;
	}

	if (wl2866_data == NULL || wl2866_data->client == NULL)
		return -EINVAL;

	res = smartldo_set(NULL, sensor_idx, pin, pin_state);
	if (res != IMGSENSOR_RETURN_SUCCESS)
		printk("%s:write ldo failed\n", __func__);
	else
		printk("%s:write ldo success\n", __func__);

/*
for smartldo debug direct to write register
*/
	// res = i2c_smbus_write_byte_data(wl2866_data->client, sensor_idx, pin);
	// if (res != 0) {
	// 	printk("%s: write value(%x) error, res = %d\n", __func__, sensor_idx, res);
	// } else {
	// 	printk("%s: write addr : 0x%x value 0x%x", __func__, sensor_idx, pin);
	// }

	return len;
}



static int ldo_camera_proc_show(struct seq_file *file, void *data)
{
	int i = 0;

	if (wl2866_data == NULL || wl2866_data->client == NULL) {
		seq_puts(file, "wl2866_client_p is NULL,please check device\n");
		return -EINVAL;
	}

	for (i = 0; i < 16; i++) {
		seq_printf(file, "reg[%d]=0x%x\n", i, i2c_smbus_read_byte_data(wl2866_data->client, i));
	}

	return 0;
}

/* 20230626 longcheer zhangfeng5 edit.begin */
/* add interfaces for vcama. export this to all kernel module to call
 * smartldo_set_vcama(0): disable vcamio
 * smartldo_set_vcama(1): enable vcamio
 */
int smartldo_set_vcama(int enable){
	enum IMGSENSOR_RETURN ret;

	if (wl2866_data == NULL || wl2866_data->client == NULL)
		return -EINVAL;

	printk("robe_debug: %s run.\n", __func__);
	if (enable == 0) {
		ret = smartldo_set(NULL, 1, 3, 0);
		printk("robe_debug: smartldo set vcama off.\n");
	} else {
		ret = smartldo_set(NULL, 1, 3, 9);
		printk("robe_debug: smartldo set vcama on.\n");
	}
	if (ret != IMGSENSOR_RETURN_SUCCESS) {
		return -1;
	}
	else {
		return 0;
	}
}
EXPORT_SYMBOL(smartldo_set_vcama);
/* 20230626 longcheer zhangfeng5 edit.end */

static int ldo_camera_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ldo_camera_proc_show, inode->i_private);
}

static const struct proc_ops ldo_camera_proc_fops = {
	.proc_open = ldo_camera_proc_open,
	.proc_read = seq_read,
	.proc_write = ldo_camera_proc_write,
	.proc_release	= single_release,
	.proc_lseek	= seq_lseek,
};


//static int cust_pinctrl_dts(struct platform_device *pdev)
static int cust_pinctrl_dts(void)
{
	int ret = 0;
	struct device *dev;
	/* get pinctrl */
	printk("[%s]start !\n", __func__);
	dev=&wl2866_data->client->dev;
	cust_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(cust_pinctrl)) {
		printk("smartldo Failed to get pinctrl.\n");
		ret = PTR_ERR(cust_pinctrl);
		return ret;
	}
	/* smartldo clk pin initialization */
	cust_sck_high = pinctrl_lookup_state(cust_pinctrl, CUST_PINCTRL_STATE_CLK_HIGH);
	if (IS_ERR(cust_sck_high)) {
		printk("smartldo Failed to init (%s)\n", CUST_PINCTRL_STATE_CLK_HIGH);
		ret = PTR_ERR(cust_sck_high);
	}
	cust_sck_low = pinctrl_lookup_state(cust_pinctrl, CUST_PINCTRL_STATE_CLK_LOW);
	if (IS_ERR(cust_sck_low)) {
		printk("smartldo Failed to init (%s)\n", CUST_PINCTRL_STATE_CLK_LOW);
		ret = PTR_ERR(cust_sck_low);
	}
	cust_sck = pinctrl_lookup_state(cust_pinctrl, CUST_PINCTRL_STATE_CLK);
	if (IS_ERR(cust_sck)) {
		printk("smartldo Failed to init (%s)\n", CUST_PINCTRL_STATE_CLK);
		ret = PTR_ERR(cust_sck);
	}
	/* smartldo sda pin initialization */
	cust_sda_high = pinctrl_lookup_state(cust_pinctrl, CUST_PINCTRL_STATE_SDA_HIGH);
	if (IS_ERR(cust_sda_high)) {
		printk("smartldo Failed to init (%s)\n", CUST_PINCTRL_STATE_SDA_HIGH);
		ret = PTR_ERR(cust_sda_high);
	}
	cust_sda_low = pinctrl_lookup_state(cust_pinctrl, CUST_PINCTRL_STATE_SDA_LOW);
	if (IS_ERR(cust_sda_low)) {
		printk("smartldo Failed to init (%s)\n", CUST_PINCTRL_STATE_SDA_LOW);
		ret = PTR_ERR(cust_sda_low);
	}
	cust_sda = pinctrl_lookup_state(cust_pinctrl, CUST_PINCTRL_STATE_SDA);
	if (IS_ERR(cust_sda)) {
		printk("smartldo Failed to init (%s)\n", CUST_PINCTRL_STATE_SDA);
		ret = PTR_ERR(cust_sda);
	}

	/* smartldo en pin initialization */
	cust_en_high = pinctrl_lookup_state(cust_pinctrl, CUST_PINCTRL_STATE_EN_HIGH);
	if (IS_ERR(cust_en_high)) {
		printk("smartldo Failed to init (%s)\n", CUST_PINCTRL_STATE_EN_HIGH);
		ret = PTR_ERR(cust_en_high);
	}
	cust_en_low = pinctrl_lookup_state(cust_pinctrl, CUST_PINCTRL_STATE_EN_LOW);
	if (IS_ERR(cust_en_low)) {
		printk("smartldo Failed to init (%s)\n", CUST_PINCTRL_STATE_EN_LOW);
		ret = PTR_ERR(cust_en_low);
	}
	printk("[%s]end !\n", __func__);
	return ret;
}


static void smartldo_i2c_reset(void)
{
	printk("[%s]start !\n", __func__);
	pinctrl_select_state(cust_pinctrl, cust_en_high);
	mdelay(5);
	pinctrl_select_state(cust_pinctrl, cust_sck_high);
	pinctrl_select_state(cust_pinctrl, cust_sda_high);
	mdelay(5);
	pinctrl_select_state(cust_pinctrl, cust_sck_low);
	pinctrl_select_state(cust_pinctrl, cust_sda_low);
	mdelay(5);
	pinctrl_select_state(cust_pinctrl, cust_en_low);
	mdelay(5);
	pinctrl_select_state(cust_pinctrl, cust_sck);
	pinctrl_select_state(cust_pinctrl, cust_sda);
	mdelay(5);
	printk("[%s]end !\n", __func__);
}
static int wl2866_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = client->adapter;
	int ret=0;
#ifdef LDO_CAMERA_DEBUG
	struct proc_dir_entry *ldo_status = NULL;
#endif

	printk("%s: prob enter", __func__);
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WRITE_BYTE_DATA
				     | I2C_FUNC_SMBUS_READ_BYTE)) {
		printk("%s: i2c_check_functionality error", __func__);
		return -ENODEV;
	}


	wl2866_data = kzalloc(sizeof(struct wl2866_data), GFP_KERNEL);
	if (!wl2866_data) {
		return -ENOMEM;
		printk("%s: kzalloc error", __func__);
	}
	printk("%s: kzalloc success", __func__);
	/* Init real i2c_client */
	i2c_set_clientdata(client, wl2866_data);
	mutex_init(&wl2866_data->lock);


	wl2866_data->client = client;

#ifdef LDO_CAMERA_DEBUG
	ldo_status = proc_create(LDO_CAMERA_STATUS, 0644, NULL, &ldo_camera_proc_fops);
	if (ldo_status == NULL) {
		printk("[%s] error: create_proc_entry ldo_status failed\n", __func__);
		goto free_mem;
	}
#endif
	ret = cust_pinctrl_dts();
	if(ret)
		printk("[%s] cust_pinctrl_dts get failed! %d\n", __func__,ret);

	ret = pinctrl_select_state(cust_pinctrl, cust_en_low);
	if(ret)
		printk("[%s] cust_en_low set failed!\n", __func__);

	printk("%s success!", __func__);
	return 0;
free_mem:
	if (wl2866_data) {
		kfree(wl2866_data);
		wl2866_data = NULL;
	}
	return -ENODEV;
}

#ifdef CONFIG_OF
static const struct of_device_id wl2866_of_match[] = {
	{ .compatible = "mediatek,camera_smart_ldo" },
	{}
};
MODULE_DEVICE_TABLE(of, wl2866_of_match);
#endif

static const struct i2c_device_id wl2866_i2c_id[] = {
	{ "wl2866", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, wl2866_i2c_id);

static struct i2c_driver wl2866_driver = {
	.driver = {
		.name	= "ldo_wl2866",
		.of_match_table = of_match_ptr(wl2866_of_match),
	},
	.probe = wl2866_probe,
	.id_table = wl2866_i2c_id,
};
enum IMGSENSOR_RETURN smartldo_i2c_create(void)
{
    i2c_add_driver(&wl2866_driver);
    return IMGSENSOR_RETURN_SUCCESS;
}
enum IMGSENSOR_RETURN smartldo_i2c_delete(void)
{
    i2c_del_driver(&wl2866_driver);
    return IMGSENSOR_RETURN_SUCCESS;
}

static int __init smartldo_wl2866_init(void)
{
	pr_info("%s start!\n", __func__);
	smartldo_i2c_create();
	return 0;
}
static void __exit smartldo_wl2866_exit(void)
{
	pr_info("%s end!\n", __func__);
	smartldo_i2c_delete();
}

module_init(smartldo_wl2866_init);
module_exit(smartldo_wl2866_exit);

MODULE_DESCRIPTION("WL 2866D I2C LDO Driver");
MODULE_AUTHOR("wang@longcheer.com>");
MODULE_LICENSE("GPL v2");



