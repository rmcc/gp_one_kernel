/* drivers/input/keyboard/synaptics_i2c_rmi.c
 *
 * Copyright (C) 2007 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*
#include <linux/module.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gasgauge_bridge.h>
#include <asm/io.h>
#ifdef CONFIG_ANDROID_POWER
#include <linux/android_power.h>
#endif
*/

struct i2c_client * g_i2c_client;

#define OW_BRIDGE_SLAVE_ADDRESS   0x18

#define CMD_DEVICE_RESET   0xF0
#define CMD_SET_READ_POINT 0xE1
#define CMD_OW_RESET       0xB4
#define CMD_OW_WRITE_BYTE  0xA5
#define CMD_OW_READ_BYTE   0x96

#define REGISTER_STATUS         0xF0
#define REGISTER_READ_DATA      0xE1
#define REGISTER_CONFIGURATION  0xC3

#include <asm/gpio.h>

int get_i2c_bus(struct i2c_adapter * adap);
int send_i2c_package(struct i2c_adapter * adap, struct i2c_msg *msgs, int num);
void release_i2c_bus(struct i2c_adapter * adap);

#if 0
#define DBGMSG(x) {printk x;}
#else
#define DBGMSG(x) {;}
#endif

static void ZeusDS2482_init(struct i2c_client * client)
{
	g_i2c_client = client;

	gpio_tlmm_config( GPIO_CFG( 23, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA ), GPIO_ENABLE );
	gpio_direction_output(23, 0);
	//printk(KERN_INFO "<ubh> ZeusDS2482_init 00. : %d\n", gpio_get_value(23));
}

/* read keyboard via i2c address + register offset, return # bytes read */
static int i2crdow(uint8_t * buf)
{
	struct i2c_msg msgs[] = {
		[0] = {
			.addr	= g_i2c_client->addr,
			.flags	= I2C_M_RD,
			.buf	= (void *)buf,
			.len	= 1
		}
	};

	return send_i2c_package(g_i2c_client->adapter, msgs, 1);
}

/* Write the specified data to the  control reg */
uint8_t g_srp[2] = {CMD_SET_READ_POINT};
uint8_t g_1wwb[2] = {CMD_OW_WRITE_BYTE};

struct i2c_msg g_msgs[] = {
	[0] = {	// cmd
		.addr	= OW_BRIDGE_SLAVE_ADDRESS,
		.flags	= 0,
		.buf	= (void *)&g_srp[1],
		.len	= 1
	},
	[1] = {	// srp
		.addr	= OW_BRIDGE_SLAVE_ADDRESS,
		.flags	= 0,
		.buf	= (void *)g_srp,
		.len	= 2
	},
	[2] = {	// 1wwb
		.addr	= OW_BRIDGE_SLAVE_ADDRESS,
		.flags	= 0,
		.buf	= (void *)g_1wwb,
		.len	= 2
	},
};

#define i2cwrcmd(x) (g_srp[1] = x, send_i2c_package(g_i2c_client->adapter, &(g_msgs[0]), 1))
#define i2cwrsrp(x) (g_srp[1] = x, send_i2c_package(g_i2c_client->adapter, &(g_msgs[1]), 1))
#define i2cwr1wwb(x) (g_1wwb[1] = x, send_i2c_package(g_i2c_client->adapter, &(g_msgs[2]), 1))

#define CHECK_NORMAL_BUSY_TIME	10

static int ZeusDS2482_CkBusy(char * ctitle)
{
	int iRet, i = 0;
	uint8_t state;
	do {
		if ((iRet = i2crdow(&state)) < 0) {
			printk(KERN_INFO "<ubh> %s : ZeusDS2482_CkBusy fail on no.%d read : %d - state(%x)\n", ctitle, i, iRet, state);
			return iRet;
		}
		DBGMSG((KERN_INFO "<ubh> ZeusDS2482_OWReset no.%d : state(%x)\n", i, state))
		if ((state & 0x01) == 0) {
			DBGMSG((KERN_INFO "<ubh> ZeusDS2482_OWReset done on i(%d) : state(%x)\n", i, state))
			return 0;
		}
		i++;
	} while (i < CHECK_NORMAL_BUSY_TIME);
	printk(KERN_INFO "<ubh> %s : ZeusDS2482_CkBusy %d time out : state(%x)\n", ctitle, i, state);
	return -1;
}

