/*
 * Copyright (c) 2008-2009 QUALCOMM USA, INC.
 * 
 * All source code in this file is licensed under the following license
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org
 */

#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <media/msm_camera.h>
#include <media/msm_camera_sensor.h>
#include <mach/gpio.h>
#include <mach/camera.h>
#include <mach/msm_camio.h>
#include "mt9p012.h"

/*=============================================================
    SENSOR REGISTER DEFINES
==============================================================*/
#define MT9P012_REG_MODEL_ID         0x0000
#define MT9P012_MODEL_ID             0x2801
#define REG_GROUPED_PARAMETER_HOLD   0x0104
#define GROUPED_PARAMETER_HOLD       0x0100
#define GROUPED_PARAMETER_UPDATE     0x0000
#define REG_COARSE_INT_TIME          0x3012
#define REG_VT_PIX_CLK_DIV           0x0300
#define REG_VT_SYS_CLK_DIV           0x0302
#define REG_PRE_PLL_CLK_DIV          0x0304
#define REG_PLL_MULTIPLIER           0x0306
#define REG_OP_PIX_CLK_DIV           0x0308
#define REG_OP_SYS_CLK_DIV           0x030A
#define REG_SCALE_M                  0x0404
#define REG_FRAME_LENGTH_LINES       0x300A
#define REG_LINE_LENGTH_PCK          0x300C
#define REG_X_ADDR_START             0x3004
#define REG_Y_ADDR_START             0x3002
#define REG_X_ADDR_END               0x3008
#define REG_Y_ADDR_END               0x3006
#define REG_X_OUTPUT_SIZE            0x034C
#define REG_Y_OUTPUT_SIZE            0x034E
#define REG_FINE_INTEGRATION_TIME    0x3014
#define REG_ROW_SPEED                0x3016
#define MT9P012_REG_RESET_REGISTER   0x301A
#define MT9P012_RESET_REGISTER_PWON  0x10CC
#define MT9P012_RESET_REGISTER_PWOFF 0x10C8
#define REG_READ_MODE                0x3040
#define REG_GLOBAL_GAIN              0x305E
#define REG_TEST_PATTERN_MODE        0x3070

#define MT9P012_REV_7


struct reg_struct {
	uint16_t vt_pix_clk_div;     /* 0x0300 */
	uint16_t vt_sys_clk_div;     /* 0x0302 */
	uint16_t pre_pll_clk_div;    /* 0x0304 */
	uint16_t pll_multiplier;     /* 0x0306 */
	uint16_t op_pix_clk_div;     /* 0x0308 */
	uint16_t op_sys_clk_div;     /* 0x030A */
	uint16_t scale_m;            /* 0x0404 */
	uint16_t row_speed;          /* 0x3016 */
	uint16_t x_addr_start;       /* 0x3004 */
	uint16_t x_addr_end;         /* 0x3008 */
	uint16_t y_addr_start;       /* 0x3002 */
	uint16_t y_addr_end;         /* 0x3006 */
	uint16_t read_mode;          /* 0x3040 */
	uint16_t x_output_size ;     /* 0x034C */
	uint16_t y_output_size;      /* 0x034E */
	uint16_t line_length_pck;    /* 0x300C */
	uint16_t frame_length_lines; /* 0x300A */
	uint16_t coarse_int_time;    /* 0x3012 */
	uint16_t fine_int_time;      /* 0x3014 */
};

/*Micron settings from Applications for lower power consumption.*/
struct reg_struct mt9p012_reg_pat[2] = {
	{ /* Preview */
		/* vt_pix_clk_div          REG=0x0300 */
		6,

		/* vt_sys_clk_div          REG=0x0302 */
		1,

		/* pre_pll_clk_div         REG=0x0304 */
		2,

		/* pll_multiplier          REG=0x0306 */
		60,

		/* op_pix_clk_div          REG=0x0308 */
		8,

		/* op_sys_clk_div          REG=0x030A */
		1,

		/* scale_m                 REG=0x0404 */
		16,

		/* row_speed               REG=0x3016 */
		0x0111,

		/* x_addr_start            REG=0x3004 */
		8,

		/* x_addr_end              REG=0x3008 */
		2597,

		/* y_addr_start            REG=0x3002 */
		8,

		/* y_addr_end              REG=0x3006 */
		1949,

		/* read_mode               REG=0x3040
		 * Preview 2x2 skipping */
		0x00C3,

		/* x_output_size           REG=0x034C */
		1296,

		/* y_output_size           REG=0x034E */
		972,

		/* line_length_pck         REG=0x300C */
		3784,

		/* frame_length_lines      REG=0x300A */
		1057,

		/* coarse_integration_time REG=0x3012 */
		16,

		/* fine_integration_time   REG=0x3014 */
		1764
	},
	{ /*Snapshot*/
		/* vt_pix_clk_div          REG=0x0300 */
		6,

		/* vt_sys_clk_div          REG=0x0302 */
		1,

		/* pre_pll_clk_div         REG=0x0304 */
		2,

		/* pll_multiplier          REG=0x0306
		 * 60 for 10fps snapshot */
		60,

		/* op_pix_clk_div          REG=0x0308 */
		8,

		/* op_sys_clk_div          REG=0x030A */
		1,

		/* scale_m                 REG=0x0404 */
		16,

		/* row_speed               REG=0x3016 */
		0x0111,

		/* x_addr_start            REG=0x3004 */
		8,

		/* x_addr_end              REG=0x3008 */
		2615,

		/* y_addr_start            REG=0x3002 */
		8,

		/* y_addr_end              REG=0x3006 */
		1967,

		/* read_mode               REG=0x3040 */
		0x0041,

		/* x_output_size           REG=0x034C */
		2608,

		/* y_output_size           REG=0x034E */
		1960,

		/* line_length_pck         REG=0x300C */
		3911,

		/* frame_length_lines      REG=0x300A //10 fps snapshot */
		2045,

		/* coarse_integration_time REG=0x3012 */
		16,

		/* fine_integration_time   REG=0x3014 */
		882
	}
};

enum mt9p012_test_mode_t {
	TEST_OFF,
	TEST_1,
	TEST_2,
	TEST_3
};

enum mt9p012_resolution_t {
	QTR_SIZE,
	FULL_SIZE,
	INVALID_SIZE
};

enum mt9p012_reg_update_t {
	/* Sensor egisters that need to be updated during initialization */
	REG_INIT,
	/* Sensor egisters that needs periodic I2C writes */
	UPDATE_PERIODIC,
	/* All the sensor Registers will be updated */
	UPDATE_ALL,
	/* Not valid update */
	UPDATE_INVALID
};

enum mt9p012_setting_t {
	RES_PREVIEW,
	RES_CAPTURE
};

/* actuator's Slave Address */
#define MT9P012_AF_I2C_ADDR   0x18

/* AF Total steps parameters */
#define MT9P012_STEPS_NEAR_TO_CLOSEST_INF  32
#define MT9P012_TOTAL_STEPS_NEAR_TO_FAR    32

#define MT9P012_MU5M0_PREVIEW_DUMMY_PIXELS 0
#define MT9P012_MU5M0_PREVIEW_DUMMY_LINES  0

/* Time in milisecs for waiting for the sensor to reset.*/
#define MT9P012_RESET_DELAY_MSECS   66

/* for 20 fps preview */
#define MT9P012_DEFAULT_CLOCK_RATE  24000000
#define MT9P012_DEFAULT_MAX_FPS     26 /* ???? */

struct mt9p012_work_t {
	struct work_struct work;
};

struct mt9p012_ctrl_t {
	int8_t  opened;
	struct  msm_camera_sensor_info *sensordata;
	struct  mt9p012_work_t *sensorw;
	struct  i2c_client *client;

	enum sensor_mode_t sensormode;
	uint32_t fps_divider; /* init to 1 * 0x00000400 */
	uint32_t pict_fps_divider; /* init to 1 * 0x00000400 */

