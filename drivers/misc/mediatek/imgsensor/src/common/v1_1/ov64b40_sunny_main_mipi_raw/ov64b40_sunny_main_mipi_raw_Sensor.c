/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 OV64B40SUNNYmipiraw_sensor.c
 *
 * Project:
 * --------
 *	 ALPS MT6795
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 * sensor setting : AM11a
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#include <asm/neon.h>
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#ifdef CONFIG_RLK_CAM_PERFORMANCE_IMPROVE
#include <linux/dma-mapping.h>
#endif
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include <linux/slab.h>
#include "ov64b40_sunny_main_mipi_raw_Sensor.h"

#define ENABLE_PDAF 1
#define EEPROM_SLAVE_ID 0xA2
#define VENDOR_ID 0x07

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = OV64B40_SUNNY_MAIN_SENSOR_ID,		//record sensor id defined in Kd_imgsensor.h

	.checksum_value = 0x47a75476,		//checksum value for Camera Auto Test

	.pre = {
	/*N6 code for  HQ-309670 by hudongjiao at 20230809 start*/
		.pclk = 345600000,
		.linelength = 2808,
	/*N6 code for  HQ-309670 by hudongjiao at 20230809 end*/
		.framelength = 4102,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4624,
		.grabwindow_height = 3472,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 1300114286,
	},
	.cap = {//cap same as preview
	/*N6 code for  HQ-309670 by hudongjiao at 20230809 start*/
		.pclk = 345600000,
		.linelength = 2808,
	/*N6 code for  HQ-309670 by hudongjiao at 20230809 end*/
		.framelength = 4108,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4624,
		.grabwindow_height = 3472,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 1300147200,
	},
	.normal_video = {//video same as preview
	/*N6 code for  HQ-309670 by hudongjiao at 20230809 start*/
		.pclk = 345600000,
		.linelength = 2088,
	/*N6 code for  HQ-309670 by hudongjiao at 20230809 end*/
		.framelength = 5516,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4608,
		.grabwindow_height = 2592,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 1300114286,
	},
	.hs_video = {//slow motion 1080p 240fps
	/*N6 code for  HQ-309670 by hudongjiao at 20230809 start*/
		.pclk = 345600000,
		.linelength = 1152,
	/*N6 code for  HQ-309670 by hudongjiao at 20230809 end*/
		.framelength = 1250,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 2400,
		.mipi_pixel_rate =  1300114286,
	},
	.slim_video = {//pre
	/*N6 code for  HQ-309670 by hudongjiao at 20230809 start*/
		.pclk = 345600000,
		.linelength = 2808,
	/*N6 code for  HQ-309670 by hudongjiao at 20230809 end*/
		.framelength = 4108,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4624,
		.grabwindow_height = 3472,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 1300147200,
	},
	.custom1 = {//stereo same as preview
	/*N6 code for  HQ-309670 by hudongjiao at 20230809 start*/
		.pclk = 345600000,
		.linelength = 2808,
		.framelength = 5106,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4624,
		.grabwindow_height = 3472,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 241,
		.mipi_pixel_rate = 1300147200,
	/*N6 code for  HQ-309670 by hudongjiao at 20230809 end*/
	},
	.custom2 = {//slow motion 1080P 120fps(4SUMA2A2)
	/*N6 code for  HQ-309670 by hudongjiao at 20230809 start*/
		.pclk = 345600000,
		.linelength = 1152,
	/*N6 code for  HQ-309670 by hudongjiao at 20230809 end*/
		.framelength = 2500,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 1200,
		.mipi_pixel_rate = 1300114286,
	},
	.custom3 = {//12M 60FPS (4SUM)
	/*N6 code for  HQ-309670 by hudongjiao at 20230809 start*/
		.pclk = 345600000,
		.linelength = 2088,
	/*N6 code for  HQ-309670 by hudongjiao at 20230809 end*/
		.framelength = 2758,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4608,
		.grabwindow_height = 2592,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 600,
		.mipi_pixel_rate = 1300114286,
	},
	.custom4 = {//full size capture
	/*N6 code for  HQ-309670 by hudongjiao at 20230809 start*/
		.pclk = 345600000,
		.linelength = 3240,
	/*N6 code for  HQ-309670 by hudongjiao at 20230809 end*/
		.framelength = 7112,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 9248,
		.grabwindow_height = 6944,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 150,
		.mipi_pixel_rate = 1300114286,
	},
	.custom5 = {//in sensor zoom2x
	/*N6 code for  HQ-309670 by hudongjiao at 20230809 start*/
		.pclk = 345600000,
		.linelength = 2808,
	/*N6 code for  HQ-309670 by hudongjiao at 20230809 end*/
		.framelength = 4102,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4624,
		.grabwindow_height = 3472,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 1300114286,
	},
  
	.margin       = 31, /* sensor framelength & shutter margin */
	.min_shutter  = 16, /* min shutter */
	.min_gain     = BASEGAIN, //1x
	/*N6 code for  HQ-317654 by hudongjiao at 20230816 start*/
	.max_gain     = 16 * BASEGAIN,//15.5
	/*N6 code for  HQ-317654 by hudongjiao at 20230816 end*/
	.min_gain_iso = 50,
	.exp_step     = 2,
	.gain_step    = 1,
	.gain_type    = 1,

	.max_frame_length           = 0xffffe9,
	.ae_shut_delay_frame        = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame     = 2, /* isp gain delay frame for AE cycle */
	/*N6 code for  HQ-309670 by hudongjiao at 20230809 start*/
	.frame_time_delay_frame     = 2,
	/*N6 code for  HQ-309670 by hudongjiao at 20230809 end*/

	.ihdr_support               = 0, /* 1, support; 0,not support */
	.ihdr_le_firstline          = 0, /* 1,le first ; 0, se first */
	.temperature_support        = 0, /* 1, support; 0,not support */

	.sensor_mode_num        = 10, /* support sensor mode num */
	.cap_delay_frame        = 2, /* enter capture delay frame num */
	.pre_delay_frame        = 1, /* enter preview delay frame num */
	.video_delay_frame      = 2, /* enter video delay frame num */
	.hs_video_delay_frame   = 2,
	.slim_video_delay_frame = 2, /* enter slim video delay frame num */
	.custom1_delay_frame    = 2, /* enter custom1 delay frame num */
	.custom2_delay_frame    = 2,
	.custom3_delay_frame    = 2,
	.custom4_delay_frame    = 2,
	.custom5_delay_frame    = 1,

	.isp_driving_current = ISP_DRIVING_4MA, //mclk driving current
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,//sensor_interface_type
	.mipi_sensor_type = MIPI_CPHY, //0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
	.mipi_settle_delay_mode = 1,//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_B,//sensor output first pixel color
	.mclk = 24,//mclk value, suggest 24 or 26 for 24Mhz or 26Mhz
	.mipi_lane_num = SENSOR_MIPI_3_LANE,//mipi lane num
	.i2c_addr_table = {0x20, 0xff},//record sensor support all write id addr, only supprt 4must end with 0xff
	.i2c_speed = 1000,

};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,				//mirrorflip information
	.sensor_mode = IMGSENSOR_MODE_INIT, //IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
	.shutter = 0x4C00,					//current shutter
	.gain = 0x200,						//current gain
	.dummy_pixel = 0,					//current dummypixel
	.dummy_line = 0,					//current dummyline
	.current_fps = 300,  //full size current fps : 24fps for PIP, 30fps for Normal or ZSD
	.autoflicker_en = KAL_FALSE,  //auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
	.test_pattern = KAL_FALSE,		//test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,//current scenario id
	.ihdr_en = 0, //sensor need support LE, SE with HDR feature
	.i2c_write_id = 0x20,//record current sensor's i2c write id
	.current_ae_effective_frame = 2,
	.is_writen_otp_done = KAL_FALSE,
	.is_first_frame = KAL_TRUE,
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[10] = {
	{ 9248,  6944,  0,  0,  9248,  6944,  4624,  3472,  0,   0,  4624,  3472,  0,  0,  4624,  3472},	/*Preview*/
	{ 9248,  6944,  0,  0,  9248,  6944,  4624,  3472,  0,   0,  4624,  3472,  0,  0,  4624,  3472},	/*capture*/
	{ 9248,  6944,  0,  0,  9248,  6944,  4624,  3472,  8, 440,  4608,  2592,  0,  0,  4608,  2592},	/*normal video*/
	{ 9248,  6944,784,1312, 7680,  4320,  1920,  1080,  0,   0,  1920,  1080,  0,  0,  1920,  1080},	/*hs video*/
	{ 9248,  6944,  0,  0,  9248,  6944,  4624,  3472,  0,   0,  4624,  3472,  0,  0,  4624,  3472},	/*slim video*/
	{ 9248,  6944,  0,  0,  9248,  6944,  4624,  3472,  0,   0,  4624,  3472,  0,  0,  4624,  3472},	/*custom1*/
	{ 9248,  6944,784,1312, 7680,  4320,  1920,  1080,  0,   0,  1920,  1080,  0,  0,  1920,  1080},	/*custom2*/
	{ 9248,  6944,  0,  0,  9248,  6944,  4624,  3472,  8, 440,  4608,  2592,  0,  0,  4608,  2592},	/*custom3*/
	{ 9248,  6944,  0,  0,  9248,  6944,  9248,  6944,  0,   0,  9248,  6944,  0,  0,  9248,  6944},	/*custom4*/
	{ 9248,  6944,2312,1736,4624,  3472,  4624,  3472,  0,   0,  4624,  3472,  0,  0,  4624,  3472},	/*custom5*/
};