static int ZeusDS2482_Reset(void)
{
	int iRet;
	iRet = i2cwrcmd(CMD_DEVICE_RESET);
	if (iRet < 0) {
		printk(KERN_INFO "<ubh> ZeusDS2482_Reset fails 00. !!! : %d\n", iRet);
	}
	else {
		uint8_t state;
		int i;

		DBGMSG((KERN_INFO "<ubh> ZeusDS2482_Reset wait ...\n"))
		for (i = 0; ((i < 10) && (0 <= (iRet = i2crdow(&state))) && ((state | 0x08) != 0x18)); i++)
			DBGMSG((KERN_INFO "<ubh> ZeusDS2482_Reset wait : i(%d) - state(%d)\n", i, state))
		DBGMSG((KERN_INFO "<ubh> ZeusDS2482_Reset end : i(%d) - state(%d) - iRet(%d)\n", i, state, iRet))
	}

	return iRet;
}

static int ZeusDS2482_OWReset(void)
{
	int iRet, i;
	uint8_t state;

	for (i = 0; ((0 > (iRet = i2cwrcmd(CMD_OW_RESET))) && (i < 3)); i++)	//, ZeusDS2482_Reset())
		//printk(KERN_INFO "<ubh> ZeusDS2482_OWReset 00-%d : fails(%d) try again...\r\n", i, iRet);
		printk(KERN_INFO "<ZOWRS> 00-%d\r\n", i);

	if (iRet < 0) {
		printk(KERN_INFO "<ubh> ZeusDS2482_OWReset fails(%d) 00-%d. !!!\r\n", i, iRet);
	}
	else {
		// 1. check if 1-wire is busy 
		DBGMSG((KERN_INFO "<ubh> ZeusDS2482_OWReset 01 : check ow busy ...\n"))
		iRet = ZeusDS2482_CkBusy("ZeusDS2482_OWReset 01");

		// 2. checking for presence pulse detect
		//if (iRet < 0) return iRet;
		//for (i = 0; ((i < CHECK_NORMAL_BUSY_TIME) && ((iRet = i2crdow(&state)) >= 0) && ((state & 0x2) == 0)); i++)
		//	printk(KERN_INFO "<ubh> ZeusDS2482_OWReset 02-%d : state(%x)\n", i, state);
	}

	return iRet;
}

static int ZeusDS2482_OWReadByte(uint8_t * data)
{
	int iRet;
	uint8_t state;

	// 1. check if 1-wire is busy 
	if ((iRet = i2cwrsrp(REGISTER_STATUS)) < 0) {
		printk(KERN_INFO "<ubh> ZeusDS2482_OWReadByte fails 00. !!! (%d) - i2cwrsrp\n", iRet);
		return iRet;
	}

	DBGMSG((KERN_INFO "<ubh> ZeusDS2482_OWReadByte 01 : check ow busy ...\n"))
	if (iRet = ZeusDS2482_CkBusy("ZeusDS2482_OWReadByte 01")) {
		return iRet;
	}

	// 2. Read a byte from 1-wire
	if ((i2cwrcmd(CMD_OW_READ_BYTE)) < 0) {
		printk(KERN_INFO "<ubh> ZeusDS2482_OWReadByte fails 02. !!! (%d) - i2cwrcmd\n", iRet);
		return iRet;
	}

	DBGMSG((KERN_INFO "<ubh> ZeusDS2482_OWReadByte 02 : check ow busy ...\n"))
	if (iRet = ZeusDS2482_CkBusy("ZeusDS2482_OWReadByte 02")) {
		return iRet;
	}

	// 3. Read the byte data from DS2482 buffer
	if ((iRet = i2cwrsrp(REGISTER_READ_DATA)) < 0) {
		printk(KERN_INFO "<ubh> ZeusDS2482_OWReadByte fails 03. !!! (%d) - i2cwrsrp\n", iRet);
		return iRet;
	}

	iRet = i2crdow(data);

	return iRet;
}