	uint16_t curr_lens_pos;
	uint16_t init_curr_lens_pos;
	uint16_t my_reg_gain;
	uint32_t my_reg_line_count;

	enum mt9p012_resolution_t prev_res;
	enum mt9p012_resolution_t pict_res;
	enum mt9p012_resolution_t curr_res;
	enum mt9p012_test_mode_t  set_test;
};

struct mt9p012_i2c_reg_conf {
	unsigned short waddr;
	unsigned short wdata;
};

static struct mt9p012_ctrl_t *mt9p012_ctrl;
static DECLARE_WAIT_QUEUE_HEAD(mt9p012_wait_queue);
DECLARE_MUTEX(mt9p012_sem);

static int mt9p012_i2c_rxdata(unsigned short saddr, unsigned char *rxdata,
	int length)
{
    struct i2c_msg msgs[] = {
	{   .addr   = saddr,
		.flags = 0,
		.len   = 2,
		.buf   = rxdata,
	},
	{   .addr   = saddr,
		.flags = I2C_M_RD,
		.len   = length,
		.buf   = rxdata,
	},
    };

	if (i2c_transfer(mt9p012_ctrl->client->adapter, msgs, 2) < 0) {
		CDBG("mt9p012_i2c_rxdata failed!\n");
		return -EIO;
	}

	return 0;
}

static int32_t mt9p012_i2c_read_w(unsigned short saddr, unsigned short raddr,
	unsigned short *rdata)
{
	int32_t rc = 0;
	unsigned char buf[4];

	if (!rdata)
		return -EIO;

	memset(buf, 0, sizeof(buf));

	buf[0] = (raddr & 0xFF00)>>8;
	buf[1] = (raddr & 0x00FF);

	rc = mt9p012_i2c_rxdata(saddr, buf, 2);
	if (rc < 0)
		return rc;

	*rdata = buf[0] << 8 | buf[1];

	if (rc < 0)
		CDBG("mt9p012_i2c_read failed!\n");

	return rc;
}

static int32_t mt9p012_i2c_txdata(unsigned short saddr,	unsigned char *txdata,
	int length)
{
	struct i2c_msg msg[] = {
		{
		.addr  = saddr,
		.flags = 0,
		.len = length,
		.buf = txdata,
		},
	};

	if (i2c_transfer(mt9p012_ctrl->client->adapter, msg, 1) < 0) {
		CDBG("mt9p012_i2c_txdata failed\n");
		return -EIO;
	}

	return 0;
}

static int32_t mt9p012_i2c_write_b(unsigned short saddr, unsigned short baddr,
	unsigned short bdata)
{
	int32_t rc = -EFAULT;
	unsigned char buf[2];

	memset(buf, 0, sizeof(buf));
	buf[0] = baddr;
	buf[1] = bdata;
	rc = mt9p012_i2c_txdata(saddr, buf, 2);

	if (rc < 0)
		CDBG("i2c_write failed, saddr = 0x%x addr = 0x%x, val =0x%x!\n",
		saddr, baddr, bdata);

	return rc;
}

static int32_t mt9p012_i2c_write_w(unsigned short saddr, unsigned short waddr,
	unsigned short wdata)
{
	int32_t rc = -EFAULT;
	unsigned char buf[4];

	memset(buf, 0, sizeof(buf));
	buf[0] = (waddr & 0xFF00)>>8;
	buf[1] = (waddr & 0x00FF);
	buf[2] = (wdata & 0xFF00)>>8;
	buf[3] = (wdata & 0x00FF);

	rc = mt9p012_i2c_txdata(saddr, buf, 4);

	if (rc < 0)
		CDBG("i2c_write_w failed, addr = 0x%x, val = 0x%x!\n",
			waddr, wdata);

	return rc;
}

static int32_t mt9p012_i2c_write_w_table(
	struct mt9p012_i2c_reg_conf *reg_conf_tbl, int num)
{
	int i;
	int32_t rc = -EFAULT;

	for (i = 0; i < num; i++) {
		rc = mt9p012_i2c_write_w(mt9p012_ctrl->client->addr,
			reg_conf_tbl->waddr, reg_conf_tbl->wdata);
		if (rc < 0)
			break;
		reg_conf_tbl++;
	}

	return rc;
}

static int32_t mt9p012_test(enum mt9p012_test_mode_t mo)
{
	int32_t rc = 0;

	rc = mt9p012_i2c_write_w(mt9p012_ctrl->client->addr,
		REG_GROUPED_PARAMETER_HOLD,
		GROUPED_PARAMETER_HOLD);
	if (rc < 0)
		return rc;

	if (mo == TEST_OFF)
		return 0;
	else {
		struct mt9p012_i2c_reg_conf test_tbl[] = {
			{REG_TEST_PATTERN_MODE, mo},
			{0x3044, 0x0544 & 0xFBFF},
			{0x30CA, 0x0004 | 0x0001},
			{0x30D4, 0x9020 & 0x7FFF},
			{0x31E0, 0x0003 & 0xFFFE},
			{0x3180, 0x91FF & 0x7FFF},
			{0x301A, (0x10CC | 0x8000) & 0xFFF7},
			{0x301E, 0x0000},
			{0x3780, 0x0000},
		};

		rc = mt9p012_i2c_write_w_table(&test_tbl[0],
			ARRAY_SIZE(test_tbl));
		if (rc < 0)
			return rc;
	}

	rc = mt9p012_i2c_write_w(mt9p012_ctrl->client->addr,
		REG_GROUPED_PARAMETER_HOLD,
		GROUPED_PARAMETER_UPDATE);
	if (rc < 0)
		return rc;

	return rc;
}