static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[3] = {
	// Preview mode setting
	{
		0x03, 0x0A,   0x00,   0x08, 0x40, 0x00,
		0x00, 0x2B, 0x1210, 0x0D90, 0x00, 0x00, 0x0000, 0x0000,
		0x01, 0x2B, 0x0470, 0x035C, 0x00, 0x00, 0x0000, 0x0000
	},
	// Video mode setting
	{
		0x03, 0x0A,   0x00,   0x08, 0x40, 0x00,
		0x00, 0x2B, 0x1210, 0x0A28, 0x00, 0x00, 0x0000, 0x0000,
		0x01, 0x2B, 0x0470, 0x0288, 0x00, 0x00, 0x0000, 0x0000
	},
	//custom5 for ISZ
	{
		0x03, 0x0A,   0x00,   0x08, 0x40, 0x00,
		0x00, 0x2B, 0x1210, 0x0D90, 0x00, 0x00, 0x0000, 0x0000,
		0x01, 0x2B, 0x0240, 0x364, 0x00, 0x00, 0x0000, 0x0000

	},
};

#if ENABLE_PDAF
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info[3] = {
	{
		.i4OffsetX	= 40,
		.i4OffsetY	= 16,
		.i4PitchX	= 16,
		.i4PitchY	= 16,
		.i4PairNum	= 8,
		.i4SubBlkW	= 8,
		.i4SubBlkH	= 4,
		.i4BlockNumX = 284,
		.i4BlockNumY = 215,
		.iMirrorFlip = 0,
		.i4PosL = {
			{47, 18}, {55, 18}, {43, 22}, {51, 22},
			{47, 26}, {55, 26}, {43, 30}, {51, 30},
		},
		.i4PosR = {
			{46, 18}, {54, 18}, {42, 22}, {50, 22},
			{46, 26}, {54, 26}, {42, 30}, {50, 30},
		}
	},
	{
/*N6 code for HQ-323612 by hudongjiao at 20230831 start*/
		.i4OffsetX	= 32,
		.i4OffsetY	= 0,
		.i4PitchX	= 16,
		.i4PitchY	= 16,
		.i4PairNum	= 8,
		.i4SubBlkW	= 8,
		.i4SubBlkH	= 4,
		.i4BlockNumX = 284,
		.i4BlockNumY = 162,
		.iMirrorFlip = 0,
		.i4PosL = {
			{39, 2},  {47, 2},	{35, 6}, {43, 6},
			{39, 10}, {47, 10}, {35, 14}, {43, 14},
		},
		.i4PosR = {
			{38, 2},  {46, 2},  {34, 6}, {42, 6},
			{38, 10}, {46, 10}, {34, 14}, {42, 14},
		}
/*N6 code for HQ-323612 by hudongjiao at 20230831 end*/
	},
	{
		.i4OffsetX	= 80,
		.i4OffsetY	= 32,
		.i4PitchX	= 16,
		.i4PitchY	= 16,
		.i4PairNum	= 4,
		.i4SubBlkW	= 16,
		.i4SubBlkH	= 4,
		.i4BlockNumX = 288,
		.i4BlockNumY = 217,
		.iMirrorFlip = 0,
		.i4PosL = {
			{93, 35}, {93, 37}, {85, 43}, {85, 45},
		},
		.i4PosR = {
			{92, 35}, {92, 37}, {84, 43}, {84, 45},
		},
		.i4Crop = {
			{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
			{0, 0}, {0, 0}, {0, 0}, {0, 0}, {2312, 1736},
		},
	}
};
#endif

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF)};

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};
	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

#if 0
static kal_uint16 read_eeprom_module_id(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;

	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *) &get_byte, 1, EEPROM_SLAVE_ID);//Sunny

	return get_byte;
}
#endif

static kal_uint32 return_sensor_id(void)
{
	kal_uint32 sensor_id = 0;

	sensor_id = ((read_cmos_sensor(0x300B) << 8) | read_cmos_sensor(0x300C));

	LOG_INF("[%s] sensor_id: 0x%x", __func__, sensor_id);

	return sensor_id;
}

