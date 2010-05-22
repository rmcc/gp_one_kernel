/*
 *     max8831.c - max8831 LED EXPANDER for BACKLIGHT and LEDs CONTROLLER
 *
 *     Copyright (C) 2009 Audi PC Huang <audipchuang@fihtdc.com>
 *     Copyright (C) 2008 FIH CO., Inc.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; version 2 of the License.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/i2c/max7302.h>
#include <mach/max8831.h>
#include <mach/gpio.h>
#include <mach/msm_iomap.h>
#include <mach/msm_smd.h>

struct max8831_driver_data {
	struct mutex max8831_lock;
	struct i2c_client *max8831_i2c_client;
	unsigned int HWID;
};

static struct max8831_driver_data max8831_drvdata;

/*************I2C functions*******************/
static int max8831_read( struct i2c_client *client, u8 *rxdata, int length )
{
	struct i2c_msg msgs[] =
	{
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &rxdata[0],
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = &rxdata[1],
		},
	};

	int ret;
	if ( ( ret = i2c_transfer( client->adapter, msgs, 2 ) ) < 0 )
	{
		dev_err(&client->dev, "i2c rx failed %d\n", ret );
		return -EIO;
	}

	return 0;
}

static int max8831_write( struct i2c_client *client, u8 *txdata, int length )
{
	struct i2c_msg msg[] =
	{
		{
			.addr = client->addr,
			.flags = 0,
			.len = length,
			.buf = txdata,
		},
	};

	if ( i2c_transfer( client->adapter, msg, 1 ) < 0 )
	{
		dev_err(&client->dev, "i2c tx failed\n" );
		return -EIO;
	}

	return 0;
}

static bool max8831_exist(void)
{
	struct device *max8831_dev = &max8831_drvdata.max8831_i2c_client->dev;
	u8 buf[3] = {0x00, 0x00, 0x00};
	int rc;
	
	buf[0] = MAX8831_CHIP_ID1;
	rc = max8831_read(max8831_drvdata.max8831_i2c_client, buf, 2);
	if (rc < 0) {
		dev_err(max8831_dev, "%s: read CHIP ID failed!!\n", __func__);
		return false;
	} /*else if ((MAX8831_CHIP_ID1_VALUE != buf[1]) || 
			(MAX8831_CHIP_ID2_VALUE != buf[2])) {
		dev_err(max8831_dev, "%s: Incorrect CHIP ID <1. 0x%02x> <2. 0x%02x>!!\n", __func__, buf[1], buf[2]);
		return false;
	} */
	
	return true;
}

static bool max7302_exist(void)
{
	//u8 buf[3] = {0x00, 0x00, 0x00};
	//int rc;
	
	return true;
}

static int max8831_set_brightness(int port, int level)
{
	struct device *max8831_dev = &max8831_drvdata.max8831_i2c_client->dev;
	u8 buf[2] = {0x00, 0x00};
	int rc;
	
	buf[0] = port;
	buf[1] = level;
	
	rc = max8831_write(max8831_drvdata.max8831_i2c_client, buf, 2);
	if (rc < 0) {
		dev_err(max8831_dev, "%s: write ON/OFF CNTL REG failed!!\n", __func__);
		return -1;
	}
	
	return 0;
}