static int32_t mt9p012_set_lc(void)
{
	int32_t rc;

	struct mt9p012_i2c_reg_conf lc_tbl[] = {
		/* [Lens shading 85 Percent TL84] */
		/* P_RD_P0Q0 */
		{0x360A, 0x7FEF},
		/* P_RD_P0Q1 */
		{0x360C, 0x232C},
		/* P_RD_P0Q2 */
		{0x360E, 0x7050},
		/* P_RD_P0Q3 */
		{0x3610, 0xF3CC},
		/* P_RD_P0Q4 */
		{0x3612, 0x89D1},
		/* P_RD_P1Q0 */
		{0x364A, 0xBE0D},
		/* P_RD_P1Q1 */
		{0x364C, 0x9ACB},
		/* P_RD_P1Q2 */
		{0x364E, 0x2150},
		/* P_RD_P1Q3 */
		{0x3650, 0xB26B},
		/* P_RD_P1Q4 */
		{0x3652, 0x9511},
		/* P_RD_P2Q0 */
		{0x368A, 0x2151},
		/* P_RD_P2Q1 */
		{0x368C, 0x00AD},
		/* P_RD_P2Q2 */
		{0x368E, 0x8334},
		/* P_RD_P2Q3 */
		{0x3690, 0x478E},
		/* P_RD_P2Q4 */
		{0x3692, 0x0515},
		/* P_RD_P3Q0 */
		{0x36CA, 0x0710},
		/* P_RD_P3Q1 */
		{0x36CC, 0x452D},
		/* P_RD_P3Q2 */
		{0x36CE, 0xF352},
		/* P_RD_P3Q3 */
		{0x36D0, 0x190F},
		/* P_RD_P3Q4 */
		{0x36D2, 0x4413},
		/* P_RD_P4Q0 */
		{0x370A, 0xD112},
		/* P_RD_P4Q1 */
		{0x370C, 0xF50F},
		/* P_RD_P4Q2 */
		{0x370C, 0xF50F},
		/* P_RD_P4Q3 */
		{0x3710, 0xDC11},
		/* P_RD_P4Q4 */
		{0x3712, 0xD776},
		/* P_GR_P0Q0 */
		{0x3600, 0x1750},
		/* P_GR_P0Q1 */
		{0x3602, 0xF0AC},
		/* P_GR_P0Q2 */
		{0x3604, 0x4711},
		/* P_GR_P0Q3 */
		{0x3606, 0x07CE},
		/* P_GR_P0Q4 */
		{0x3608, 0x96B2},
		/* P_GR_P1Q0 */
		{0x3640, 0xA9AE},
		/* P_GR_P1Q1 */
		{0x3642, 0xF9AC},
		/* P_GR_P1Q2 */
		{0x3644, 0x39F1},
		/* P_GR_P1Q3 */
		{0x3646, 0x016F},
		/* P_GR_P1Q4 */
		{0x3648, 0x8AB2},
		/* P_GR_P2Q0 */
		{0x3680, 0x1752},
		/* P_GR_P2Q1 */
		{0x3682, 0x70F0},
		/* P_GR_P2Q2 */
		{0x3684, 0x83F5},
		/* P_GR_P2Q3 */
		{0x3686, 0x8392},
		/* P_GR_P2Q4 */
		{0x3688, 0x1FD6},
		/* P_GR_P3Q0 */
		{0x36C0, 0x1131},
		/* P_GR_P3Q1 */
		{0x36C2, 0x3DAF},
		/* P_GR_P3Q2 */
		{0x36C4, 0x89B4},
		/* P_GR_P3Q3 */
		{0x36C6, 0xA391},
		/* P_GR_P3Q4 */
		{0x36C8, 0x1334},
		/* P_GR_P4Q0 */
		{0x3700, 0xDC13},
		/* P_GR_P4Q1 */
		{0x3702, 0xD052},
		/* P_GR_P4Q2 */
		{0x3704, 0x5156},
		/* P_GR_P4Q3 */
		{0x3706, 0x1F13},
		/* P_GR_P4Q4 */
		{0x3708, 0x8C38},
		/* P_BL_P0Q0 */
		{0x3614, 0x0050},
		/* P_BL_P0Q1 */
		{0x3616, 0xBD4C},
		/* P_BL_P0Q2 */
		{0x3618, 0x41B0},
		/* P_BL_P0Q3 */
		{0x361A, 0x660D},
		/* P_BL_P0Q4 */
		{0x361C, 0xC590},
		/* P_BL_P1Q0 */
		{0x3654, 0x87EC},
		/* P_BL_P1Q1 */
		{0x3656, 0xE44C},
		/* P_BL_P1Q2 */
		{0x3658, 0x302E},
		/* P_BL_P1Q3 */
		{0x365A, 0x106E},
		/* P_BL_P1Q4 */
		{0x365C, 0xB58E},
		/* P_BL_P2Q0 */
		{0x3694, 0x0DD1},
		/* P_BL_P2Q1 */
		{0x3696, 0x2A50},
		/* P_BL_P2Q2 */
		{0x3698, 0xC793},
		/* P_BL_P2Q3 */
		{0x369A, 0xE8F1},
		/* P_BL_P2Q4 */
		{0x369C, 0x4174},
		/* P_BL_P3Q0 */
		{0x36D4, 0x01EF},
		/* P_BL_P3Q1 */
		{0x36D6, 0x06CF},
		/* P_BL_P3Q2 */
		{0x36D8, 0x8D91},
		/* P_BL_P3Q3 */
		{0x36DA, 0x91F0},
		/* P_BL_P3Q4 */
		{0x36DC, 0x52EF},
		/* P_BL_P4Q0 */
		{0x3714, 0xA6D2},
		/* P_BL_P4Q1 */
		{0x3716, 0xA312},
		/* P_BL_P4Q2 */
		{0x3718, 0x2695},
		/* P_BL_P4Q3 */
		{0x371A, 0x3953},
		/* P_BL_P4Q4 */
		{0x371C, 0x9356},
		/* P_GB_P0Q0 */
		{0x361E, 0x7EAF},
		/* P_GB_P0Q1 */
		{0x3620, 0x2A4C},
		/* P_GB_P0Q2 */
		{0x3622, 0x49F0},
		{0x3624, 0xF1EC},
		/* P_GB_P0Q4 */
		{0x3626, 0xC670},
		/* P_GB_P1Q0 */
		{0x365E, 0x8E0C},
		/* P_GB_P1Q1 */
		{0x3660, 0xC2A9},
		/* P_GB_P1Q2 */
		{0x3662, 0x274F},
		/* P_GB_P1Q3 */
		{0x3664, 0xADAB},
		/* P_GB_P1Q4 */
		{0x3666, 0x8EF0},
		/* P_GB_P2Q0 */
		{0x369E, 0x09B1},
		/* P_GB_P2Q1 */
		{0x36A0, 0xAA2E},
		/* P_GB_P2Q2 */
		{0x36A2, 0xC3D3},
		/* P_GB_P2Q3 */
		{0x36A4, 0x7FAF},
		/* P_GB_P2Q4 */
		{0x36A6, 0x3F34},
		/* P_GB_P3Q0 */
		{0x36DE, 0x4C8F},
		/* P_GB_P3Q1 */
		{0x36E0, 0x886E},
		/* P_GB_P3Q2 */
		{0x36E2, 0xE831},
		/* P_GB_P3Q3 */
		{0x36E4, 0x1FD0},
		/* P_GB_P3Q4 */
		{0x36E6, 0x1192},
		/* P_GB_P4Q0 */
		{0x371E, 0xB952},
		/* P_GB_P4Q1 */
		{0x3720, 0x6DCF},
		/* P_GB_P4Q2 */
		{0x3722, 0x1B55},
		/* P_GB_P4Q3 */
		{0x3724, 0xA112},
		/* P_GB_P4Q4 */
		{0x3726, 0x82F6},
		/* POLY_ORIGIN_C */
		{0x3782, 0x0510},
		/* POLY_ORIGIN_R  */
		{0x3784, 0x0390},
		/* POLY_SC_ENABLE */
		{0x3780, 0x8000},
	};

	/* rolloff table for illuminant A */
	struct mt9p012_i2c_reg_conf rolloff_tbl[] = {
		/* P_RD_P0Q0 */
		{0x360A, 0x7FEF},
		/* P_RD_P0Q1 */
		{0x360C, 0x232C},
		/* P_RD_P0Q2 */
		{0x360E, 0x7050},
		/* P_RD_P0Q3 */
		{0x3610, 0xF3CC},
		/* P_RD_P0Q4 */
		{0x3612, 0x89D1},
		/* P_RD_P1Q0 */
		{0x364A, 0xBE0D},
		/* P_RD_P1Q1 */
		{0x364C, 0x9ACB},
		/* P_RD_P1Q2 */
		{0x364E, 0x2150},
		/* P_RD_P1Q3 */
		{0x3650, 0xB26B},
		/* P_RD_P1Q4 */
		{0x3652, 0x9511},
		/* P_RD_P2Q0 */
		{0x368A, 0x2151},
		/* P_RD_P2Q1 */
		{0x368C, 0x00AD},
		/* P_RD_P2Q2 */
		{0x368E, 0x8334},
		/* P_RD_P2Q3 */
		{0x3690, 0x478E},
		/* P_RD_P2Q4 */
		{0x3692, 0x0515},
		/* P_RD_P3Q0 */
		{0x36CA, 0x0710},
		/* P_RD_P3Q1 */
		{0x36CC, 0x452D},
		/* P_RD_P3Q2 */
		{0x36CE, 0xF352},
		/* P_RD_P3Q3 */
		{0x36D0, 0x190F},
		/* P_RD_P3Q4 */
		{0x36D2, 0x4413},
		/* P_RD_P4Q0 */
		{0x370A, 0xD112},
		/* P_RD_P4Q1 */
		{0x370C, 0xF50F},
		/* P_RD_P4Q2 */
		{0x370E, 0x6375},
		/* P_RD_P4Q3 */
		{0x3710, 0xDC11},
		/* P_RD_P4Q4 */
		{0x3712, 0xD776},
		/* P_GR_P0Q0 */
		{0x3600, 0x1750},
		/* P_GR_P0Q1 */
		{0x3602, 0xF0AC},
		/* P_GR_P0Q2 */
		{0x3604, 0x4711},
		/* P_GR_P0Q3 */
		{0x3606, 0x07CE},
		/* P_GR_P0Q4 */
		{0x3608, 0x96B2},
		/* P_GR_P1Q0 */
		{0x3640, 0xA9AE},
		/* P_GR_P1Q1 */
		{0x3642, 0xF9AC},
		/* P_GR_P1Q2 */
		{0x3644, 0x39F1},
		/* P_GR_P1Q3 */
		{0x3646, 0x016F},
		/* P_GR_P1Q4 */
		{0x3648, 0x8AB2},
		/* P_GR_P2Q0 */
		{0x3680, 0x1752},
		/* P_GR_P2Q1 */
		{0x3682, 0x70F0},
		/* P_GR_P2Q2 */
		{0x3684, 0x83F5},
		/* P_GR_P2Q3 */
		{0x3686, 0x8392},
		/* P_GR_P2Q4 */
		{0x3688, 0x1FD6},
		/* P_GR_P3Q0 */
		{0x36C0, 0x1131},
		/* P_GR_P3Q1 */
		{0x36C2, 0x3DAF},
		/* P_GR_P3Q2 */
		{0x36C4, 0x89B4},
		/* P_GR_P3Q3 */
		{0x36C6, 0xA391},
		/* P_GR_P3Q4 */
		{0x36C8, 0x1334},
		/* P_GR_P4Q0 */
		{0x3700, 0xDC13},
		/* P_GR_P4Q1 */
		{0x3702, 0xD052},
		/* P_GR_P4Q2 */
		{0x3704, 0x5156},
		/* P_GR_P4Q3 */
		{0x3706, 0x1F13},
		/* P_GR_P4Q4 */
		{0x3708, 0x8C38},
		/* P_BL_P0Q0 */
		{0x3614, 0x0050},
		/* P_BL_P0Q1 */
		{0x3616, 0xBD4C},
		/* P_BL_P0Q2 */
		{0x3618, 0x41B0},
		/* P_BL_P0Q3 */
		{0x361A, 0x660D},
		/* P_BL_P0Q4 */
		{0x361C, 0xC590},
		/* P_BL_P1Q0 */
		{0x3654, 0x87EC},
		/* P_BL_P1Q1 */
		{0x3656, 0xE44C},
		/* P_BL_P1Q2 */
		{0x3658, 0x302E},
		/* P_BL_P1Q3 */
		{0x365A, 0x106E},
		/* P_BL_P1Q4 */
		{0x365C, 0xB58E},
		/* P_BL_P2Q0 */
		{0x3694, 0x0DD1},
		/* P_BL_P2Q1 */
		{0x3696, 0x2A50},
		/* P_BL_P2Q2 */
		{0x3698, 0xC793},
		/* P_BL_P2Q3 */
		{0x369A, 0xE8F1},
		/* P_BL_P2Q4 */
		{0x369C, 0x4174},
		/* P_BL_P3Q0 */
		{0x36D4, 0x01EF},
		/* P_BL_P3Q1 */
		{0x36D6, 0x06CF},
		/* P_BL_P3Q2 */
		{0x36D8, 0x8D91},
		/* P_BL_P3Q3 */
		{0x36DA, 0x91F0},
		/* P_BL_P3Q4 */
		{0x36DC, 0x52EF},
		/* P_BL_P4Q0 */
		{0x3714, 0xA6D2},
		/* P_BL_P4Q1 */
		{0x3716, 0xA312},
		/* P_BL_P4Q2 */
		{0x3718, 0x2695},
		/* P_BL_P4Q3 */
		{0x371A, 0x3953},
		/* P_BL_P4Q4 */
		{0x371C, 0x9356},
		/* P_GB_P0Q0 */
		{0x361E, 0x7EAF},
		/* P_GB_P0Q1 */
		{0x3620, 0x2A4C},
		/* P_GB_P0Q2 */
		{0x3622, 0x49F0},
		{0x3624, 0xF1EC},
		/* P_GB_P0Q4 */
		{0x3626, 0xC670},
		/* P_GB_P1Q0 */
		{0x365E, 0x8E0C},
		/* P_GB_P1Q1 */
		{0x3660, 0xC2A9},
		/* P_GB_P1Q2 */
		{0x3662, 0x274F},
		/* P_GB_P1Q3 */
		{0x3664, 0xADAB},
		/* P_GB_P1Q4 */
		{0x3666, 0x8EF0},
		/* P_GB_P2Q0 */
		{0x369E, 0x09B1},
		/* P_GB_P2Q1 */
		{0x36A0, 0xAA2E},
		/* P_GB_P2Q2 */
		{0x36A2, 0xC3D3},
		/* P_GB_P2Q3 */
		{0x36A4, 0x7FAF},
		/* P_GB_P2Q4 */
		{0x36A6, 0x3F34},
		/* P_GB_P3Q0 */
		{0x36DE, 0x4C8F},
		/* P_GB_P3Q1 */
		{0x36E0, 0x886E},
		/* P_GB_P3Q2 */
		{0x36E2, 0xE831},
		/* P_GB_P3Q3 */
		{0x36E4, 0x1FD0},
		/* P_GB_P3Q4 */
		{0x36E6, 0x1192},
		/* P_GB_P4Q0 */
		{0x371E, 0xB952},
		/* P_GB_P4Q1 */
		{0x3720, 0x6DCF},
		/* P_GB_P4Q2 */
		{0x3722, 0x1B55},
		/* P_GB_P4Q3 */
		{0x3724, 0xA112},
		/* P_GB_P4Q4 */
		{0x3726, 0x82F6},
		/* POLY_ORIGIN_C */
		{0x3782, 0x0510},
		/* POLY_ORIGIN_R  */
		{0x3784, 0x0390},
		/* POLY_SC_ENABLE */
		{0x3780, 0x8000},
	};

	rc = mt9p012_i2c_write_w_table(&lc_tbl[0],
		ARRAY_SIZE(lc_tbl));
	if (rc < 0)
		return rc;

	rc = mt9p012_i2c_write_w_table(&rolloff_tbl[0],
		ARRAY_SIZE(rolloff_tbl));

	return rc;
}