static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d \n", imgsensor.dummy_line, imgsensor.dummy_pixel);
	/* you can set dummy by imgsensor.dummy_line and imgsensor.dummy_pixel, or you can set dummy by imgsensor.frame_length and imgsensor.line_length */
	write_cmos_sensor(0x3208, 0x00);
	/*N6 code for  HQ-309670 by hudongjiao at 20230809 start*/
	// write_cmos_sensor(0x380c, (imgsensor.line_length >> 8) & 0xFF);
	// write_cmos_sensor(0x380d, imgsensor.line_length & 0xFF);
	/*N6 code for  HQ-309670 by hudongjiao at 20230809 end*/
	write_cmos_sensor(0x380e, (imgsensor.frame_length >> 8) & 0xFF);
	write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
	write_cmos_sensor(0x3208, 0x10);
	write_cmos_sensor(0x3208, 0xa0);

}	/*	set_dummy  */

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	//kal_int16 dummy_line;
	kal_uint32 frame_length = imgsensor.frame_length;
	//unsigned long flags;

	LOG_INF("framerate = %d, min framelength =%d, should enable? \n", framerate, min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ? frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	//dummy_line = frame_length - imgsensor.min_frame_length;
	//if (dummy_line < 0)
		//imgsensor.dummy_line = 0;
	//else
		//imgsensor.dummy_line = dummy_line;
	//imgsensor.frame_length = frame_length + imgsensor.dummy_line;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}	/*	set_max_framerate  */

/*************************************************************************
* FUNCTION
*	set_shutter
*
* DESCRIPTION
*	This function set e-shutter of sensor to change exposure time.
*
* PARAMETERS
*	iShutter : exposured lines
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	// OV Recommend Solution
	// if shutter bigger than frame_length, should extend frame length first
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;

	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;
	// Framelength should be an even number
	shutter = (shutter >> 1) << 1;
	imgsensor.frame_length = (imgsensor.frame_length >> 1) << 1;

	//If we write shutter for the first frame in slow motion mode, sensor will output 2 exception frames.
	//So skip write shutter for the first frame when scenario is in the slow motion mode.
	if (imgsensor.is_first_frame && (imgsensor.current_scenario_id == 6 || imgsensor.current_scenario_id == 3)) {
		imgsensor.is_first_frame = KAL_FALSE;
	} else {
		if (imgsensor.autoflicker_en) {
			realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
			if (realtime_fps >= 297 && realtime_fps <= 305)
				set_max_framerate(296, 0);
			else if (realtime_fps >= 147 && realtime_fps <= 150)
				set_max_framerate(146, 0);
			else {
				// Extend frame length
				//write_cmos_sensor(0x3208, 0x00);
				write_cmos_sensor(0x3840, (imgsensor.frame_length >> 16) & 0xFF);
				write_cmos_sensor(0x380e, (imgsensor.frame_length >> 8) & 0xFF);
				write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
				//write_cmos_sensor(0x3208, 0x10);
				//write_cmos_sensor(0x3208, 0xa0);
			}
		} else {
			// Extend frame length
			//write_cmos_sensor(0x3208, 0x00);
			write_cmos_sensor(0x3840, (imgsensor.frame_length >> 16) & 0xFF);
			write_cmos_sensor(0x380e, (imgsensor.frame_length >> 8) & 0xFF);
			write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
			//write_cmos_sensor(0x3208, 0x10);
			//write_cmos_sensor(0x3208, 0xa0);
		}

		// Update Shutter
		//write_cmos_sensor(0x3208, 0x01);
		write_cmos_sensor(0x3840, (imgsensor.frame_length >> 16) & 0xFF);
		write_cmos_sensor(0x380e, (imgsensor.frame_length >> 8) & 0xFF);
		write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
		write_cmos_sensor(0x3500, (shutter >> 16) & 0xFF);
		write_cmos_sensor(0x3501, (shutter >> 8) & 0xFF);
		write_cmos_sensor(0x3502, (shutter)  & 0xFF);
		//write_cmos_sensor(0x3208, 0x11);
		//write_cmos_sensor(0x3208, 0xa1);
	}
	imgsensor.current_ae_effective_frame = 2;

	LOG_INF("Exit! shutter =%d, framelength =%d\n", shutter, imgsensor.frame_length);
}

static void set_shutter_frame_length(kal_uint16 shutter,
			kal_uint16 frame_length)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	spin_lock(&imgsensor_drv_lock);
	/*Change frame time*/
	/*N6 code for  HQ-309670 by hudongjiao at 20230809 start*/
	if (frame_length > 1)
		dummy_line = frame_length - imgsensor.frame_length;
	imgsensor.frame_length = imgsensor.frame_length + dummy_line;

	if (shutter > imgsensor.frame_length - imgsensor_info.margin) {
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	}
	/*else {
		imgsensor.frame_length = imgsensor.min_frame_length;
	}*/
	/*N6 code for  HQ-309670 by hudongjiao at 20230809 end*/
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length -
		imgsensor_info.margin)) ? (imgsensor_info.max_frame_length -
		imgsensor_info.margin) : shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk /
			imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			// Extend frame length
			write_cmos_sensor(0x3840, (imgsensor.frame_length >> 16) & 0xFF);
			write_cmos_sensor(0x380e, (imgsensor.frame_length >> 8) & 0xFF);
			write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
		}
	} else {
		// Extend frame length
		write_cmos_sensor(0x3840, (imgsensor.frame_length >> 16) & 0xFF);
		write_cmos_sensor(0x380e, (imgsensor.frame_length >> 8) & 0xFF);
		write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
	}

	// Update Shutter
	write_cmos_sensor(0x3500, (shutter >> 16) & 0xFF);
	write_cmos_sensor(0x3502, (shutter) & 0xFF);
	write_cmos_sensor(0x3501, (shutter >> 8) & 0xFF);
	LOG_INF("[OV48BII]Add for N3D! shutter =%d, framelength =%d\n",
		shutter, imgsensor.frame_length);
}
/*N6 code for  HQ-317654 by hudongjiao at 20230816 start*/
// #define FACTOR 992.0f//(15.5f　* 64.0f)
// static kal_uint32 digital_gain_calc(kal_uint16 aaa_gain)
// {
// 	float real_dig_gain = 1.0f;//MIN Dgain
// 	kal_uint32 reg_dig_gain = 1024;//1024 = 1x

// 	real_dig_gain = aaa_gain / FACTOR;

// 	if (real_dig_gain > 15.99f)//Max digital gain
// 	{
// 		real_dig_gain = 15.99f;
// 	}

// 	reg_dig_gain = (kal_uint32)(real_dig_gain * 1024) << 6;

// 	return reg_dig_gain;
// }
/*N6 code for  HQ-317654 by hudongjiao at 20230816 end*/
/*N6 code for  HQ-317654 by hudongjiao at 20230816 start*/
static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0;
	reg_gain = gain * 256 / BASEGAIN;

	if(reg_gain < 256)
		reg_gain = 256;
	else if(reg_gain > 4080)
		reg_gain = 4080;

	return reg_gain;
}
/*N6 code for  HQ-317654 by hudongjiao at 20230816 start*/

/*************************************************************************
* FUNCTION
*	set_gain
*
* DESCRIPTION
*	This function is to set global gain to sensor.
*
* PARAMETERS
*	iGain : sensor global gain(base: 0x40)
*
* RETURNS
*	the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
	/*N6 code for  HQ-317654 by hudongjiao at 20230816 start*/
	kal_uint16 reg_gain, max_gain, min_gain;

	max_gain = imgsensor_info.max_gain;//setuphere for mode use
	min_gain = imgsensor_info.min_gain;//setuphere for mode use


	if (gain < min_gain || gain > max_gain) {
		LOG_INF("Error max gain setting: %d Should between %d & %d\n",
			gain, min_gain, max_gain);
		if (gain < min_gain)
			gain = min_gain;
		else if (gain > max_gain)
			gain = max_gain;
	}

	reg_gain = gain2reg(gain);
	LOG_INF("gain = %d, reg_gain = 0x%x, max_gain:0x%x\n ",
		gain, reg_gain, max_gain);

	write_cmos_sensor(0x3508, (reg_gain>>8) & 0xFF);
	write_cmos_sensor(0x3509,  reg_gain & 0xFF);
	/*N6 code for  HQ-317654 by hudongjiao at 20230816 end*/
	return gain;
}	/*	set_gain  */

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
	LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n", le, se, gain);
	if (imgsensor.ihdr_en) {
		spin_lock(&imgsensor_drv_lock);
		if (le > imgsensor.min_frame_length - imgsensor_info.margin)
			imgsensor.frame_length = le + imgsensor_info.margin;
		else
			imgsensor.frame_length = imgsensor.min_frame_length;
		if (imgsensor.frame_length > imgsensor_info.max_frame_length)
			imgsensor.frame_length = imgsensor_info.max_frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (le < imgsensor_info.min_shutter)
			le = imgsensor_info.min_shutter;

		if (se < imgsensor_info.min_shutter)
			se = imgsensor_info.min_shutter;

		// Extend frame length first
		write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);

		write_cmos_sensor(0x3502, (le) & 0xFF);
		write_cmos_sensor(0x3501, (le >> 8) & 0xFF);
		write_cmos_sensor(0x3500, (le >> 16) & 0xFF);

		write_cmos_sensor(0x3512, (se << 4) & 0xFF);
		write_cmos_sensor(0x3511, (se >> 4) & 0xFF);
		write_cmos_sensor(0x3510, (se >> 12) & 0x0F);

		set_gain(gain);
	}
}