static int max8831_set_backlight(int port, int level)
{
	struct device *max8831_dev = &max8831_drvdata.max8831_i2c_client->dev;
	u8 rxbuf[2] = {0x00, 0x00};
	u8 wxbuf[2] = {0x00, 0x00};
	int rc;
	
	// Get current backlight status.
	rxbuf[0] = MAX8831_ON_OFF_CNTL;
	rc = max8831_read(max8831_drvdata.max8831_i2c_client, rxbuf, 1);
	if (rc < 0) {
		dev_err(max8831_dev, "%s: read ON/OFF CNTL REG failed!!\n", __func__);
		return -1;
	} else {
		dev_dbg(max8831_dev, "%s: read ON/OFF CNTL REG  0x%02x 0x%02x !!\n", __func__, rxbuf[0], rxbuf[1]);	
	}
	
	//0x00 < level < 0x7B
	if (level < MAX8831_ILED_CNTL_MIN_VAL) {
		level = MAX8831_ILED_CNTL_MIN_VAL;
	} else if (level > MAX8831_ILED_CNTL_MAX_VAL) {
		level = MAX8831_ILED_CNTL_MAX_VAL;	
	}
	
	switch (port) {
	case ZUA_KPD_BL_CNTL:
		if (level == 0) { //Turn off KPD Backlight
			if (rxbuf[1] & ZUA_KPD_BL_MASK) { //KPD Backlight is on.
				wxbuf[0] = MAX8831_ON_OFF_CNTL;
				wxbuf[1] = rxbuf[1] & ~ZUA_KPD_BL_ON; //Turn off KPD Backlight
				rc = max8831_write(max8831_drvdata.max8831_i2c_client, wxbuf, 2);
				if (rc < 0) {
					dev_err(max8831_dev, "%s: write ON/OFF CNTL REG failed!!\n", __func__);
					return -1;
				} else {
					dev_dbg(max8831_dev, "%s: KPD BL OFF!!rxbuf 0x%02x wxbuf 0x%02x\n", __func__, rxbuf[1], wxbuf[1]);
					return 0;
				}
			} else { //KPD Backlight has been turned off.
				dev_dbg(max8831_dev, "%s: KPD BL was turned OFF!!\n", __func__);
			}
		} else {
			//Set KPD Brightness
			max8831_set_brightness(ZUA_KPD_BL_CNTL, level);
			dev_dbg(max8831_dev, "%s: Set KPD Brightness Finished!!\n", __func__);
			
			if (!(rxbuf[1] & ZUA_KPD_BL_MASK)) { //KPD Backlight is off.
				wxbuf[0] = MAX8831_ON_OFF_CNTL;
				wxbuf[1] = rxbuf[1] | ZUA_KPD_BL_ON; //Turn on KPD Backlight.
				rc = max8831_write(max8831_drvdata.max8831_i2c_client, wxbuf, 2);
				if (rc < 0) {
					dev_err(max8831_dev, "%s: write ON/OFF CNTL REG failed!!\n", __func__);
					return -1;
				} else {
					dev_dbg(max8831_dev, "%s: KPD BL ON!!rxbuf 0x%02x wxbuf 0x%02x\n", __func__, rxbuf[1], wxbuf[1]);				
				}
			}
		}
		break;
	case ZUA_LCD_BL_CNTL:
		if (level == 0) {
			if (rxbuf[1] & ZUA_LCD_BL_MASK) { //LCD Backlight is on.
				wxbuf[0] = MAX8831_ON_OFF_CNTL;
				wxbuf[1] = rxbuf[1] & ~ZUA_LCD_BL_ON; //Turn off LCD Backlight
				rc = max8831_write(max8831_drvdata.max8831_i2c_client, wxbuf, 2);
				if (rc < 0) {
					dev_err(max8831_dev, "%s: write ON/OFF CNTL REG failed!!\n", __func__);
					return -1;
				} else {
					dev_dbg(max8831_dev, "%s: LCD BL OFF!! rxbuf 0x%02x wxbuf 0x%02x\n", __func__, rxbuf[1], wxbuf[1]);
					return 0;
				}
			} else { //LCD Backlight is off.
				dev_dbg(max8831_dev, "%s: LCD BL was turned OFF!!\n", __func__);
			}
		} else {
			//Set LCD Brightness
			max8831_set_brightness(ZUA_LCD_BL_CNTL, level);	
			dev_dbg(max8831_dev, "%s: Set LCD Brightness Finished!!\n", __func__);
			
			if (!(rxbuf[1] & ZUA_LCD_BL_MASK)) { //LCD Backlight is off.
				wxbuf[0] = MAX8831_ON_OFF_CNTL;
				wxbuf[1] = rxbuf[1] | ZUA_LCD_BL_ON; //Turn on LCD Backlight.
				rc = max8831_write(max8831_drvdata.max8831_i2c_client, wxbuf, 2);
				if (rc < 0) {
					dev_err(max8831_dev, "%s: write ON/OFF CNTL REG failed!!\n", __func__);
					return -1;
				} else {
					dev_dbg(max8831_dev, "%s: LCD BL ON!!rxbuf 0x%02x wxbuf 0x%02x\n", __func__, rxbuf[1], wxbuf[1]);				
				}
			}
		}
		break;
	default:
		dev_err(max8831_dev, "%s: Invalid Port Number!!\n", __func__);
	}
	
	return 0;
}