static int ZeusDS2482_OWWriteByte(uint8_t data)
{
	int iRet;

	// 1. check if 1-wire is busy 
	if ((iRet = i2cwrsrp(REGISTER_STATUS)) < 0) {
		printk(KERN_INFO "<ubh> ZeusDS2482_OWWriteByte fails 00. !!! (%d) - i2cwrsrp\n", iRet);
		return iRet;
	}

	DBGMSG((KERN_INFO "<ubh> ZeusDS2482_OWWriteByte 01 : check ow busy ...\n"))
	if (iRet = ZeusDS2482_CkBusy("ZeusDS2482_OWWriteByte 01")) {
		return iRet;
	}

	// 2. ask DS2482 write one data byte to 1-wire slave
	if ((iRet = i2cwr1wwb(data)) < 0) {
		printk(KERN_INFO "<ubh> ZeusDS2482_OWWriteByte fails 02. !!! (%d) - i2cwr1wwb\n", iRet);
	}

	DBGMSG((KERN_INFO "<ubh> ZeusDS2482_OWWriteByte 03 : check ow busy ...\n"))
	return ZeusDS2482_CkBusy("ZeusDS2482_OWWriteByte 03");
}

#define DS2780CMD_SKIP_NET_ADDR  0xCC  /* battery smart ic cocommand*/
#define DS2780CMD_READ_DATA      0x69
/* FIH_ADQ, Kenny { */
#define DS2780CMD_WRITE_DATA      0x6C
/* } FIH_ADQ, Kenny */

int g_traceGasgauge = 0;
int getGasgaugeState()
{
	return g_traceGasgauge;
}
EXPORT_SYMBOL_GPL(getGasgaugeState);

static int BATReadMemory16(uint8_t reg, short * Data)
{
	int iRet, iStep;
	union {
		short s;
		struct {
			uint8_t low;
			uint8_t high;
		} u;
	} data;

	do {
g_traceGasgauge = 10;
		if ((iRet = ZeusDS2482_OWReset()) < 0) {iStep = 0; break;}
g_traceGasgauge++;

		if ((iRet = ZeusDS2482_OWWriteByte(DS2780CMD_SKIP_NET_ADDR)) < 0) {iStep = 1; break;}
g_traceGasgauge++;

		if ((iRet = ZeusDS2482_OWWriteByte(DS2780CMD_READ_DATA)) < 0) {iStep = 2; break;}
g_traceGasgauge++;
		if ((iRet = ZeusDS2482_OWWriteByte(reg)) < 0) {iStep = 3; break;}
g_traceGasgauge++;

		if ((iRet = ZeusDS2482_OWReadByte(&(data.u.high))) < 0) {iStep = 4; break;}
g_traceGasgauge++;
		if ((iRet = ZeusDS2482_OWReadByte(&(data.u.low))) < 0) {iStep = 5; break;}
g_traceGasgauge = 0;

		DBGMSG((KERN_INFO "<ubh> BATReadMemory16 : high(%d) low(%d) s(%d)\n", data.u.high, data.u.low, data.s))
		*Data = data.s;
		return iRet;
	} while (0);
g_traceGasgauge = 0;

	printk(KERN_INFO "<ubh> BATReadMemory16 fails in Step %d. !!! - %d\n", iStep, iRet);
	return iRet;
}

static int BATReadMemory8(uint8_t reg, uint8_t * Data)
{
	int iRet, iStep;
	uint8_t c;

	do {
g_traceGasgauge = 20;
		if ((iRet = ZeusDS2482_OWReset()) < 0) {iStep = 0; break;}
g_traceGasgauge++;

		if ((iRet = ZeusDS2482_OWWriteByte(DS2780CMD_SKIP_NET_ADDR)) < 0) {iStep = 1; break;}
g_traceGasgauge++;

		if ((iRet = ZeusDS2482_OWWriteByte(DS2780CMD_READ_DATA)) < 0) {iStep = 2; break;}
g_traceGasgauge++;
		if ((iRet = ZeusDS2482_OWWriteByte(reg)) < 0) {iStep = 3; break;}
g_traceGasgauge++;

		if ((iRet = ZeusDS2482_OWReadByte(&c)) < 0) {iStep = 4; break;}
g_traceGasgauge = 0;

		*Data = c;
		return iRet;
	} while (0);
g_traceGasgauge = 0;

	printk(KERN_INFO "<ubh> BATReadMemory8 fails in Step %d. !!! - %d\n", iStep, iRet);
	return iRet;
}