/*************************************************************************
* FUNCTION
*	night_mode
*
* DESCRIPTION
*	This function night mode of sensor.
*
* PARAMETERS
*	bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/
}	/*	night_mode	*/

static kal_uint32 get_sensor_temperature(void)
{
	UINT8 temperature = 0, temp;
	static kal_uint32 temp_c;
	UINT32 temperature_convert = 0;
	static DEFINE_MUTEX(get_temperature_mutex);

	/*enable temperature TEMP_SEN_CTL */
	mutex_lock(&get_temperature_mutex);
	temp = read_cmos_sensor(0x4d12);
	write_cmos_sensor(0x4d12, temp | 0x01);

	temperature = read_cmos_sensor(0x4d13);
	mutex_unlock(&get_temperature_mutex);

	if (temperature > 0 && temperature < 85) {
		temperature_convert = temperature;
		temp_c = temperature_convert;
	} else {
		temperature_convert = temp_c;//using last normal temperature
	}

	LOG_INF("temp_c(%d), read_reg(%d)\n", temperature_convert, temperature);
	return temperature_convert;
}

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

	if (enable) {
		imgsensor.is_first_frame = KAL_FALSE;
		mdelay(5);
		write_cmos_sensor(0x0100, 0x01);
		mdelay(5);
	} else {
		imgsensor.is_first_frame = KAL_TRUE;
		write_cmos_sensor(0x0100, 0x00);
	}
	return ERROR_NONE;
}


#define MULTI_WRITE 1

#if MULTI_WRITE
#define I2C_BUFFER_LEN 765 /* trans# max is 255, each 3 bytes */
#else
#define I2C_BUFFER_LEN 3
#endif

/*N6 code by wangjie at 2023/9/11 start*/
static kal_uint16 table_write_cmos_sensor(kal_uint16 *para,
					  kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	unsigned int  tosend, IDX;
	unsigned short  addr, data, addr_last;

	tosend = 0;
	IDX = 0;
	while(IDX < len)
	{
		addr = para[IDX];
		if(tosend == 0)
		{
			addr_last = addr - 1;
		}
		else
		{
			addr_last = para[IDX - 2];
		}
		if (tosend == 0)
		{
			puSendCmd[tosend++] = (char)(addr >> 8);
			puSendCmd[tosend++] = (char)(addr & 0xFF);
			data = para[IDX+1];
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
		}
		else if (addr == (addr_last + 1))
		{
			data = para[IDX+1];
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
		}
		// to send out the data if the sen buffer is full or last data or to program to the different address.
		if ((tosend == I2C_BUFFER_LEN) || (IDX == len) || (addr != (addr_last + 1)))
		{
			if(tosend < 4)
			{
				iWriteRegI2CTiming(puSendCmd, 3,
					imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
			}
			else
			{
				iBurstWriteReg_multi(puSendCmd,
							tosend,
							imgsensor.i2c_write_id,
							tosend,
							imgsensor_info.i2c_speed);
			}
			tosend = 0;
		}
	}
	return 0;
}
/*N6 code by wangjie at 2023/9/11 end*/

#define PDC_SIZE   720 // 732 - 12
#define XTALK_SIZE 288

#define VENDOR_ID_OFFSET          0x0001

#define PDC_EEPROM_OFFSET_SUNNY   (0x12D6+12)
#define PDC_SENSOR_OFFSET   0x5F80

#define XTALK_EEPROM_OFFSET 0x2956
#define XTALK_SENSOR_OFFSET 0x5A40


static kal_uint16 pdc_cali_setting[PDC_SIZE * 2];
static kal_uint16 xtalk_cali_setting[XTALK_SIZE * 2];

static int Is_Read_Otp_Data;

static void read_cali_data_from_eeprom(void)
{
	int i, ret;
	kal_uint8 vendor_id               = 0;
	char pu_send_cmd[2]               = {0};
	char pdc_cali_array[PDC_SIZE]     = {0};
	char xtalk_cali_array[XTALK_SIZE] = {0};

	pu_send_cmd[0] = (char)(VENDOR_ID_OFFSET >> 8);
	pu_send_cmd[1] = (char)(VENDOR_ID_OFFSET & 0xFF);

	/* Check I2C is normal */
	ret = iReadRegI2C(pu_send_cmd, 2, &vendor_id, 1, EEPROM_SLAVE_ID);
	if (ret != 0) {
		LOG_INF("iReadRegI2C error");
		return;
	}
	if (Is_Read_Otp_Data != 1) {
		/*Multi-Read pdc data*/
		pu_send_cmd[0] = (char)(PDC_EEPROM_OFFSET_SUNNY >> 8);
		pu_send_cmd[1] = (char)(PDC_EEPROM_OFFSET_SUNNY & 0xFF);
		iReadRegI2C(pu_send_cmd, 2, &pdc_cali_array[0], PDC_SIZE, EEPROM_SLAVE_ID);
		for (i = 0; i < PDC_SIZE; i++) {
			pdc_cali_setting[2 * i] = PDC_SENSOR_OFFSET + i;
			pdc_cali_setting[2 * i + 1] = pdc_cali_array[i];
		}

		/*Multi-Read xtalk data*/
		pu_send_cmd[0] = (char)(XTALK_EEPROM_OFFSET >> 8);
		pu_send_cmd[1] = (char)(XTALK_EEPROM_OFFSET & 0xFF);
		iReadRegI2C(pu_send_cmd, 2, &xtalk_cali_array[0], XTALK_SIZE, EEPROM_SLAVE_ID);
		for (i = 0; i < XTALK_SIZE; i++) {
			xtalk_cali_setting[2 * i] = XTALK_SENSOR_OFFSET + i;
			xtalk_cali_setting[2 * i + 1] = xtalk_cali_array[i];
		}

		Is_Read_Otp_Data = 1;
	}
}

static void write_pdc_data_to_sensor(void)
{
	if (Is_Read_Otp_Data == 1) {
		LOG_INF("write pdc to sensor E\n");
		table_write_cmos_sensor(pdc_cali_setting,
				sizeof(pdc_cali_setting) / sizeof(kal_uint16));
	}
}

static void write_xtalk_data_to_sensor(void)
{
	if (Is_Read_Otp_Data == 1) {
		LOG_INF("write xtalk to sensor E\n");
		table_write_cmos_sensor(xtalk_cali_setting,
				sizeof(xtalk_cali_setting) / sizeof(kal_uint16));
	}
}

static void sensor_init(void)
{
	//write_cmos_sensor(0x0103, 0x01);
	//mdelay(10);
	LOG_INF("ov64b40 sensor init start\n");
	table_write_cmos_sensor(ov64b40sunny_init_setting,
			sizeof(ov64b40sunny_init_setting) / sizeof(kal_uint16));
	LOG_INF("ov64b40 sensor init end\n");
}	/*	sensor_init  */

static void preview_setting(void)
{
	LOG_INF("E\n");

	table_write_cmos_sensor(ov64b40sunny_preview_setting,
		sizeof(ov64b40sunny_preview_setting) / sizeof(kal_uint16));
}	/*	preview_setting  */

static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("E\n");

	preview_setting();
}	/*	preview_setting  */