int max7302_port_set_level(int port, int level)
{
	struct device *max8831_dev = &max8831_drvdata.max8831_i2c_client->dev;
 	int ret = 0;
	u8 wxbuf[2];
	
	mutex_lock(&max8831_drvdata.max8831_lock);
		if (level < 1)
			level = PWM_STATIC_LOW;
		else if (level > 31)
			level = PWM_STATIC_HIGH;
		else
			level = ((31 - level) | 0x41);

		//set lcd bl port on
		wxbuf[0] = port;
		wxbuf[1] = level;
		ret = max8831_write(max8831_drvdata.max8831_i2c_client, wxbuf, 2);
	mutex_unlock(&max8831_drvdata.max8831_lock);
	if (ret) {
	  	dev_err(max8831_dev, "%s: set i2c tx on failed\n", __func__);
		return ret;
	} else {
	  	dev_dbg(max8831_dev, "%s: set level finished: port=0x%02x level=%d\n", __func__, port, level);		
	}

	return ret;
}

int kpd_bl_set_intensity(int level) 
{
	int rc = -1;
	
	if (max8831_drvdata.HWID >= CMCS_HW_VER_EVT2) {
		if (!max8831_exist()) {
			return rc;
		}
		
		mutex_lock(&max8831_drvdata.max8831_lock);
			level = level * ZUA_KPD_ILED_CNTL_MAX_VAL / 255;
			rc = max8831_set_backlight(ZUA_KPD_BL_CNTL, level);
		mutex_unlock(&max8831_drvdata.max8831_lock);
	} else {
		if (!max7302_exist()) {
			return rc;
		}
		
		level = level * 32 / 255;
		rc = max7302_port_set_level(KPD_BL_PORT, level);
	}
	
	return rc;
}
EXPORT_SYMBOL(kpd_bl_set_intensity);

int lcd_bl_set_intensity(int level)
{
	int rc = -1;

	if (max8831_drvdata.HWID >= CMCS_HW_VER_EVT2) {
		if (!max8831_exist()) {
			return rc;
		}
		
		mutex_lock(&max8831_drvdata.max8831_lock);
			level = level * ZUA_LCD_ILED_CNTL_MAX_VAL / 255;
			rc = max8831_set_backlight(ZUA_LCD_BL_CNTL, level);
		mutex_unlock(&max8831_drvdata.max8831_lock);
	} else {
		if (!max7302_exist()) {
			return rc;
		}

		level = level * 32 / 255;
		rc = max7302_port_set_level(LCD_BL_PORT, level);
	}
	
	return rc;
}
EXPORT_SYMBOL(lcd_bl_set_intensity);

///+++MAX7302 ONLY
static int max7302_set_conf26(int blk_prd, int rst_timer, int rst_por)
{
	struct device *max8831_dev = &max8831_drvdata.max8831_i2c_client->dev;
  	int ret = 0;
	u8 wxbuf[2] = {0x00, 0x00};

	mutex_lock(&max8831_drvdata.max8831_lock);
		if (blk_prd < 0)
			blk_prd = 0;
		else if (blk_prd > 7)
			blk_prd = 7;

		wxbuf[0] = CONF_26_REG;
		wxbuf[1] = CONF_26_DATA(blk_prd, rst_timer, rst_por);
		ret = max8831_write(max8831_drvdata.max8831_i2c_client, wxbuf, 2);
	mutex_unlock(&max8831_drvdata.max8831_lock);	
	if (ret) {
	  	dev_err(max8831_dev, "%s: set i2c tx on failed\n", __func__);
		return ret;
	}

	return ret;
}