/* FIH_ADQ, Kenny { */
static int BATWriteMemory16(uint8_t reg, short Data)
{
	int iRet, iStep;
	union {
		short s;
		struct {
			uint8_t low;
			uint8_t high;
		} u;
	} data;
    
    data.s = Data;
	do {
g_traceGasgauge = 30;
		if ((iRet = ZeusDS2482_OWReset()) < 0) {iStep = 0; break;}
g_traceGasgauge++;

		if ((iRet = ZeusDS2482_OWWriteByte(DS2780CMD_SKIP_NET_ADDR)) < 0) {iStep = 1; break;}
g_traceGasgauge++;

		if ((iRet = ZeusDS2482_OWWriteByte(DS2780CMD_WRITE_DATA)) < 0) {iStep = 2; break;}
g_traceGasgauge++;
		if ((iRet = ZeusDS2482_OWWriteByte(reg)) < 0) {iStep = 3; break;}
g_traceGasgauge++;

		if ((iRet = ZeusDS2482_OWWriteByte(data.u.high)) < 0) {iStep = 4; break;}
g_traceGasgauge++;
		if ((iRet = ZeusDS2482_OWWriteByte(data.u.low)) < 0) {iStep = 5; break;}
g_traceGasgauge = 0;

		DBGMSG((KERN_INFO "<ubh> BATWriteMemory16 : high(%d) low(%d) s(%d)\n", data.u.high, data.u.low, data.s))
		
		return iRet;
	} while (0);
g_traceGasgauge = 0;

	printk(KERN_INFO "<ubh> BATWriteMemory16 fails in Step %d. !!! - %d\n", iStep, iRet);
	return iRet;
}

static int BATWriteMemory8(uint8_t reg, uint8_t  Data)
{
	int iRet, iStep;
	uint8_t c;
    
    c = Data;
	do {
g_traceGasgauge = 40;
		if ((iRet = ZeusDS2482_OWReset()) < 0) {iStep = 0; break;}
g_traceGasgauge++;

		if ((iRet = ZeusDS2482_OWWriteByte(DS2780CMD_SKIP_NET_ADDR)) < 0) {iStep = 1; break;}
g_traceGasgauge++;

		if ((iRet = ZeusDS2482_OWWriteByte(DS2780CMD_WRITE_DATA)) < 0) {iStep = 2; break;}
g_traceGasgauge++;
		if ((iRet = ZeusDS2482_OWWriteByte(reg)) < 0) {iStep = 3; break;}
g_traceGasgauge++;

		if ((iRet = ZeusDS2482_OWWriteByte(c)) < 0) {iStep = 4; break;}
g_traceGasgauge = 0;

		return iRet;
	} while (0);
g_traceGasgauge = 0;

	printk(KERN_INFO "<ubh> BATWriteMemory8 fails in Step %d. !!! - %d\n", iStep, iRet);
	return iRet;
}
/* } FIH_ADQ, Kenny */

struct mutex g_ow2428_suspend_lock;

static int OpenBAT(void)
{
	mutex_lock(&g_ow2428_suspend_lock);
	gpio_set_value(23, 1);
	return get_i2c_bus(g_i2c_client->adapter);
}

static void CloseBAT(void)
{
	release_i2c_bus(g_i2c_client->adapter);
	gpio_set_value(23, 0);
	mutex_unlock(&g_ow2428_suspend_lock);
}

#define DS2780_STATUS				0x01    // STATUS
#define DS2780_RAAC_MSB			0x02    // Remaining Active Absolute Capacity MSB
#define DS2780_RAAC_LSB			0x03    // Remaining Active Absolute Capacity LSB
#define DS2780_RARC					0x06		// Remaining Active Relative Capacity
#define DS2780_TRH          0x0A    // Temperature register MSB
#define DS2780_TRL          0x0B    // Temperature register LSB
#define DS2780_VRH          0x0C    // Voltage register MSB
#define DS2780_VRL          0x0D    // Voltage register LSB
#define DS2780_CRH          0x0E    // Current register MSB
#define DS2780_CRL          0x0F    // Current register LSB
/* FIH_ADQ, Kenny { */
#define DS2780_ACRH         0x10    // Accumulated Current Register MSB
#define DS2780_ACRL         0x11    // Accumulated Current Register LSB
#define DS2780_AS           0x14    // Age Scalar
#define DS2780_FULLH        0x16    // Full Capacity MSB
#define DS2780_FULLL        0x17    // Full Capacity LSB
#define DS2780_FULL40H      0x6A    // Full40 MSB
#define DS2780_FULL40L      0x6B    // Full40 LSB
/* } FIH_ADQ, Kenny */