static void normal_video_setting()
{
	LOG_INF("E!\n");

	table_write_cmos_sensor(ov64b40sunny_normal_video_setting,
		sizeof(ov64b40sunny_normal_video_setting) / sizeof(kal_uint16));
}

static void hs_video_setting(void)
{
	LOG_INF("E\n");

	table_write_cmos_sensor(ov64b40sunny_hs_video_setting,
		sizeof(ov64b40sunny_hs_video_setting) / sizeof(kal_uint16));
}

static void slim_video_setting(void)
{
	LOG_INF("E\n");
	preview_setting();
}

static void custom1_setting(void)
{
	LOG_INF("E\n");
	/*N6 code for  HQ-309670 by hudongjiao at 20230809 start*/
	table_write_cmos_sensor(ov64b40sunny_custom1_setting,
		sizeof(ov64b40sunny_custom1_setting) / sizeof(kal_uint16));
	/*N6 code for  HQ-309670 by hudongjiao at 20230809 end*/
}

static void custom2_setting(void)
{
	LOG_INF("E\n");

	table_write_cmos_sensor(ov64b40sunny_custom2_setting,
			sizeof(ov64b40sunny_custom2_setting) / sizeof(kal_uint16));
}

static void custom3_setting(void)
{
	LOG_INF("E\n");

	table_write_cmos_sensor(ov64b40sunny_custom3_setting,
			sizeof(ov64b40sunny_custom3_setting) / sizeof(kal_uint16));
}

static void custom4_setting(void)
{
	LOG_INF("E\n");

	table_write_cmos_sensor(ov64b40sunny_custom4_setting,
			sizeof(ov64b40sunny_custom4_setting) / sizeof(kal_uint16));
}

static void custom5_setting(void)
{
	LOG_INF("E\n");

	table_write_cmos_sensor(ov64b40sunny_custom5_setting,
			sizeof(ov64b40sunny_custom5_setting) / sizeof(kal_uint16));
}

static kal_uint16 get_vendor_id(void)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(0x01 >> 8), (char)(0x01 & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 1, 0xA2);
	return get_byte;
}

/*************************************************************************
* FUNCTION
*	get_imgsensor_id
*
* DESCRIPTION
*	This function get the sensor ID
*
* PARAMETERS
*	*sensorID : return the sensor ID
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
//extern void app_get_front_sensor_name(char *back_name);
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
  kal_uint8 vendor_id = 0;
	LOG_INF("OV64B40SUNNY get_imgsensor_id in\n");

	vendor_id = get_vendor_id();
	if (vendor_id != VENDOR_ID){
		LOG_INF("OV64B40SUNNY get_vendor_id read is %x!", vendor_id);
		//*sensor_id = 0xFFFFFFFF;
		//return ERROR_SENSOR_CONNECT_FAIL;
	}

	//sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();
			if (*sensor_id == imgsensor_info.sensor_id) {
				read_cali_data_from_eeprom();//read calibration data
				LOG_INF("probe success, i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			} else {
				LOG_INF("Read sensor id fail,i2c write id: 0x%x, id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
			}
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		// if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*	open
*
* DESCRIPTION
*	This function initialize the registers of CMOS sensor
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 open(void)
{
	//const kal_uint8 i2c_addr[] = {IMGSENSOR_WRITE_ID_1, IMGSENSOR_WRITE_ID_2};
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint16 sensor_id = 0;
	LOG_INF("PLATFORM:MT6873,MIPI 3LANE\n");
	//LOG_INF("preview 1280*960@30fps,864Mbps/lane; video 1280*960@30fps,864Mbps/lane; capture 5M@30fps,864Mbps/lane\n");
	//sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
				break;
			}
			LOG_INF("Read sensor id fail,i2c write id: 0x%x, id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;

	/* initail sequence write in  */
	sensor_init();
	write_pdc_data_to_sensor();

	mdelay(10);

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.shutter = 0x2D00;
	imgsensor.gain = 0x100;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	imgsensor.is_writen_otp_done = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}	/*	open  */

/*************************************************************************
* FUNCTION
*	close
*
* DESCRIPTION
*
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 close(void)
{
	LOG_INF("E\n");

	/*No Need to implement this function*/

	return ERROR_NONE;
}	/*	close  */

/*************************************************************************
* FUNCTION
* preview
*
* DESCRIPTION
*	This function start the sensor preview.
*
* PARAMETERS
*	*image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("loading preview_setting\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	//imgsensor.video_mode = KAL_FALSE;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();

	mdelay(10);
	return ERROR_NONE;
}	/*	preview   */