static void mt9p012_get_pict_fps(uint16_t fps, uint16_t *pfps)
{
	/* input fps is preview fps in Q8 format */
	uint32_t divider;   /*Q10 */
	uint32_t pclk_mult; /*Q10 */

	if (mt9p012_ctrl->prev_res == QTR_SIZE) {
		divider = (uint32_t)
		(((mt9p012_reg_pat[RES_PREVIEW].frame_length_lines *
		mt9p012_reg_pat[RES_PREVIEW].line_length_pck) * 0x00000400) /
		(mt9p012_reg_pat[RES_CAPTURE].frame_length_lines *
		mt9p012_reg_pat[RES_CAPTURE].line_length_pck));

		pclk_mult =
		(uint32_t) ((mt9p012_reg_pat[RES_CAPTURE].pll_multiplier *
		0x00000400) / (mt9p012_reg_pat[RES_PREVIEW].pll_multiplier));
	} else {
		/* full size resolution used for preview. */
		divider   = 0x00000400;  /*1.0 */
		pclk_mult = 0x00000400;  /*1.0 */
	}

	/* Verify PCLK settings and frame sizes. */
	*pfps = (uint16_t) (fps * divider * pclk_mult / 0x00000400 /
		0x00000400);
}

static uint16_t mt9p012_get_prev_lines_pf(void)
{
	if (mt9p012_ctrl->prev_res == QTR_SIZE)
		return mt9p012_reg_pat[RES_PREVIEW].frame_length_lines;
	else
		return mt9p012_reg_pat[RES_CAPTURE].frame_length_lines;
}

static uint16_t mt9p012_get_prev_pixels_pl(void)
{
	if (mt9p012_ctrl->prev_res == QTR_SIZE)
		return mt9p012_reg_pat[RES_PREVIEW].line_length_pck;
	else
		return mt9p012_reg_pat[RES_CAPTURE].line_length_pck;
}