static int max7302_set_conf27(int bus_t, int p3_oscout, int p2_oscin, int p1_int, int input_tran)
{
	struct device *max8831_dev = &max8831_drvdata.max8831_i2c_client->dev;
  	int ret = 0;
	u8 wxbuf[2] = {0x00, 0x00};

	mutex_lock(&max8831_drvdata.max8831_lock);
		wxbuf[0] = CONF_27_REG;
		wxbuf[1] = CONF_27_DATA(bus_t, p3_oscout, p2_oscin, p1_int, input_tran);
		ret = max8831_write(max8831_drvdata.max8831_i2c_client, wxbuf, 2);
	mutex_unlock(&max8831_drvdata.max8831_lock);
	if (ret) {
	  	dev_err(max8831_dev, "%s: set i2c tx on failed\n", __func__);
		return ret;
	}

	return ret;
}

int led_rgb_set_intensity(int R, int G, int B)
{
  	int ret1 = 0, ret2 = 0, ret3 = 0;
	
	if (!max7302_exist()) {
		return -1;
	}
	
	if (R >= 0) {
	  	ret1 = max7302_port_set_level(LED1_R_PORT, R);
	}

	if (G >= 0) {
	  	ret2 = max7302_port_set_level(LED1_G_PORT, G);
	}

	if (B >= 0) {
	  	ret3 = max7302_port_set_level(LED1_B_PORT, B);
	}

	if ((ret1 < 0) || (ret2 < 0) || (ret3 < 0))
		return -1;

  	return 0;
}
EXPORT_SYMBOL(led_rgb_set_intensity);

int led_234_set_intensity(int L2, int L3, int L4)
{
  	int ret1 = 0, ret2 = 0, ret3 = 0;
	
	if (!max7302_exist()) {
		return -1;
	}

	if (L2 >= 0) {
	  	ret1 = max7302_port_set_level(LED2_PORT, L2);
	}

	if (L3 >= 0) {
	  	ret2 = max7302_port_set_level(LED3_PORT, L3);
	}

	if (L4 >= 0) {
	  	ret3 = max7302_port_set_level(LED4_PORT, L4);
	}

	if ((ret1 < 0) || (ret2 < 0) || (ret3 < 0))
		return -1;

  	return 0;
}
EXPORT_SYMBOL(led_234_set_intensity);

int led_set_blk_t(unsigned int blk_prd)
{
  	int ret = 0;
	
	if(max7302_exist())
	  	return -1;

	if(blk_prd > 8)
		blk_prd = 7;
	
	ret = max7302_set_conf26(blk_prd, 1, 0);
	if(ret < 0)
	  	return -1;

	return 0;
}
EXPORT_SYMBOL(led_set_blk_t);
///---MAX7302 ONLY

static int max8831_reset(void)
{
	struct device *max8831_dev = &max8831_drvdata.max8831_i2c_client->dev;
	u8 buf[2] = {0x00, 0x00};
	int rc = -1;
	
	if (!max8831_exist()) {
		return rc;
	}

	/*buf[0] = MAX8831_ON_OFF_CNTL;
	buf[1] = 0x00;

	mutex_lock(&max8831_drvdata.max8831_lock);
		rc = max8831_write(max8831_drvdata.max8831_i2c_client, buf, 2);
	mutex_unlock(&max8831_drvdata.max8831_lock);

	if (rc < 0) {
		printk(KERN_ERR "%s: write ON/OFF CNTL REG failed!!\n", __func__);
		return -1;
	}*/
	
	buf[0] = ZUA_LCD_BL_RAMP_CNTL;
	buf[1] = MAX8831_RAMP_RATE_64MS | (MAX8831_RAMP_RATE_64MS << MAX8831_RAMP_DN_SHIFT);
	buf[2] = MAX8831_RAMP_RATE_2048MS | (MAX8831_RAMP_RATE_2048MS << MAX8831_RAMP_DN_SHIFT); //RAMP UP 4096 ms, RAMP DN 4096 ms

	mutex_lock(&max8831_drvdata.max8831_lock);
		rc = max8831_write(max8831_drvdata.max8831_i2c_client, buf, 3);
	mutex_unlock(&max8831_drvdata.max8831_lock);
	if (rc < 0) {
		dev_err(max8831_dev, "%s: write ON/OFF CNTL REG failed!!\n", __func__);
		return -1;
	} else {
		dev_dbg(max8831_dev, "%s: 0x%02x!!\n", __func__, buf[1]);
	}
	
	return 0;
}