/*************************************************************************
* FUNCTION
*	capture
*
* DESCRIPTION
*	This function setup the CMOS sensor in capture MY_OUTPUT mode
*
* PARAMETERS
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
						  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("loading capture_setting\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	{
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps);
	mdelay(10);

	if (imgsensor.test_pattern == KAL_TRUE)	{
		write_cmos_sensor(0x5000, (read_cmos_sensor(0x5000) & 0xBF) | 0x00);
	}

	return ERROR_NONE;
}	/* capture() */

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("loading normal_video_setting\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting();

	mdelay(10);

	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("loading hs_video_setting\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	//imgsensor.video_mode = KAL_TRUE;
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	//imgsensor.current_fps = 2400;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();

	mdelay(10);

	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	//imgsensor.video_mode = KAL_TRUE;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	//imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();

	mdelay(10);

	return ERROR_NONE;
}	/*	slim_video	 */

static kal_uint32 custom1(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT * image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("loading custom1_setting\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom1_setting();

	return ERROR_NONE;
}	/*      custom1       */

static kal_uint32 custom2(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT * image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("loading custom2_setting\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	imgsensor.pclk = imgsensor_info.custom2.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/* imgsensor.current_fps = 1200; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom2_setting();

	return ERROR_NONE;
}	/*      custom2       */

static kal_uint32 custom3(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT * image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("loading custom3_setting\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;
	imgsensor.pclk = imgsensor_info.custom3.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.custom3.linelength;
	imgsensor.frame_length = imgsensor_info.custom3.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/* imgsensor.current_fps = 600; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom3_setting();

	return ERROR_NONE;
}	/*      custom3       */

static kal_uint32 custom4(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("loading custom4_setting\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM4;
	imgsensor.pclk = imgsensor_info.custom4.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.custom4.linelength;
	imgsensor.frame_length = imgsensor_info.custom4.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom4.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/* imgsensor.current_fps = 150; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	if (imgsensor.is_writen_otp_done == KAL_FALSE) {
		write_xtalk_data_to_sensor();
		imgsensor.is_writen_otp_done = KAL_TRUE;
	}
	mdelay(2);

	custom4_setting();

	return ERROR_NONE;
}	/*      custom4       */

static kal_uint32 custom5(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("loading custom5_setting\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM5;
	imgsensor.pclk = imgsensor_info.custom5.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.custom5.linelength;
	imgsensor.frame_length = imgsensor_info.custom5.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom5.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/* imgsensor.current_fps = 150; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	if (imgsensor.is_writen_otp_done == KAL_FALSE) {
		write_xtalk_data_to_sensor();
		imgsensor.is_writen_otp_done = KAL_TRUE;
	}

	custom5_setting();

	return ERROR_NONE;
}	/*      custom5       */

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	LOG_INF("E\n");
	sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;

	sensor_resolution->SensorHighSpeedVideoWidth	 = imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight	 = imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth	 = imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight	 = imgsensor_info.slim_video.grabwindow_height;

	sensor_resolution->SensorCustom1Width = imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height = imgsensor_info.custom1.grabwindow_height;

	sensor_resolution->SensorCustom2Width = imgsensor_info.custom2.grabwindow_width;
	sensor_resolution->SensorCustom2Height = imgsensor_info.custom2.grabwindow_height;

	sensor_resolution->SensorCustom3Width = imgsensor_info.custom3.grabwindow_width;
	sensor_resolution->SensorCustom3Height = imgsensor_info.custom3.grabwindow_height;

	sensor_resolution->SensorCustom4Width = imgsensor_info.custom4.grabwindow_width;
	sensor_resolution->SensorCustom4Height = imgsensor_info.custom4.grabwindow_height;

	sensor_resolution->SensorCustom5Width = imgsensor_info.custom5.grabwindow_width;
	sensor_resolution->SensorCustom5Height = imgsensor_info.custom5.grabwindow_height;

	return ERROR_NONE;
}	/*	get_resolution	*/

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
					  MSDK_SENSOR_INFO_STRUCT *sensor_info,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	//sensor_info->SensorVideoFrameRate = imgsensor_info.normal_video.max_framerate/10; /* not use */
	//sensor_info->SensorStillCaptureFrameRate= imgsensor_info.cap.max_framerate/10; /* not use */
	//imgsensor_info->SensorWebCamCaptureFrameRate= imgsensor_info.v.max_framerate; /* not use */

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; // inverse with datasheet
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;
	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
	sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame;
	sensor_info->Custom4DelayFrame = imgsensor_info.custom4_delay_frame;
	sensor_info->Custom5DelayFrame = imgsensor_info.custom5_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame; 		 /* The frame of setting shutter default 0 for TG int */
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;	/* The frame of setting sensor gain */
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->FrameTimeDelayFrame = imgsensor_info.frame_time_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->TEMPERATURE_SUPPORT = imgsensor_info.temperature_support;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;  // 0 is default 1x
	sensor_info->SensorHightSampling = 0;	// 0 is default 1x
	sensor_info->SensorPacketECCOrder = 1;

#if ENABLE_PDAF
	sensor_info->PDAF_Support = 2;
#endif

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.cap.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:

		sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		sensor_info->SensorGrabStartX = imgsensor_info.custom3.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom3.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom3.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		sensor_info->SensorGrabStartX = imgsensor_info.custom4.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom4.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom4.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		sensor_info->SensorGrabStartX = imgsensor_info.custom5.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom5.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom5.mipi_data_lp2hs_settle_dc;

		break;
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	return ERROR_NONE;
}	/*	get_info  */

static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		capture(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		normal_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		hs_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		slim_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		custom1(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		custom2(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		custom3(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		custom4(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		custom5(image_window, sensor_config_data);
		break;
	default:
		LOG_INF("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* control() */

//N6 code for HQ-305655, by p-zhaobeidou3, 20230906, start
static kal_uint32 seamless_switch(enum MSDK_SCENARIO_ID_ENUM scenario_id, kal_uint32 shutter, kal_uint32 gain,
    kal_uint32 shutter_2ndframe, kal_uint32 gain_2ndframe)
{
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		LOG_INF("seamless switch to preview! gain(%d), shutter(%d), gain_2ndframe(%d)\n", gain, shutter, gain_2ndframe);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.sensor_mode = scenario_id;
		imgsensor.autoflicker_en = KAL_FALSE;
		imgsensor.pclk = imgsensor_info.pre.pclk;
		imgsensor.line_length = imgsensor_info.pre.linelength;
		imgsensor.frame_length = imgsensor_info.pre.framelength;
		imgsensor.min_frame_length = imgsensor_info.pre.framelength;
		spin_unlock(&imgsensor_drv_lock);

		//N6 code for HQ-356950, by p-zhaobeidou3, 20231227, start
		//write_cmos_sensor(0x3208, 0x00);
		//write_cmos_sensor(0x3208, 0x10);
		write_cmos_sensor(0x3208, 0x02);
		write_cmos_sensor(0x3016, 0xF9);
		preview_setting();
		if (gain_2ndframe != 0) {
			set_gain(gain_2ndframe);
		}
		if (shutter != 0) {
			set_shutter(shutter);
		}
		write_cmos_sensor(0x3016, 0xf0);
		write_cmos_sensor(0x3208, 0x12);
		write_cmos_sensor(0x320d, 0x00);
		write_cmos_sensor(0x3208, 0xA2);
		/*write_cmos_sensor(0x3209, 0x01);
		write_cmos_sensor(0x320a, 0x01);
		write_cmos_sensor(0x382a, 0x83);
		write_cmos_sensor(0x320e, 0xA0);*/
		//N6 code for HQ-356950, by p-zhaobeidou3, 20231227, end
		imgsensor.current_ae_effective_frame = 1;
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		LOG_INF("seamless switch to 2x mode! gain(%d), shutter(%d), gain_2ndframe(%d)\n", gain, shutter, gain_2ndframe);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.sensor_mode = scenario_id;
		imgsensor.autoflicker_en = KAL_FALSE;
		imgsensor.pclk = imgsensor_info.custom5.pclk;
		imgsensor.line_length = imgsensor_info.custom5.linelength;
		imgsensor.frame_length = imgsensor_info.custom5.framelength;
		imgsensor.min_frame_length = imgsensor_info.custom5.framelength;
		spin_unlock(&imgsensor_drv_lock);

		if (imgsensor.is_writen_otp_done == KAL_FALSE) {
			write_xtalk_data_to_sensor();
			imgsensor.is_writen_otp_done = KAL_TRUE;
		}

		//N6 code for HQ-356950, by p-zhaobeidou3, 20231227, start
		//write_cmos_sensor(0x3208, 0x00);
		//write_cmos_sensor(0x3208, 0x10);
		write_cmos_sensor(0x3208, 0x03);
		write_cmos_sensor(0x3016, 0xF9);
		custom5_setting();
		if (gain_2ndframe != 0) {
			set_gain(gain_2ndframe);
		}
		if (shutter != 0) {
			set_shutter(shutter);
		}
		write_cmos_sensor(0x3016, 0xf0);
		write_cmos_sensor(0x3208, 0x13);
		write_cmos_sensor(0x320d, 0x00);
		write_cmos_sensor(0x3208, 0xA3);
		/*write_cmos_sensor(0x3209, 0x01);
		write_cmos_sensor(0x320a, 0x01);
		write_cmos_sensor(0x382a, 0x83);
		write_cmos_sensor(0x320e, 0xA0);*/
		//N6 code for HQ-356950, by p-zhaobeidou3, 20231227, end
		imgsensor.current_ae_effective_frame = 1;
		break;
	default:
		LOG_INF("error! wrong setting in set_seamless_switch = %d", scenario_id);
		return 0xff;
	}
	imgsensor.fast_mode_on = KAL_TRUE;
	LOG_INF("%s success, scenario is switched to %d", __func__, scenario_id);
	return 0;
}
//N6 code for HQ-305655, by p-zhaobeidou3, 20230906, end

static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INF("framerate = %d\n ", framerate);
	// SetVideoMode Function should fix framerate
	if (framerate == 0)
		// Dynamic frame rate
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
	else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = framerate;
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps, 1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d \n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable) //enable auto flicker
		imgsensor.autoflicker_en = KAL_TRUE;
	else //Cancel Auto flick
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);
	mdelay(15); // workaround : fix i2c timeout
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			set_dummy();
		} else {
			/*No need to set*/
			LOG_INF("frame_length %d < shutter %d", imgsensor.frame_length, imgsensor.shutter);
		}
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk / framerate * 10 / imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ? (frame_length - imgsensor_info.normal_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			set_dummy();
		}
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	//case MSDK_SCENARIO_ID_CAMERA_ZSD:
		frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ? (frame_length - imgsensor_info.cap.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			set_dummy();
		}
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ? (frame_length - imgsensor_info.hs_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			set_dummy();
		}
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ? (frame_length - imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			set_dummy();
		}
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk / framerate * 10 / imgsensor_info.custom1.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom1.framelength) ? (frame_length - imgsensor_info.custom1.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			set_dummy();
		} else {
			/*No need to set*/
			LOG_INF("frame_length %d < shutter %d", imgsensor.frame_length, imgsensor.shutter);
		}
		break;

	case MSDK_SCENARIO_ID_CUSTOM2:
		frame_length = imgsensor_info.custom2.pclk / framerate * 10 / imgsensor_info.custom2.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom2.framelength) ? (frame_length - imgsensor_info.custom2.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom2.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			set_dummy();
		} else {
			/*No need to set*/
			LOG_INF("frame_length %d < shutter %d", imgsensor.frame_length, imgsensor.shutter);
		}
		break;

	case MSDK_SCENARIO_ID_CUSTOM3:
		frame_length = imgsensor_info.custom3.pclk / framerate * 10 / imgsensor_info.custom3.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom3.framelength) ? (frame_length - imgsensor_info.custom3.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom3.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			set_dummy();
		} else {
			/*No need to set*/
			LOG_INF("frame_length %d < shutter %d", imgsensor.frame_length, imgsensor.shutter);
		}
		break;

	case MSDK_SCENARIO_ID_CUSTOM4:
		frame_length = imgsensor_info.custom4.pclk / framerate * 10 / imgsensor_info.custom4.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom4.framelength) ? (frame_length - imgsensor_info.custom4.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom4.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			set_dummy();
		} else {
			/*No need to set*/
			LOG_INF("frame_length %d < shutter %d", imgsensor.frame_length, imgsensor.shutter);
		}
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		frame_length = imgsensor_info.custom5.pclk / framerate * 10 / imgsensor_info.custom5.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom5.framelength) ? (frame_length - imgsensor_info.custom5.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom5.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			set_dummy();
		} else {
			/*No need to set*/
			LOG_INF("frame_length %d < shutter %d", imgsensor.frame_length, imgsensor.shutter);
		}
		break;

	default:  //coding with  preview scenario by default
		frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			set_dummy();
		}
		LOG_INF("error scenario_id = %d, we use preview scenario \n", scenario_id);
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		*framerate = imgsensor_info.pre.max_framerate;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		*framerate = imgsensor_info.normal_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		*framerate = imgsensor_info.cap.max_framerate;
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		*framerate = imgsensor_info.hs_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		*framerate = imgsensor_info.slim_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		*framerate = imgsensor_info.custom1.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		*framerate = imgsensor_info.custom2.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		*framerate = imgsensor_info.custom3.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		*framerate = imgsensor_info.custom4.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		*framerate = imgsensor_info.custom5.max_framerate;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);
	 //N6 code for HQ-324136 by luwei at 2023/09/11 start
	if (enable) {
		write_cmos_sensor(0x430B, 0x00);
		write_cmos_sensor(0x430C, 0x00);
		write_cmos_sensor(0x4310, 0x00);
		write_cmos_sensor(0x4311, 0x00);
	} else {
		write_cmos_sensor(0x430B, 0xFF);
		write_cmos_sensor(0x430C, 0xFF);
		write_cmos_sensor(0x4310, 0xFF);
		write_cmos_sensor(0x4311, 0xFF);
	}
	//N6 code for HQ-324136 by luwei at 2023/09/11 end
	/*
	if (enable) {
		// 0x5081[0]: 1 enable,  0 disable
		// 0x5081[5:4]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
		write_cmos_sensor(0x5081, 0x09);
	} else {
		write_cmos_sensor(0x5081, 0x00);
	}
	*/
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
		UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;
	// unsigned long long *feature_return_para=(unsigned long long *) feature_para;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data = (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	//N6 code for HQ-305655, by p-zhaobeidou3, 20230906, start
	uint32_t *pAeCtrls;
	struct SET_SENSOR_AWB_GAIN *pSetSensorAWB = NULL;
	uint32_t *pScenarios;
	//N6 code for HQ-305655, by p-zhaobeidou3, 20230906, end

#if ENABLE_PDAF
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
#endif

	LOG_INF("feature_id = %d\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_gain;
		*(feature_data + 2) = imgsensor_info.max_gain;
		break;
	case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
		*(feature_data + 0) = imgsensor_info.min_gain_iso;
		*(feature_data + 1) = imgsensor_info.gain_step;
		*(feature_data + 2) = imgsensor_info.gain_type;
		break;
	case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_shutter;
		*(feature_data + 2) = imgsensor_info.exp_step;
		break;
#if defined(_XIAOMI_CHOPIN_)
	case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 6150000;
		break;
#endif
        case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
                /*
                 * 1, if driver support new sw frame sync
                 * set_shutter_frame_length() support third para auto_extend_en
                 */
                *(feature_data + 1) = 1;
                /* margin info by scenario */
                *(feature_data + 2) = imgsensor_info.margin;
                break;
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.cap.framelength << 16)
				+ imgsensor_info.cap.linelength;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.normal_video.framelength << 16)
				+ imgsensor_info.normal_video.linelength;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.hs_video.framelength << 16)
				+ imgsensor_info.hs_video.linelength;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.slim_video.framelength << 16)
				+ imgsensor_info.slim_video.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom1.framelength << 16)
				+ imgsensor_info.custom1.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom2.framelength << 16)
				+ imgsensor_info.custom2.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom3.framelength << 16)
				+ imgsensor_info.custom3.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom4.framelength << 16)
				+ imgsensor_info.custom4.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom5.framelength << 16)
				+ imgsensor_info.custom5.linelength;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
			break;
		}
		break;

	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= imgsensor_info.cap.pclk;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= imgsensor_info.normal_video.pclk;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= imgsensor_info.hs_video.pclk;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= imgsensor_info.slim_video.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= imgsensor_info.custom1.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= imgsensor_info.custom2.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= imgsensor_info.custom3.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= imgsensor_info.custom4.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= imgsensor_info.custom5.pclk;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= imgsensor_info.pre.pclk;
			break;
		}
		break;

	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;

	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		night_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		// get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
		// if EEPROM does not exist in camera module.
		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode(*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id(feature_return_para_32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode((BOOL)*feature_data_16, *(feature_data_16+1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*(feature_data), (MUINT32 *)(uintptr_t)(*(feature_data+1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: //for factory mode auto testing
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_32 = get_sensor_temperature();
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("current fps :%d\n", (UINT32)*feature_data);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = *feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_en = (BOOL)*feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (UINT32)*feature_data);

		wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[1], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[2], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[3], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[4], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[5], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[6], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[7], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[8], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[9], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[0], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n", (UINT16)*feature_data, (UINT16)*(feature_data + 1), (UINT16)*(feature_data + 2));
		ihdr_write_shutter_gain((UINT16)*feature_data, (UINT16)*(feature_data+1), (UINT16)*(feature_data+2));
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length((UINT16)(*feature_data),
			(UINT16)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		switch (*(feature_data + 1)) {
		case MSDK_SCENARIO_ID_CUSTOM4:
 		case MSDK_SCENARIO_ID_CUSTOM5:
			*feature_return_para_32 = 1000; /*BINNING_AVERAGED*/
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_CUSTOM2:
			*feature_return_para_32 = 2940; /*BINNING_AVERAGED*/
			break;
		default:
			*feature_return_para_32 = 1470; /*BINNING_AVERAGED*/
			break;
		}
		LOG_INF("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
			*feature_return_para_32);
		*feature_para_len = 4;

		break;
	case SENSOR_FEATURE_GET_PIXEL_RATE:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.cap.pclk /
			(imgsensor_info.cap.linelength - 80))*
			imgsensor_info.cap.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.normal_video.pclk /
			(imgsensor_info.normal_video.linelength - 80))*
			imgsensor_info.normal_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.hs_video.pclk /
			(imgsensor_info.hs_video.linelength - 80))*
			imgsensor_info.hs_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.slim_video.pclk /
			(imgsensor_info.slim_video.linelength - 80))*
			imgsensor_info.slim_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.custom1.pclk /
			(imgsensor_info.custom1.linelength - 80))*
			imgsensor_info.custom1.grabwindow_width;

			break;

		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.custom2.pclk /
			(imgsensor_info.custom2.linelength - 80))*
			imgsensor_info.custom2.grabwindow_width;

			break;

		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.custom3.pclk /
			(imgsensor_info.custom3.linelength - 80))*
			imgsensor_info.custom3.grabwindow_width;

			break;

		case MSDK_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.custom4.pclk /
			(imgsensor_info.custom4.linelength - 80))*
			imgsensor_info.custom4.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.custom5.pclk /
			(imgsensor_info.custom5.linelength - 80))*
			imgsensor_info.custom5.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.pre.pclk /
			(imgsensor_info.pre.linelength - 80))*
			imgsensor_info.pre.grabwindow_width;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_AE_EFFECTIVE_FRAME_FOR_LE:
		*feature_return_para_32 = imgsensor.current_ae_effective_frame;
		LOG_INF("GET AE EFFECTIVE %d\n", *feature_return_para_32);
		break;
	case SENSOR_FEATURE_GET_AE_FRAME_MODE_FOR_LE:
		memcpy(feature_return_para_32, &imgsensor.ae_frm_mode, sizeof(struct IMGSENSOR_AE_FRM_MODE));
		LOG_INF("GET_AE_FRAME_MODE");
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.cap.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.slim_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom1.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom2.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom3.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom4.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom5.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_VC_INFO:
		LOG_INF("SENSOR_FEATURE_GET_VC_INFO %d\n",
			(UINT16) *feature_data);

		pvcinfo = (struct SENSOR_VC_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
/*N6 code for HQ-323612 by hudongjiao at 20230831 start*/
		case MSDK_SCENARIO_ID_CUSTOM3:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[1],
			       sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
/*N6 code for HQ-323612 by hudongjiao at 20230831 end*/
		case MSDK_SCENARIO_ID_CUSTOM5:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[2],
			       sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CUSTOM1:
		default:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0],
			       sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		}
		break;