static uint16_t mt9p012_get_pict_lines_pf(void)
{
	return mt9p012_reg_pat[RES_CAPTURE].frame_length_lines;
}

static uint16_t mt9p012_get_pict_pixels_pl(void)
{
	return mt9p012_reg_pat[RES_CAPTURE].line_length_pck;
}

static uint32_t mt9p012_get_pict_max_exp_lc(void)
{
	uint16_t snapshot_lines_per_frame;

	if (mt9p012_ctrl->pict_res == QTR_SIZE)
		snapshot_lines_per_frame =
		mt9p012_reg_pat[RES_PREVIEW].frame_length_lines - 1;
	else
		snapshot_lines_per_frame =
		mt9p012_reg_pat[RES_CAPTURE].frame_length_lines - 1;

	return snapshot_lines_per_frame * 24;
}

static int32_t mt9p012_set_fps(struct fps_cfg *fps)
{
	/* input is new fps in Q8 format */
	int32_t rc = 0;

	mt9p012_ctrl->fps_divider = fps->fps_div;
	mt9p012_ctrl->pict_fps_divider = fps->pict_fps_div;

	rc =
		mt9p012_i2c_write_w(mt9p012_ctrl->client->addr,
			REG_GROUPED_PARAMETER_HOLD,
			GROUPED_PARAMETER_HOLD);
	if (rc < 0)
		return -EBUSY;

	rc =
		mt9p012_i2c_write_w(mt9p012_ctrl->client->addr,
			REG_LINE_LENGTH_PCK,
			(mt9p012_reg_pat[RES_PREVIEW].line_length_pck *
			fps->f_mult / 0x00000400));
	if (rc < 0)
		return rc;

	rc =
		mt9p012_i2c_write_w(mt9p012_ctrl->client->addr,
			REG_GROUPED_PARAMETER_HOLD,
			GROUPED_PARAMETER_UPDATE);

	return rc;
}

static int32_t mt9p012_write_exp_gain(uint16_t gain, uint32_t line)
{
	uint16_t max_legal_gain = 0x01FF;
	uint32_t line_length_ratio = 0x00000400;
	enum mt9p012_setting_t setting;
	int32_t rc = 0;

	CDBG("Line:%d mt9p012_write_exp_gain \n", __LINE__);

	if (mt9p012_ctrl->sensormode == SENSOR_PREVIEW_MODE) {
		mt9p012_ctrl->my_reg_gain = gain;
		mt9p012_ctrl->my_reg_line_count = (uint16_t)line;
	}

	if (gain > max_legal_gain) {
		CDBG("Max legal gain Line:%d \n", __LINE__);
		gain = max_legal_gain;
	}

	/* Verify no overflow */
	if (mt9p012_ctrl->sensormode != SENSOR_SNAPSHOT_MODE) {
		line = (uint32_t)(line * mt9p012_ctrl->fps_divider /
			0x00000400);
		setting = RES_PREVIEW;
	} else {
		line = (uint32_t)(line * mt9p012_ctrl->pict_fps_divider /
			0x00000400);
		setting = RES_CAPTURE;
	}

	/* Set digital gain to 1 */
#ifdef MT9P012_REV_7
	gain |= 0x1000;
#else
	gain |= 0x0200;
#endif

	if ((mt9p012_reg_pat[setting].frame_length_lines - 1) < line) {
		line_length_ratio = (uint32_t) (line * 0x00000400) /
		(mt9p012_reg_pat[setting].frame_length_lines - 1);
	} else
		line_length_ratio = 0x00000400;

	rc =
		mt9p012_i2c_write_w(mt9p012_ctrl->client->addr,
			REG_GROUPED_PARAMETER_HOLD,
			GROUPED_PARAMETER_HOLD);
	if (rc < 0) {
		CDBG("mt9p012_i2c_write_w failed... Line:%d \n", __LINE__);
		return rc;
	}

	rc =
		mt9p012_i2c_write_w(
			mt9p012_ctrl->client->addr,
			REG_GLOBAL_GAIN, gain);
	if (rc < 0) {
		CDBG("mt9p012_i2c_write_w failed... Line:%d \n", __LINE__);
		return rc;
	}

	rc =
		mt9p012_i2c_write_w(mt9p012_ctrl->client->addr,
			REG_COARSE_INT_TIME,
			line);
	if (rc < 0) {
		CDBG("mt9p012_i2c_write_w failed... Line:%d \n", __LINE__);
		return rc;
	}

	CDBG("mt9p012_write_exp_gain: gain = %d, line = %d\n", gain, line);

	rc =
		mt9p012_i2c_write_w(mt9p012_ctrl->client->addr,
			REG_GROUPED_PARAMETER_HOLD,
			GROUPED_PARAMETER_UPDATE);
	if (rc < 0)
		CDBG("mt9p012_i2c_write_w failed... Line:%d \n", __LINE__);

	return rc;
}

static int32_t mt9p012_set_pict_exp_gain(uint16_t gain, uint32_t line)
{
	int32_t rc = 0;

	CDBG("Line:%d mt9p012_set_pict_exp_gain \n", __LINE__);

	rc =
		mt9p012_write_exp_gain(gain, line);
	if (rc < 0) {
		CDBG("Line:%d mt9p012_set_pict_exp_gain failed... \n",
			__LINE__);
		return rc;
	}

	rc =
	mt9p012_i2c_write_w(mt9p012_ctrl->client->addr,
		MT9P012_REG_RESET_REGISTER,
		0x10CC | 0x0002);
	if (rc < 0) {
		CDBG("mt9p012_i2c_write_w failed... Line:%d \n", __LINE__);
		return rc;
	}

	mdelay(5);

	/* camera_timed_wait(snapshot_wait*exposure_ratio); */
	return rc;
}

static int32_t mt9p012_setting(enum mt9p012_reg_update_t rupdate,
	enum mt9p012_setting_t rt)
{
	int32_t rc = 0;