static ssize_t max8831_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i, bytes_read = 0;
	u8 loc_buf[4096];
	u8 rxbuf[7];
	
	rxbuf[0] = MAX8831_ON_OFF_CNTL;
	max8831_read(max8831_drvdata.max8831_i2c_client, rxbuf, 1);
	bytes_read += sprintf(&loc_buf[bytes_read], "ON/OFF_CNTL: 0x%02x\n", rxbuf[1]);	
	
	rxbuf[0] = MAX8831_LED1_RAMP_CNTL;
	max8831_read(max8831_drvdata.max8831_i2c_client, rxbuf, 5);	
	for (i = 1; i < 6; i++) {
		bytes_read += sprintf(&loc_buf[bytes_read], "LED%d_RAMP_CNTL: 0x%02x\n", i, rxbuf[i]);
	}
	
	rxbuf[0] = MAX8831_ILED1_CNTL;
	max8831_read(max8831_drvdata.max8831_i2c_client, rxbuf, 5);	
	for (i = 1; i < 6; i++) {
		bytes_read += sprintf(&loc_buf[bytes_read], "ILED%d_CNTL: 0x%02x\n", i, rxbuf[i]);
	}
	
	rxbuf[0] = MAX8831_LED3_BLINK_CNTL;
	max8831_read(max8831_drvdata.max8831_i2c_client, rxbuf, 3);	
	for (i = 1; i < 4; i++) {
		bytes_read += sprintf(&loc_buf[bytes_read], "LED%d_BLINK_CNTL: 0x%02x\n", i + 2, rxbuf[i]);
	}

	rxbuf[0] = MAX8831_BOOST_CNTL;
	max8831_read(max8831_drvdata.max8831_i2c_client, rxbuf, 1);
	bytes_read += sprintf(&loc_buf[bytes_read], "BOOST_CNTL: 0x%02x\n", rxbuf[1]);	
	
	rxbuf[0] = MAX8831_STAT1;
	max8831_read(max8831_drvdata.max8831_i2c_client, rxbuf, 2);	
	for (i = 1; i < 3; i++) {
		bytes_read += sprintf(&loc_buf[bytes_read], "STAT%d: 0x%02x\n", i, rxbuf[i]);
	}
	
	rxbuf[0] = MAX8831_CHIP_ID1;
	max8831_read(max8831_drvdata.max8831_i2c_client, rxbuf, 2);	
	for (i = 1; i < 3; i++) {
		bytes_read += sprintf(&loc_buf[bytes_read], "CHIP_ID%d: 0x%02x\n", i, rxbuf[i]);
	}

	bytes_read = sprintf(buf, "%s", loc_buf);

	return bytes_read;
}

static ssize_t max8831_debug_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned cmd_number;
	
	sscanf(buf, "%3d\n", &cmd_number);
	
	dev_dbg(dev, "%s: COMMAND: %d\n", __func__, cmd_number);

	return count;
}

DEVICE_ATTR(max8831_debug, 0644, max8831_debug_show, max8831_debug_store);

#ifdef CONFIG_PM
static int max8831_suspend(struct i2c_client *client, pm_message_t mesg)
{
	//int ret = 0;
	/*struct max7302_i2c_data value;
	
	mutex_lock(&g_mutex);	
	value.reg	= 0x00;
	value.data	= 0x00; //Turn Off: LED1 and LED2
	
	ret = i2c_tx((u8*)&value, 2);
	if (ret != 0) {
		printk(KERN_INFO "Turn off keypad and lcd backlights failed!!\n");
	}*/

	return 0;
}