#if ENABLE_PDAF

	case SENSOR_FEATURE_GET_PDAF_DATA:
		LOG_INF("odin GET_PDAF_DATA EEPROM\n");
		break;

	case SENSOR_FEATURE_GET_PDAF_INFO:
		PDAFinfo= (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));
		switch( *feature_data)
		{
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
			case MSDK_SCENARIO_ID_CUSTOM1:
				memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info[0], sizeof(struct SET_PD_BLOCK_INFO_T));
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			case MSDK_SCENARIO_ID_CUSTOM3:
				memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info[1], sizeof(struct SET_PD_BLOCK_INFO_T));
				break;
			case MSDK_SCENARIO_ID_CUSTOM5:
				memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info[2], sizeof(struct SET_PD_BLOCK_INFO_T));
				break;
			default:
				break;
		}
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		switch (*feature_data)
		{
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
			case MSDK_SCENARIO_ID_CUSTOM1:
			case MSDK_SCENARIO_ID_CUSTOM3:
			case MSDK_SCENARIO_ID_CUSTOM5:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
				break;
			default:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
		}
		break;
	case SENSOR_FEATURE_SET_PDAF:
		imgsensor.pdaf_mode = *feature_data_16;
		LOG_INF("[odin] pdaf mode : %d \n", imgsensor.pdaf_mode);
		break;