	switch (rupdate) {
	case UPDATE_PERIODIC: {
	  if (rt == RES_PREVIEW || rt == RES_CAPTURE) {

		struct mt9p012_i2c_reg_conf ppc_tbl[] = {
		{REG_GROUPED_PARAMETER_HOLD, GROUPED_PARAMETER_HOLD},
		{REG_ROW_SPEED, mt9p012_reg_pat[rt].row_speed},
		{REG_X_ADDR_START, mt9p012_reg_pat[rt].x_addr_start},
		{REG_X_ADDR_END, mt9p012_reg_pat[rt].x_addr_end},
		{REG_Y_ADDR_START, mt9p012_reg_pat[rt].y_addr_start},
		{REG_Y_ADDR_END, mt9p012_reg_pat[rt].y_addr_end},
		{REG_READ_MODE, mt9p012_reg_pat[rt].read_mode},
		{REG_SCALE_M, mt9p012_reg_pat[rt].scale_m},
		{REG_X_OUTPUT_SIZE, mt9p012_reg_pat[rt].x_output_size},
		{REG_Y_OUTPUT_SIZE, mt9p012_reg_pat[rt].y_output_size},

		{REG_LINE_LENGTH_PCK, mt9p012_reg_pat[rt].line_length_pck},
		{REG_FRAME_LENGTH_LINES,
			(mt9p012_reg_pat[rt].frame_length_lines *
			mt9p012_ctrl->fps_divider / 0x00000400)},
		{REG_COARSE_INT_TIME, mt9p012_reg_pat[rt].coarse_int_time},
		{REG_FINE_INTEGRATION_TIME, mt9p012_reg_pat[rt].fine_int_time},
		{REG_GROUPED_PARAMETER_HOLD, GROUPED_PARAMETER_UPDATE},
		};

		rc = mt9p012_i2c_write_w_table(&ppc_tbl[0],
			ARRAY_SIZE(ppc_tbl));
		if (rc < 0)
			return rc;

		rc = mt9p012_test(mt9p012_ctrl->set_test);
		if (rc < 0)
			return rc;

		rc =
			mt9p012_i2c_write_w(mt9p012_ctrl->client->addr,
			MT9P012_REG_RESET_REGISTER,
			MT9P012_RESET_REGISTER_PWON | 0x0002);
		if (rc < 0)
			return rc;

		mdelay(5); /* 15? wait for sensor to transition*/

		return rc;
	  }
	}
    break; /* UPDATE_PERIODIC */

	case REG_INIT: {
	    if (rt == RES_PREVIEW || rt == RES_CAPTURE) {
		struct mt9p012_i2c_reg_conf ipc_tbl1[] = {
		{MT9P012_REG_RESET_REGISTER, MT9P012_RESET_REGISTER_PWOFF},
		{REG_VT_PIX_CLK_DIV, mt9p012_reg_pat[rt].vt_pix_clk_div},
		{REG_VT_SYS_CLK_DIV, mt9p012_reg_pat[rt].vt_sys_clk_div},
		{REG_PRE_PLL_CLK_DIV, mt9p012_reg_pat[rt].pre_pll_clk_div},
		{REG_PLL_MULTIPLIER, mt9p012_reg_pat[rt].pll_multiplier},
		{REG_OP_PIX_CLK_DIV, mt9p012_reg_pat[rt].op_pix_clk_div},
		{REG_OP_SYS_CLK_DIV, mt9p012_reg_pat[rt].op_sys_clk_div},
#ifdef MT9P012_REV_7
		{0x30B0, 0x0001},
		{0x308E, 0xE060},
		{0x3092, 0x0A52},
		{0x3094, 0x4656},
		{0x3096, 0x5652},
		{0x30CA, 0x8006},
		{0x312A, 0xDD02},
		{0x312C, 0x00E4},
		{0x3170, 0x299A},
#endif
		/* optimized settings for noise */
		{0x3088, 0x6FF6},
		{0x3154, 0x0282},
		{0x3156, 0x0381},
		{0x3162, 0x04CE},
		{0x0204, 0x0010},
		{0x0206, 0x0010},
		{0x0208, 0x0010},
		{0x020A, 0x0010},
		{0x020C, 0x0010},
		{MT9P012_REG_RESET_REGISTER, MT9P012_RESET_REGISTER_PWON},
		};

		struct mt9p012_i2c_reg_conf ipc_tbl2[] = {
		{MT9P012_REG_RESET_REGISTER, MT9P012_RESET_REGISTER_PWOFF},
		{REG_VT_PIX_CLK_DIV, mt9p012_reg_pat[rt].vt_pix_clk_div},
		{REG_VT_SYS_CLK_DIV, mt9p012_reg_pat[rt].vt_sys_clk_div},
		{REG_PRE_PLL_CLK_DIV, mt9p012_reg_pat[rt].pre_pll_clk_div},
		{REG_PLL_MULTIPLIER, mt9p012_reg_pat[rt].pll_multiplier},
		{REG_OP_PIX_CLK_DIV, mt9p012_reg_pat[rt].op_pix_clk_div},
		{REG_OP_SYS_CLK_DIV, mt9p012_reg_pat[rt].op_sys_clk_div},
#ifdef MT9P012_REV_7
		{0x30B0, 0x0001},
		{0x308E, 0xE060},
		{0x3092, 0x0A52},
		{0x3094, 0x4656},
		{0x3096, 0x5652},
		{0x30CA, 0x8006},
		{0x312A, 0xDD02},
		{0x312C, 0x00E4},
		{0x3170, 0x299A},
#endif
		/* optimized settings for noise */
		{0x3088, 0x6FF6},
		{0x3154, 0x0282},
		{0x3156, 0x0381},
		{0x3162, 0x04CE},
		{0x0204, 0x0010},
		{0x0206, 0x0010},
		{0x0208, 0x0010},
		{0x020A, 0x0010},
		{0x020C, 0x0010},
		{MT9P012_REG_RESET_REGISTER, MT9P012_RESET_REGISTER_PWON},
		};

		struct mt9p012_i2c_reg_conf ipc_tbl3[] = {
		{REG_GROUPED_PARAMETER_HOLD, GROUPED_PARAMETER_HOLD},
		/* Set preview or snapshot mode */
		{REG_ROW_SPEED, mt9p012_reg_pat[rt].row_speed},
		{REG_X_ADDR_START, mt9p012_reg_pat[rt].x_addr_start},
		{REG_X_ADDR_END, mt9p012_reg_pat[rt].x_addr_end},
		{REG_Y_ADDR_START, mt9p012_reg_pat[rt].y_addr_start},
		{REG_Y_ADDR_END, mt9p012_reg_pat[rt].y_addr_end},
		{REG_READ_MODE, mt9p012_reg_pat[rt].read_mode},
		{REG_SCALE_M, mt9p012_reg_pat[rt].scale_m},
		{REG_X_OUTPUT_SIZE, mt9p012_reg_pat[rt].x_output_size},
		{REG_Y_OUTPUT_SIZE, mt9p012_reg_pat[rt].y_output_size},
		{REG_LINE_LENGTH_PCK, mt9p012_reg_pat[rt].line_length_pck},
		{REG_FRAME_LENGTH_LINES,
			mt9p012_reg_pat[rt].frame_length_lines},
		{REG_COARSE_INT_TIME, mt9p012_reg_pat[rt].coarse_int_time},
		{REG_FINE_INTEGRATION_TIME, mt9p012_reg_pat[rt].fine_int_time},
		{REG_GROUPED_PARAMETER_HOLD, GROUPED_PARAMETER_UPDATE},
		};

		/* reset fps_divider */
		mt9p012_ctrl->fps_divider = 1 * 0x0400;

		rc = mt9p012_i2c_write_w_table(&ipc_tbl1[0],
			ARRAY_SIZE(ipc_tbl1));
		if (rc < 0)
			return rc;

		rc = mt9p012_i2c_write_w_table(&ipc_tbl2[0],
			ARRAY_SIZE(ipc_tbl2));
		if (rc < 0)
			return rc;

		mdelay(5);

		rc = mt9p012_i2c_write_w_table(&ipc_tbl3[0],
			ARRAY_SIZE(ipc_tbl3));
		if (rc < 0)
			return rc;
	    }
	}
		break; /* case REG_INIT: */

	default:
		rc = -EFAULT;
		break;
	} /* switch (rupdate) */

	return rc;
}

static int32_t mt9p012_video_config(enum sensor_mode_t mode,
	enum sensor_resolution_t res)
{
	int32_t rc;

	switch (res) {
	case QTR_SIZE:
		rc = mt9p012_setting(UPDATE_PERIODIC, RES_PREVIEW);
		if (rc < 0)
			return rc;

		CDBG("mt9p012 sensor configuration done!\n");
		break;

	case FULL_SIZE:
		rc =
		mt9p012_setting(UPDATE_PERIODIC, RES_CAPTURE);
		if (rc < 0)
			return rc;

		break;

	default:
		return 0;
	} /* switch */

	mt9p012_ctrl->prev_res = res;
	mt9p012_ctrl->curr_res = res;
	mt9p012_ctrl->sensormode = mode;

	rc =
		mt9p012_write_exp_gain(mt9p012_ctrl->my_reg_gain,
			mt9p012_ctrl->my_reg_line_count);

	rc =
		mt9p012_i2c_write_w(mt9p012_ctrl->client->addr,
			MT9P012_REG_RESET_REGISTER,
			0x10cc|0x0002);

	return rc;
}

static int32_t mt9p012_snapshot_config(enum sensor_mode_t mode)
{
	int32_t rc = 0;

	rc = mt9p012_setting(UPDATE_PERIODIC, RES_CAPTURE);
	if (rc < 0)
		return rc;

	mt9p012_ctrl->curr_res = mt9p012_ctrl->pict_res;

	mt9p012_ctrl->sensormode = mode;

	return rc;
}