static int max8831_resume(struct i2c_client *client)
{
	int ret;

	if (max8831_drvdata.HWID >= CMCS_HW_VER_EVT2) {

	} else {//max7302
		//set cofig 26 reg
		dev_dbg(&client->dev, "%s: max7302 resume settings...\n", __func__);		
		
		ret = max7302_set_conf26(0, 1, 0);
	 	if (ret) {
		  	dev_err(&client->dev, "%s: set config 26 register failed\n", __func__);
		}

		//set config 27 reg
		ret = max7302_set_conf27(1, 1, 1, 1, 0);
		if (ret) {
		  	dev_err(&client->dev, "%s: set config 27 register failed\n", __func__);
		}
	}

	return 0;
}
#else
# define max8831_suspend NULL
# define max8831_resume  NULL
#endif

/******************Driver Functions*******************/
static int __devinit max8831_probe(struct i2c_client *client,
				    const struct i2c_device_id *id)
{
  	int ret = 0;

	i2c_set_clientdata(client, &max8831_drvdata);
	max8831_drvdata.max8831_i2c_client	= client;
	max8831_drvdata.HWID			= FIH_READ_HWID_FROM_SMEM();
	mutex_init(&max8831_drvdata.max8831_lock);
	
	dev_dbg(&client->dev, "%s: HWID = %d\n", __func__, max8831_drvdata.HWID);
	
	if (max8831_drvdata.HWID >= CMCS_HW_VER_EVT2) {
		ret = max8831_reset();
		if (ret < 0) {
			dev_err(&client->dev, "%s failed!! <%d>", __func__, ret);	
		}
		
		ret = device_create_file(&client->dev, &dev_attr_max8831_debug);
		if (ret < 0) {
			dev_err(&client->dev, "%s: Create attribute \"max8831_debug\" failed!! <%d>\n", __func__, ret);
			mutex_destroy(&max8831_drvdata.max8831_lock);

			return ret; 
		}
	} else {//max7302
		//set cofig 26 reg
		ret = max7302_set_conf26(0, 1, 0);
	 	if(ret)
		{
		  	dev_err(&client->dev, "set config 26 register failed\n");
			return ret;
		}

		//set config 27 reg
		ret = max7302_set_conf27(1, 1, 1, 1, 0);
		if(ret)
		{
		  	dev_err(&client->dev, "set config 27 register failed\n");
			return ret;
		}
	}
	
	return ret;
}

static int __devexit max8831_remove(struct i2c_client *client)
{
	int ret = 0;
	
	mutex_destroy(&max8831_drvdata.max8831_lock);
	device_remove_file(&client->dev, &dev_attr_max8831_debug);

	return ret;
}

static const struct i2c_device_id max8831_idtable[] = {
       { "max8831", 0 },
       { }
};

static struct i2c_driver max8831_driver = {
	.driver = {
		.name	= "max8831",
	},
	.probe		= max8831_probe,
	.remove		= __devexit_p(max8831_remove),
	.suspend  	= max8831_suspend,
	.resume   	= max8831_resume,
	.id_table	= max8831_idtable,
};

static int __init max8831_init(void)
{
	int ret = 0;

	printk( KERN_INFO "Driver init: %s\n", __func__ );

	// i2c
	ret = i2c_add_driver(&max8831_driver);
	if (ret) {
		printk(KERN_ERR "%s: Driver registration failed, module not inserted.\n", __func__);
		goto driver_del;
	}

	//all successfully.
	return ret;

driver_del:
	i2c_del_driver(&max8831_driver);
	
	return -1;
}

static void __exit max8831_exit(void)
{
	printk( KERN_INFO "Driver exit: %s\n", __func__ );
	i2c_del_driver(&max8831_driver);
}

module_init(max8831_init);
module_exit(max8831_exit);

MODULE_AUTHOR( "Audi PC Huang <audipchuang@fihtdc.com>" );
MODULE_DESCRIPTION( "MAX8831 driver" );
MODULE_LICENSE( "GPL" );