#endif
	//N6 code for HQ-305655, by p-zhaobeidou3, 20230906, start
	case SENSOR_FEATURE_SEAMLESS_SWITCH:
			pAeCtrls = (MUINT32 *)((uintptr_t)(*(feature_data + 1)));
			pSetSensorAWB =(struct SET_SENSOR_AWB_GAIN *)((uintptr_t)(*(feature_data + 3)));
			LOG_INF("SENSOR_FEATURE_SEAMLESS_SWITCH  pAeCtrls: %d", *pAeCtrls);
			seamless_switch((*feature_data), *pAeCtrls,*(pAeCtrls+1),*(pAeCtrls+4),*(pAeCtrls+5));
		break;
	case SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS:
			pScenarios = (MUINT32 *)((uintptr_t)(*(feature_data + 1)));
			switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				LOG_INF("MSDK_SCENARIO_ID_CAMERA_PREVIEW  pScenarios: %d", *pScenarios);
				*pScenarios = MSDK_SCENARIO_ID_CUSTOM5;
				break;
			case MSDK_SCENARIO_ID_CUSTOM5:
			case MSDK_SCENARIO_ID_CUSTOM4://full size
				LOG_INF("MSDK_SCENARIO_ID_CUSTOM5  pScenarios: %d", *pScenarios);
				*pScenarios = MSDK_SCENARIO_ID_CAMERA_PREVIEW;
				break;
			default:
				*pScenarios = 0xff;
				break;
			}
			LOG_INF("SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS %d %d\n", *feature_data, *pScenarios);
		break;
	//N6 code for HQ-305655, by p-zhaobeidou3, 20230906, end

	default:
		break;
	}

	return ERROR_NONE;
}    /*    feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 OV64B40_SUNNY_MAIN_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
    LOG_INF("OV64B40SUNNY_MIPI_RAW_SensorInit in\n");
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}	/*	OV64B40SUNNY_MIPI_RAW_SensorInit	*/