static int32_t mt9p012_raw_snapshot_config(enum sensor_mode_t mode)
{
	int32_t rc = 0;

	rc = mt9p012_setting(UPDATE_PERIODIC, RES_CAPTURE);
	if (rc < 0)
		return rc;

	mt9p012_ctrl->curr_res = mt9p012_ctrl->pict_res;

	mt9p012_ctrl->sensormode = mode;

	return rc;
}

static int32_t mt9p012_power_down(void)
{
	int32_t rc = 0;

	rc = mt9p012_i2c_write_w(mt9p012_ctrl->client->addr,
		MT9P012_REG_RESET_REGISTER,
		MT9P012_RESET_REGISTER_PWOFF);

	mdelay(5);
	return rc;
}

static int32_t mt9p012_move_focus(enum sensor_move_focus_t direction,
	int32_t num_steps)
{
	int16_t step_direction;
	int16_t actual_step;
	int16_t next_position;
	uint8_t code_val_msb, code_val_lsb;

	if (num_steps > MT9P012_TOTAL_STEPS_NEAR_TO_FAR)
		num_steps = MT9P012_TOTAL_STEPS_NEAR_TO_FAR;
	else if (num_steps == 0) {
		CDBG("mt9p012_move_focus failed at line %d ...\n", __LINE__);
		return -EINVAL;
	}

	if (direction == MOVE_NEAR)
		step_direction = 16; /* 10bit */
	else if (direction == MOVE_FAR)
		step_direction = -16; /* 10 bit */
	else {
		CDBG("mt9p012_move_focus failed at line %d ...\n", __LINE__);
		return -EINVAL;
	}

	if (mt9p012_ctrl->curr_lens_pos < mt9p012_ctrl->init_curr_lens_pos)
		mt9p012_ctrl->curr_lens_pos =
			mt9p012_ctrl->init_curr_lens_pos;

	actual_step = (int16_t)(step_direction * (int16_t)num_steps);
	next_position = (int16_t)(mt9p012_ctrl->curr_lens_pos + actual_step);

	if (next_position > 1023)
		next_position = 1023;
	else if (next_position < 0)
		next_position = 0;

	code_val_msb = next_position >> 4;
	code_val_lsb = (next_position & 0x000F) << 4;
	/* code_val_lsb |= mode_mask; */

	/* Writing the digital code for current to the actuator */
	if (mt9p012_i2c_write_b(MT9P012_AF_I2C_ADDR >> 1,
		code_val_msb, code_val_lsb) < 0) {
		CDBG("mt9p012_move_focus failed at line %d ...\n", __LINE__);
		return -EBUSY;
	}

	/* Storing the current lens Position */
	mt9p012_ctrl->curr_lens_pos = next_position;

	return 0;
}

static int32_t mt9p012_set_default_focus(void)
{
	int32_t rc = 0;
	uint8_t code_val_msb, code_val_lsb;

	code_val_msb = 0x00;
	code_val_lsb = 0x00;

	/* Write the digital code for current to the actuator */
	rc = mt9p012_i2c_write_b(MT9P012_AF_I2C_ADDR >> 1,
		code_val_msb, code_val_lsb);

	mt9p012_ctrl->curr_lens_pos = 0;
	mt9p012_ctrl->init_curr_lens_pos = 0;

	return rc;
}

static int32_t mt9p012_sensor_init(
	struct msm_camera_sensor_info *camdev)
{
	int32_t  rc;
	uint16_t chipid;

	rc = gpio_request(mt9p012_ctrl->sensordata->sensor_reset, "mt9p012");
	if (!rc)
		gpio_direction_output(mt9p012_ctrl->sensordata->sensor_reset,
				1);
	else
		goto init_done;

	mdelay(20);

	/* enable AF actuator */
	CDBG("enable AF actuator, gpio = %d\n",
		mt9p012_ctrl->sensordata->vcm_pwd);
	rc = gpio_request(mt9p012_ctrl->sensordata->vcm_pwd, "mt9p012");
	if (!rc)
		gpio_direction_output(mt9p012_ctrl->sensordata->vcm_pwd, 1);
	else {
		CDBG("mt9p012_ctrl gpio request failed!\n");
		goto init_fail1;
	}

	mdelay(20);

	/* enable mclk first */
	msm_camio_clk_rate_set(MT9P012_DEFAULT_CLOCK_RATE);

	mdelay(20);

	/* RESET the sensor image part via I2C command */
	CDBG("mt9p012_sensor_init(): reseting sensor.\n");
	rc = mt9p012_i2c_write_w(mt9p012_ctrl->client->addr,
		MT9P012_REG_RESET_REGISTER,
		0x10CC|0x0001);
	if (rc < 0) {
		CDBG("sensor reset failed. rc = %d\n", rc);
		goto init_fail2;
	}

	mdelay(MT9P012_RESET_DELAY_MSECS);

	/* 3. Read sensor Model ID: */
	rc = mt9p012_i2c_read_w(mt9p012_ctrl->client->addr,
		MT9P012_REG_MODEL_ID, &chipid);
	if (rc < 0)
		goto init_fail2;

	CDBG("mt9p012 model_id = 0x%x\n", chipid);

	/* 4. Compare sensor ID to MT9T012VC ID: */
	if (chipid != MT9P012_MODEL_ID) {
		rc = -ENODEV;
		goto init_fail2;
	}

	rc = mt9p012_i2c_write_w(mt9p012_ctrl->client->addr, 0x306E, 0x9000);
	if (rc < 0) {
		CDBG("REV_7 write failed. rc = %d\n", rc);
		goto init_fail2;
	}

	/* RESET_REGISTER, enable parallel interface and disable serialiser */
	CDBG("mt9p012_sensor_init(): enabling parallel interface.\n");
	rc = mt9p012_i2c_write_w(mt9p012_ctrl->client->addr, 0x301A, 0x10CC);
	if (rc < 0) {
		CDBG("enable parallel interface failed. rc = %d\n", rc);
		goto init_fail2;
	}

	/* To disable the 2 extra lines */
	rc = mt9p012_i2c_write_w(mt9p012_ctrl->client->addr,
		0x3064, 0x0805);

	if (rc < 0) {
		CDBG("disable the 2 extra lines failed. rc = %d\n", rc);
		goto init_fail2;
	}

	mdelay(MT9P012_RESET_DELAY_MSECS);

	if (mt9p012_ctrl->prev_res == QTR_SIZE)
		rc = mt9p012_setting(REG_INIT, RES_PREVIEW);
	else
		rc = mt9p012_setting(REG_INIT, RES_CAPTURE);

	if (rc < 0) {
		CDBG("mt9p012_setting failed. rc = %d\n", rc);
		goto init_fail2;
	}

	/* sensor : output enable */
	CDBG("mt9p012_sensor_init(): enabling output.\n");
	rc = mt9p012_i2c_write_w(mt9p012_ctrl->client->addr,
		MT9P012_REG_RESET_REGISTER, MT9P012_RESET_REGISTER_PWON);

	if (rc < 0) {
		CDBG("sensor output enable failed. rc = %d\n", rc);
		goto init_fail2;
	}

	rc = mt9p012_set_default_focus();
	if (rc >= 0)
		goto init_done;

init_fail2:
	gpio_direction_output(mt9p012_ctrl->sensordata->vcm_pwd,
			0);
	gpio_free(mt9p012_ctrl->sensordata->vcm_pwd);
init_fail1:
	gpio_direction_output(mt9p012_ctrl->sensordata->sensor_reset,
			0);
	gpio_free(mt9p012_ctrl->sensordata->sensor_reset);
init_done:
	return rc;
}

static int mt9p012_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&mt9p012_wait_queue);
	return 0;
}

static int32_t mt9p012_set_sensor_mode(enum sensor_mode_t mode,
					enum sensor_resolution_t res)
{
	int32_t rc = 0;

	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		rc = mt9p012_video_config(mode, res);
		break;

	case SENSOR_SNAPSHOT_MODE:
		rc = mt9p012_snapshot_config(mode);
		break;

	case SENSOR_RAW_SNAPSHOT_MODE:
		rc = mt9p012_raw_snapshot_config(mode);
		break;

	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int mt9p012_open(struct inode *inode, struct file *fp)
{
	int32_t rc = 0;

	down(&mt9p012_sem);
	CDBG("mt9p012: open = %d\n", mt9p012_ctrl->opened);

	if (mt9p012_ctrl->opened) {
		rc = 0;
		goto open_done;
	}

	rc =
		mt9p012_sensor_init(mt9p012_ctrl->sensordata);

	CDBG("mt9p012_open: sensor init rc = %d\n", rc);

	if (rc >= 0)
		mt9p012_ctrl->opened = 1;
	else
		CDBG("mt9p012_open: sensor init failed!\n");

open_done:
	up(&mt9p012_sem);
	return rc;
}

static long mt9p012_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct sensor_cfg_data_t cdata;
	long   rc = 0;

	if (copy_from_user(&cdata,
			(void *)argp,
			sizeof(struct sensor_cfg_data_t)))
		return -EFAULT;

	down(&mt9p012_sem);

	switch (cmd) {
	case MSM_CAMSENSOR_IO_CFG: {
		switch (cdata.cfgtype) {
		case CFG_GET_PICT_FPS:
			mt9p012_get_pict_fps(cdata.cfg.gfps.prevfps,
				&(cdata.cfg.gfps.pictfps));

			if (copy_to_user((void *)argp, &cdata,
					sizeof(struct sensor_cfg_data_t)))
				rc = -EFAULT;
			break;

		case CFG_GET_PREV_L_PF:
			cdata.cfg.prevl_pf = mt9p012_get_prev_lines_pf();

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data_t)))
				rc = -EFAULT;
			break;

		case CFG_GET_PREV_P_PL:
			cdata.cfg.prevp_pl = mt9p012_get_prev_pixels_pl();

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data_t)))
				rc = -EFAULT;
			break;

		case CFG_GET_PICT_L_PF:
			cdata.cfg.pictl_pf = mt9p012_get_pict_lines_pf();

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data_t)))
				rc = -EFAULT;
			break;

		case CFG_GET_PICT_P_PL:
			cdata.cfg.pictp_pl = mt9p012_get_pict_pixels_pl();

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data_t)))
				rc = -EFAULT;
			break;

		case CFG_GET_PICT_MAX_EXP_LC:
			cdata.cfg.pict_max_exp_lc =
				mt9p012_get_pict_max_exp_lc();

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data_t)))
				rc = -EFAULT;
			break;

		case CFG_SET_FPS:
		case CFG_SET_PICT_FPS:
			rc = mt9p012_set_fps(&(cdata.cfg.fps));
			break;

		case CFG_SET_EXP_GAIN:
			rc =
				mt9p012_write_exp_gain(cdata.cfg.exp_gain.gain,
					cdata.cfg.exp_gain.line);
			break;

		case CFG_SET_PICT_EXP_GAIN:
			CDBG("Line:%d CFG_SET_PICT_EXP_GAIN \n", __LINE__);
			rc =
				mt9p012_set_pict_exp_gain(
					cdata.cfg.exp_gain.gain,
					cdata.cfg.exp_gain.line);
			break;

		case CFG_SET_MODE:
			rc =
				mt9p012_set_sensor_mode(
				cdata.mode, cdata.rs);
			break;

		case CFG_PWR_DOWN:
			rc = mt9p012_power_down();
			break;

		case CFG_MOVE_FOCUS:
			rc =
				mt9p012_move_focus(
				cdata.cfg.focus.dir,
				cdata.cfg.focus.steps);
			break;

		case CFG_SET_DEFAULT_FOCUS:
			rc =
				mt9p012_set_default_focus();

			break;

		case CFG_SET_EFFECT:
			rc =
				mt9p012_set_default_focus();
			break;

		default:
			rc = -EFAULT;
			break;
		}
	}
	break;

	default:
		rc = -EFAULT;
		break;
	}

	up(&mt9p012_sem);
	return rc;
}

static int mt9p012_release(struct inode *ip, struct file *fp)
{
	int rc = -EBADF;

	down(&mt9p012_sem);
	if (!mt9p012_ctrl->opened)
		goto release_done;

	mt9p012_power_down();

	gpio_direction_output(mt9p012_ctrl->sensordata->sensor_reset,
		0);
	gpio_free(mt9p012_ctrl->sensordata->sensor_reset);

	gpio_direction_output(mt9p012_ctrl->sensordata->vcm_pwd, 0);
	gpio_free(mt9p012_ctrl->sensordata->vcm_pwd);

	rc = mt9p012_ctrl->opened = 0;

	CDBG("mt9p012_release completed\n");

release_done:
	up(&mt9p012_sem);
	return rc;
}

static struct file_operations mt9p012_fops = {
	.owner  = THIS_MODULE,
	.open   = mt9p012_open,
	.release = mt9p012_release,
	.unlocked_ioctl = mt9p012_ioctl,
};

static struct miscdevice mt9p012_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "mt9p012",
	.fops  = &mt9p012_fops,
};

static int mt9p012_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	CDBG("mt9p012_probe called!\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CDBG("i2c_check_functionality failed\n");
		goto probe_failure;
	}

	mt9p012_ctrl->sensorw =
		kzalloc(sizeof(struct mt9p012_work_t), GFP_KERNEL);

	if (!mt9p012_ctrl->sensorw) {
		CDBG("kzalloc failed.\n");
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, mt9p012_ctrl->sensorw);
	mt9p012_init_client(client);
	mt9p012_ctrl->client = client;

	mdelay(50);

	/* Register a misc device */
	rc = misc_register(&mt9p012_device);
	if (rc) {
		CDBG("mt9p012_probe misc_register failed!\n");
		goto probe_failure;
	}

	CDBG("mt9p012_probe successed! rc = %d\n", rc);
	return 0;

probe_failure:
	CDBG("mt9p012_probe failed! rc = %d\n", rc);
	return rc;
}

static int __exit mt9p012_remove(struct i2c_client *client)
{
	struct mt9p012_work_t *sensorw = i2c_get_clientdata(client);
	free_irq(client->irq, sensorw);
	i2c_detach_client(client);
	mt9p012_ctrl->client = NULL;
	misc_deregister(&mt9p012_device);
	kfree(sensorw);
	return 0;
}

static const struct i2c_device_id mt9p012_id[] = {
	{ "mt9p012", 0},
	{ }
};

static struct i2c_driver mt9p012_driver = {
	.id_table = mt9p012_id,
	.probe  = mt9p012_probe,
	.remove = __exit_p(mt9p012_remove),
	.driver = {
		.name = "mt9p012",
	},
};

int32_t mt9p012_init(void *pdata)
{
	int32_t rc = 0;

	struct msm_camera_sensor_info *data =
		(struct msm_camera_sensor_info *)pdata;

	CDBG("mt9p012_init called!\n");

	mt9p012_ctrl = kzalloc(sizeof(struct mt9p012_ctrl_t), GFP_KERNEL);

	if (!mt9p012_ctrl) {
		CDBG("mt9p012_init failed!\n");
		rc = -ENOMEM;
		goto init_failure;
	}

	mt9p012_ctrl->fps_divider = 1 * 0x00000400;
	mt9p012_ctrl->pict_fps_divider = 1 * 0x00000400;
	mt9p012_ctrl->set_test = TEST_OFF;
	mt9p012_ctrl->prev_res = QTR_SIZE;
	mt9p012_ctrl->pict_res = FULL_SIZE;

	if (data) {
		mt9p012_ctrl->sensordata = data;
		CDBG("mt9p012_init calling i2c_add_driver\n");
		rc = i2c_add_driver(&mt9p012_driver);
		if (IS_ERR_VALUE(rc))
			kfree(mt9p012_ctrl);
	}

init_failure:
	return rc;
}

void mt9p012_exit(void)
{
	i2c_del_driver(&mt9p012_driver);
}

