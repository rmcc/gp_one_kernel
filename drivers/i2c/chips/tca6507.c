/*
 *     tca6507.c - TCA6507 LED EXPANDER for BACKLIGHT and LEDs CONTROLLER
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
#include <mach/tca6507.h>
#include <mach/gpio.h>
#include <mach/msm_iomap.h>
#include <mach/msm_smd.h>
#include <linux/wakelock.h>

struct tca6507_driver_data {
	struct mutex tca6507_lock;
	struct i2c_client *tca6507_i2c_client;
	unsigned int reset_pin;
	unsigned int HWID;
	u16 led_state[3];
	u16 prev_led_state[3];
	bool is_fade_on[3];
	bool is_blinking;
	bool is_attention;
	bool is_charger_connected;
	struct wake_lock charger_state_suspend_wake_lock;
};

/// +++ FIH_ADQ +++ , MichaelKao 2009.06.08
///add for low battery LED blinking in suspend mode 
extern void Battery_power_supply_change(void);
/// --- FIH_ADQ ---

static struct tca6507_driver_data tca6507_drvdata;
static int gCharger_state = CHARGER_STATE_UNKNOWN;

/*************I2C functions*******************/
static int tca6507_read( struct i2c_client *client, u8 *rxdata, int length )
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
	
	ret = i2c_transfer( client->adapter, msgs, 2 );
	if ( 0 > ret ) {
		printk( KERN_INFO "i2c rx failed %d\n", ret );
		return -EIO;
	}

	return 0;
}

static int tca6507_write( struct i2c_client *client, u8 *txdata, int length )
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
	
	int ret;
	
	ret = i2c_transfer( client->adapter, msg, 1 );
	if ( 0 > ret ) {
		printk( KERN_INFO "i2c tx failed %d\n", ret );
		return -EIO;
	}

	return 0;
}

/*****************gpio function****************/
static int gpio_init_setting(int value)
{
	struct device *tca6507_dev = &tca6507_drvdata.tca6507_i2c_client->dev;
  	int ret = 0;
  	
	gpio_tlmm_config(GPIO_CFG(tca6507_drvdata.reset_pin, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
	
	//request gpio
	ret = gpio_request(tca6507_drvdata.reset_pin, "tca6507_reset_pin");
	if(ret) {
	  	dev_err(tca6507_dev ,"%s: GPIO-%d request failed\n", __func__, tca6507_drvdata.reset_pin);
		return ret;
	}
	
	ret = gpio_direction_output(tca6507_drvdata.reset_pin, value);
	if(ret) {
	  	dev_err(tca6507_dev, "%s: GPIO-%d gpio_direction_output failed\n", __func__, tca6507_drvdata.reset_pin);
		return ret;
	}

	return 0;
}

static void gpio_release(void)
{
	gpio_free( tca6507_drvdata.reset_pin );
}

void tca6507_led_turn_on(int led_selection, uint8_t *cmd)
{		
	switch (led_selection) {
	case TCA6507_LED_LEFT:
		cmd[1] &= ~0x40;
		cmd[2] |= 0x40;
		cmd[3] &= ~0x40;
		
		break;
	
	case TCA6507_LED_CENTER:
		if (tca6507_drvdata.HWID < CMCS_HW_VER_DVT1) {
			cmd[1] &= ~(0x08|0x10|0x20);
			cmd[2] |= (0x08|0x10|0x20);
			cmd[3] &= ~(0x08|0x10|0x20);
		} else {
			cmd[1] &= ~0x08;
			cmd[2] |= 0x08;
			cmd[3] &= ~0x08;
		}
		
		break;
	
	case TCA6507_LED_RIGHT:
		cmd[1] &= ~0x04;
		cmd[2] |= 0x04;
		cmd[3] &= ~0x04;
	}
}

void tca6507_led_turn_off(int led_selection, uint8_t *cmd)
{		
	switch (led_selection) {
	case TCA6507_LED_LEFT:
		cmd[1] &= ~0x40;
		cmd[2] &= ~0x40;
		cmd[3] &= ~0x40;
		
		break;
	
	case TCA6507_LED_CENTER:
		if (tca6507_drvdata.HWID < CMCS_HW_VER_DVT1) {
			cmd[1] &= ~(0x08|0x10|0x20);
			cmd[2] &= ~(0x08|0x10|0x20);
			cmd[3] &= ~(0x08|0x10|0x20);
		} else {
			cmd[1] &= ~(0x08);
			cmd[2] &= ~(0x08);
			cmd[3] &= ~(0x08);		
		}
		
		break;
	
	case TCA6507_LED_RIGHT:
		cmd[1] &= ~0x04;
		cmd[2] &= ~0x04;
		cmd[3] &= ~0x04;
	}
}

void tca6507_led_blink(int led_selection, uint8_t *cmd)
{
	switch (led_selection) {
	case TCA6507_LED_LEFT:
		cmd[1] |= 0x40;
		cmd[2] |= 0x40;
		cmd[3] |= 0x40;

		break;
	
	case TCA6507_LED_CENTER:
		if (tca6507_drvdata.HWID < CMCS_HW_VER_DVT1) {
			cmd[1] |= (0x08|0x10|0x20);
			cmd[2] |= (0x08|0x10|0x20);
			cmd[3] |= (0x08|0x10|0x20);
		} else {
            		cmd[1] |= 0x08;
            		cmd[2] |= 0x08;
            		cmd[3] |= 0x08;
		}
		
		break;
	
	case TCA6507_LED_RIGHT:
		cmd[1] |= 0x04;
		cmd[2] |= 0x04;
		cmd[3] |= 0x04;
	}
}

void tca6507_led_fade_on(int led_selection, uint8_t *cmd)
{
	switch (led_selection) {
	case TCA6507_LED_LEFT:
		cmd[1] &= ~0x40;
		cmd[2] |= 0x40;
		cmd[3] |= 0x40;

		break;
	
	case TCA6507_LED_CENTER:
		if (tca6507_drvdata.HWID < CMCS_HW_VER_DVT1) {
			cmd[1] &= ~(0x08|0x10|0x20);
			cmd[2] |= (0x08|0x10|0x20);
			cmd[3] |= (0x08|0x10|0x20);
		} else {
            		cmd[1] &= ~0x08;
            		cmd[2] |= 0x08;
            		cmd[3] |= 0x08;
		}
		
		break;
	
	case TCA6507_LED_RIGHT:
		cmd[1] &= ~0x04;
		cmd[2] |= 0x04;
		cmd[3] |= 0x04;
	}

	cmd[11] = 0x88;
}

void tca6507_led_fade_off(int led_selection, uint8_t *cmd)
{
	switch (led_selection) {
	case TCA6507_LED_LEFT:
		cmd[1] &= ~0x40;
		cmd[2] |= 0x40;
		cmd[3] |= 0x40;

		break;
	
	case TCA6507_LED_CENTER:
		if (tca6507_drvdata.HWID < CMCS_HW_VER_DVT1) {
			cmd[1] &= ~(0x08|0x10|0x20);
			cmd[2] |= (0x08|0x10|0x20);
			cmd[3] |= (0x08|0x10|0x20);
		} else {
            		cmd[1] &= ~0x08;
            		cmd[2] |= 0x08;
            		cmd[3] |= 0x08;
		}
		
		break;
	
	case TCA6507_LED_RIGHT:
		cmd[1] &= ~0x04;
		cmd[2] |= 0x04;
		cmd[3] |= 0x04;
	}

	cmd[11] = 0xAA;
}

int tca6507_get_state(int led_selection)
{
	struct device *tca6507_dev = &tca6507_drvdata.tca6507_i2c_client->dev;
	int i;
	
	for (i = CHARGER_STATE_FULL; i > TCA6507_ABNORMAL_STATE; i--) {
		if (tca6507_drvdata.led_state[led_selection] & (1 << i)) {
			dev_dbg(tca6507_dev, "%s: LED<%d> in <%d> State!!\n", __func__, led_selection, i);
			return i;
		}
	}
	
	return i;
}

static int tca6507_set_led(void)
{
	struct device *tca6507_dev = &tca6507_drvdata.tca6507_i2c_client->dev;
	u8 cmd[12] = {TCA6507_INITIALIZATION_REGISTER, 0x00, 0x02, 0x00, 0x77, 0x44, 0x77, 0xBB, 0xBB, 0xFF, 0xFF, 0x88};
	u8 cmd_fade_off[12] = {TCA6507_INITIALIZATION_REGISTER, 0x00, 0x02, 0x00, 0x77, 0x44, 0x77, 0xBB, 0xBB, 0xFF, 0xFF, 0xAA};
	int ret = -1;
	int i, state[3];
	bool is_blinking = false;
	bool is_fade_off = false;
	bool is_fade_on = false;

	dev_dbg(tca6507_dev, "%s\n", __func__);

	for (i = 0; i < 3; i++) {
		state[i] = tca6507_get_state(i);

		switch (state[i]) {		
		case TCA6507_LED_OFF:
			if (TCA6507_LED_PREV_STATE_ON == tca6507_drvdata.prev_led_state[i]) {
				is_fade_off = true;
			}

			break;
		case TCA6507_LED_BLINK:
		case CHARGER_STATE_LOW_POWER:
			is_blinking = true;
			cmd[10] = 0x4F;

			break;
		case TCA6507_LED_ATTENTION:
		case CHARGER_STATE_DISCHARGING:
		case TCA6507_LED_ON:
		case CHARGER_STATE_UNKNOWN:
		case CHARGER_STATE_CHARGING:
		case CHARGER_STATE_NOT_CHARGING:
		case CHARGER_STATE_FULL:
			if (TCA6507_LED_PREV_STATE_OFF == tca6507_drvdata.prev_led_state[i]) {
				is_fade_on = true;			
			}

			break;
		default:
			dev_err(tca6507_dev, "%s: ERROR LED STATE!!\n", __func__);
		}
	}
	
	tca6507_drvdata.is_blinking = is_blinking;
        
	for (i = 0; i < 3; i++) {
		switch (state[i]) {
		case TCA6507_LED_OFF:
			if (TCA6507_LED_PREV_STATE_ON == tca6507_drvdata.prev_led_state[i]) {
				if (is_fade_off && is_fade_on) {
					tca6507_led_fade_off(i, cmd_fade_off);
					tca6507_led_turn_off(i, cmd);	
				} else {
					tca6507_led_fade_off(i, cmd);
				}
			} else {
				tca6507_led_turn_off(i, cmd);			
			}
			
			tca6507_drvdata.prev_led_state[i] = TCA6507_LED_PREV_STATE_OFF;

			break;
		case TCA6507_LED_BLINK:
		case CHARGER_STATE_LOW_POWER:
			tca6507_led_blink(i, cmd);
			
			tca6507_drvdata.prev_led_state[i] = TCA6507_LED_PREV_STATE_BLINK;

			break;
		case CHARGER_STATE_DISCHARGING:
		case TCA6507_LED_ON:
		case TCA6507_LED_ATTENTION:
		case CHARGER_STATE_UNKNOWN:
		case CHARGER_STATE_CHARGING:
		case CHARGER_STATE_NOT_CHARGING:
		case CHARGER_STATE_FULL:
			if (TCA6507_LED_PREV_STATE_OFF == tca6507_drvdata.prev_led_state[i]) {
				if (is_fade_off && is_fade_on)
					tca6507_led_turn_off(i, cmd_fade_off);
					
				tca6507_led_fade_on(i, cmd);				
			} else {
				tca6507_led_turn_on(i, cmd);			
			}
			
			tca6507_drvdata.prev_led_state[i] = TCA6507_LED_PREV_STATE_ON;

			break;
		default:
			dev_err(tca6507_dev, "%s: ERROR LED STATE!!\n", __func__);
		}
	}
	
	if (is_fade_off && is_fade_on) {
		ret = tca6507_write(tca6507_drvdata.tca6507_i2c_client, cmd_fade_off, sizeof(cmd_fade_off));
		if (ret < 0) {
			dev_err(tca6507_dev, "%s: i2c_write failed<cmd_fade_off>!!\n", __func__);
		}
		mdelay(800);
	}

	ret = tca6507_write(tca6507_drvdata.tca6507_i2c_client, cmd, sizeof(cmd));
	if (ret < 0) {
		dev_err(tca6507_dev, "%s: i2c_write failed!!\n", __func__);
	}
	
	return ret;
}

void tca6507_charger_state_report(int state) 
{
	struct device *tca6507_dev = &tca6507_drvdata.tca6507_i2c_client->dev;
	int i;

	wake_lock(&tca6507_drvdata.charger_state_suspend_wake_lock);
	
	tca6507_drvdata.is_charger_connected = false;
	
	switch (state) {
	case CHARGER_STATE_UNKNOWN2:		//RED
		gCharger_state = CHARGER_STATE_UNKNOWN;
		break;
	case CHARGER_STATE_CHARGING2:		//RED
		gCharger_state = CHARGER_STATE_CHARGING;
		tca6507_drvdata.is_charger_connected = true;
		break;
	case CHARGER_STATE_DISCHARGING2:	//DNP
		gCharger_state = CHARGER_STATE_DISCHARGING;
		tca6507_drvdata.is_charger_connected = true;
		break;
	case CHARGER_STATE_NOT_CHARGING2:	//ALL LEDS OFF
		gCharger_state = CHARGER_STATE_NOT_CHARGING;
		break;
	case CHARGER_STATE_FULL2:		//GREEN	
		gCharger_state = CHARGER_STATE_FULL;
		tca6507_drvdata.is_charger_connected = true;
		break;
	case CHARGER_STATE_LOW_POWER2:		//RED BLINK
		gCharger_state = CHARGER_STATE_LOW_POWER;
	}

	mutex_lock(&tca6507_drvdata.tca6507_lock);
	for (i = 0; i < 3; i++) {
		tca6507_drvdata.led_state[i] &= 0x000F;
	}
	
	dev_info(tca6507_dev, "%s: CHARGER STATE <0x%04x>!!\n", __func__, gCharger_state);
	
	switch (gCharger_state) {
	case CHARGER_STATE_UNKNOWN:		//RED
		tca6507_drvdata.led_state[TCA6507_LED_LEFT]	|= 0 << CHARGER_STATE_UNKNOWN;
		tca6507_drvdata.led_state[TCA6507_LED_CENTER]	|= 0 << CHARGER_STATE_UNKNOWN;
		tca6507_drvdata.led_state[TCA6507_LED_RIGHT]	|= 1 << CHARGER_STATE_UNKNOWN;
		break;
	case CHARGER_STATE_CHARGING:		//RED
		tca6507_drvdata.led_state[TCA6507_LED_LEFT]	|= 0 << CHARGER_STATE_CHARGING;
		tca6507_drvdata.led_state[TCA6507_LED_CENTER]	|= 0 << CHARGER_STATE_CHARGING;
		tca6507_drvdata.led_state[TCA6507_LED_RIGHT]	|= 1 << CHARGER_STATE_CHARGING;
		break;
	case CHARGER_STATE_DISCHARGING:		//DNP
		tca6507_drvdata.led_state[TCA6507_LED_LEFT]	|= 0 << CHARGER_STATE_DISCHARGING;
		tca6507_drvdata.led_state[TCA6507_LED_CENTER]	|= 0 << CHARGER_STATE_DISCHARGING;
		tca6507_drvdata.led_state[TCA6507_LED_RIGHT]	|= 0 << CHARGER_STATE_DISCHARGING;
		break;
	case CHARGER_STATE_NOT_CHARGING:	//RED & GREEN
		tca6507_drvdata.led_state[TCA6507_LED_LEFT]	|= 0 << CHARGER_STATE_NOT_CHARGING;
		tca6507_drvdata.led_state[TCA6507_LED_CENTER]	|= 0 << CHARGER_STATE_NOT_CHARGING;
		tca6507_drvdata.led_state[TCA6507_LED_RIGHT]	|= 0 << CHARGER_STATE_NOT_CHARGING;
		break;
	case CHARGER_STATE_FULL:		//GREEN
		tca6507_drvdata.led_state[TCA6507_LED_LEFT]	|= 1 << CHARGER_STATE_FULL;
		tca6507_drvdata.led_state[TCA6507_LED_CENTER]	|= 0 << CHARGER_STATE_FULL;
		tca6507_drvdata.led_state[TCA6507_LED_RIGHT]	|= 0 << CHARGER_STATE_FULL;
		break;
	case CHARGER_STATE_LOW_POWER:		//BLINK RED
		tca6507_drvdata.led_state[TCA6507_LED_LEFT]	|= 0 << CHARGER_STATE_LOW_POWER;
		tca6507_drvdata.led_state[TCA6507_LED_CENTER]	|= 0 << CHARGER_STATE_LOW_POWER;
		tca6507_drvdata.led_state[TCA6507_LED_RIGHT]	|= 1 << CHARGER_STATE_LOW_POWER;
	}

	tca6507_set_led();

	mutex_unlock(&tca6507_drvdata.tca6507_lock);
	wake_unlock(&tca6507_drvdata.charger_state_suspend_wake_lock);
}
EXPORT_SYMBOL(tca6507_charger_state_report);

void tca6507_led_switch(bool on)
{
	int i;
	
	mutex_lock(&tca6507_drvdata.tca6507_lock);
	
	for (i = 0; i < 3; i++) {
		tca6507_drvdata.led_state[i] &= 0xFFFC;
	}
		
	tca6507_drvdata.led_state[TCA6507_LED_LEFT]	|= 1 << (on ? TCA6507_LED_ON : TCA6507_LED_OFF);
	tca6507_drvdata.led_state[TCA6507_LED_CENTER]	|= 1 << (on ? TCA6507_LED_ON : TCA6507_LED_OFF);
	tca6507_drvdata.led_state[TCA6507_LED_RIGHT]	|= 1 << (on ? TCA6507_LED_ON : TCA6507_LED_OFF);		
		
	tca6507_set_led();
	
	mutex_unlock(&tca6507_drvdata.tca6507_lock);	
	
}
EXPORT_SYMBOL(tca6507_led_switch);

void tca6507_center_blink(bool blink)
{
	mutex_lock(&tca6507_drvdata.tca6507_lock);
	
	tca6507_drvdata.led_state[TCA6507_LED_CENTER]	&= 0xFFF3;
	tca6507_drvdata.led_state[TCA6507_LED_CENTER]	|= (blink ? 1 : 0) << TCA6507_LED_BLINK;	
	
	tca6507_set_led();

	mutex_unlock(&tca6507_drvdata.tca6507_lock);	
}
EXPORT_SYMBOL(tca6507_center_blink);

void tca6507_center_attention(bool attention)
{
	mutex_lock(&tca6507_drvdata.tca6507_lock);
	
	tca6507_drvdata.led_state[TCA6507_LED_CENTER]	&= 0xFFF3;
	tca6507_drvdata.led_state[TCA6507_LED_CENTER]	|= (attention ? 1 : 0) << TCA6507_LED_ATTENTION;
	tca6507_drvdata.is_attention			= attention;

	tca6507_set_led();
	
	mutex_unlock(&tca6507_drvdata.tca6507_lock);	
	
}
EXPORT_SYMBOL(tca6507_center_attention);

static int tca6507_led_init(void)
{
	uint8_t cmd[12];
	struct device *tca6507_dev = &tca6507_drvdata.tca6507_i2c_client->dev;
	int ret = 0;

	mutex_lock(&tca6507_drvdata.tca6507_lock);
	
	cmd[0] = TCA6507_INITIALIZATION_REGISTER;

	cmd[1] = 0x00;  //Select 0
	cmd[2] = 0x02;  //Select 1
	cmd[3] = 0x00;  //Select 2
   
	//Fade On timer
	cmd[4] = 0x77;

   	 //Fully On timer
	cmd[5] = 0x44;

	//Fade Off timer
	cmd[6] = 0x77;

	//Fully Off timer 1
	cmd[7] = 0xBB;

	//Fully Off timer 2
	cmd[8] = 0xBB;

	cmd[9] = 0xFF;
	cmd[10] = 0xCF;
	cmd[11] = 0x88;
		
	ret = tca6507_write(tca6507_drvdata.tca6507_i2c_client, cmd, sizeof(cmd));
	
	mutex_unlock(&tca6507_drvdata.tca6507_lock);

	if (ret < 0) {
		dev_err(tca6507_dev, "%s: i2c_write failed!!\n", __func__);
		return ret;
	}

	return ret;
}
static ssize_t tca6507_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 loc_buf[4096];
	u8 cmd[12];
	int bytes_read = 0;
	int i;
	
	cmd[0] = TCA6507_INITIALIZATION_REGISTER;

	mutex_lock(&tca6507_drvdata.tca6507_lock);		
		tca6507_read(tca6507_drvdata.tca6507_i2c_client, cmd, sizeof(cmd) - 1);
	mutex_unlock(&tca6507_drvdata.tca6507_lock);
		
	for (i = 0; i < 12; i++) {
		bytes_read += sprintf(&loc_buf[bytes_read], "[0x%02x] 0x%02x\n", i, cmd[i]);
	}
	
	bytes_read += sprintf(&loc_buf[bytes_read], "\n%s\n LEFT: 0x%04x CENTER: 0x%04x RIGHT: 0x%04x\n", 
				__func__,
				tca6507_drvdata.led_state[TCA6507_LED_LEFT],
				tca6507_drvdata.led_state[TCA6507_LED_CENTER],
				tca6507_drvdata.led_state[TCA6507_LED_RIGHT]
				);
				
	sprintf(buf, "%s", loc_buf);

	return bytes_read;
}

static ssize_t tca6507_debug_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned cmd_number;
	
	sscanf(buf, "%3d\n", &cmd_number);
	
	dev_dbg(dev, "%s: COMMAND: %d\n", __func__, cmd_number);

	tca6507_charger_state_report(cmd_number);

	return count;
}

DEVICE_ATTR(tca6507_debug, 0644, tca6507_debug_show, tca6507_debug_store);

#ifdef CONFIG_PM
static int tca6507_suspend(struct i2c_client *nLeds, pm_message_t mesg)
{
	u8 cmd[7] = {0x13, 0x55, 0x11, 0x55, 0xDD, 0xDD, 0xFF};
	/// +++ FIH_ADQ +++ , MichaelKao 2009.06.08
	///add for low battery LED blinking in suspend mode 
	Battery_power_supply_change();
	/// --- FIH_ADQ ---

	mutex_lock(&tca6507_drvdata.tca6507_lock);
	if (tca6507_drvdata.is_blinking || tca6507_drvdata.is_attention || tca6507_drvdata.is_charger_connected) {
		tca6507_write(tca6507_drvdata.tca6507_i2c_client, cmd, sizeof(cmd));
	} else {
		gpio_init_setting(0);
		gpio_release();
	}

	dev_dbg(&nLeds->dev, "%s: ENTER SUSPEND MODE\n", __func__);

	return 0;
}

static int tca6507_resume(struct i2c_client *nLeds)
{
	if (tca6507_drvdata.is_blinking || tca6507_drvdata.is_attention || tca6507_drvdata.is_charger_connected) {
	} else {
		gpio_init_setting(1);
		gpio_release();
	}
	
	mutex_unlock(&tca6507_drvdata.tca6507_lock);

	dev_dbg(&nLeds->dev, "%s: LEAVE SUSPEND MODE\n", __func__);

	return 0;
}
#else
# define tca6507_suspend NULL
# define tca6507_resume  NULL
#endif

/******************Driver Functions*******************/
static int __devinit tca6507_probe(struct i2c_client *client,
				    const struct i2c_device_id *id)
{
	struct tca6507_platform_data *tca6507_pd = client->dev.platform_data;
  	int i, ret = 0;

	i2c_set_clientdata(client, &tca6507_drvdata);
	tca6507_drvdata.reset_pin		= tca6507_pd->tca6507_reset;
	tca6507_drvdata.tca6507_i2c_client	= client;
	tca6507_drvdata.HWID			= FIH_READ_HWID_FROM_SMEM();
	tca6507_drvdata.is_blinking		= false;
	tca6507_drvdata.is_attention		= false;
	tca6507_drvdata.is_charger_connected	= false;
	for (i = 0; i < 3; i++) {
		tca6507_drvdata.led_state[i] = 0x0001;// Initial: TURN_OFF
		tca6507_drvdata.prev_led_state[i] = TCA6507_LED_PREV_STATE_OFF;
		tca6507_drvdata.is_fade_on[i] = true;
	}
	mutex_init(&tca6507_drvdata.tca6507_lock);
	wake_lock_init(&tca6507_drvdata.charger_state_suspend_wake_lock, WAKE_LOCK_SUSPEND, "charger_state_suspend_work");
	
	ret = tca6507_led_init();
	if(ret < 0) {
		dev_err(&client->dev, "tca6507 LED init failed\n");
		mutex_destroy(&tca6507_drvdata.tca6507_lock);
		
		return ret;
	}
	
	ret = device_create_file(&client->dev, &dev_attr_tca6507_debug);
	if (ret < 0) {
		dev_err(&client->dev, "%s: Create keyboard attribute \"brightness\" failed!! <%d>", __func__, ret);
		mutex_destroy(&tca6507_drvdata.tca6507_lock);

		return ret; 
	}
	
	return ret;
}

static int __devexit tca6507_remove(struct i2c_client *client)
{
	int ret = 0;
	
	mutex_destroy(&tca6507_drvdata.tca6507_lock);
	device_remove_file(&client->dev, &dev_attr_tca6507_debug);
	wake_lock_destroy(&tca6507_drvdata.charger_state_suspend_wake_lock);

	return ret;
}

static const struct i2c_device_id tca6507_idtable[] = {
       { "tca6507", 0 },
       { }
};

static struct i2c_driver tca6507_driver = {
	.driver = {
		.name	= "tca6507",
	},
	.probe		= tca6507_probe,
	.remove		= __devexit_p(tca6507_remove),
	.suspend  	= tca6507_suspend,
	.resume   	= tca6507_resume,
	.id_table	= tca6507_idtable,
};

static int __init tca6507_init(void)
{
	int ret = 0;

	printk( KERN_INFO "Driver init: %s\n", __func__ );

	// i2c
	ret = i2c_add_driver(&tca6507_driver);
	if (ret) {
		printk(KERN_ERR "%s: Driver registration failed, module not inserted.\n", __func__);
		goto driver_del;
	}

	//all successfully.
	return ret;

driver_del:
	i2c_del_driver(&tca6507_driver);
	
	return -1;
}

static void __exit tca6507_exit(void)
{
	printk( KERN_INFO "Driver exit: %s\n", __func__ );
	i2c_del_driver(&tca6507_driver);
}

module_init(tca6507_init);
module_exit(tca6507_exit);

MODULE_AUTHOR( "Audi PC Huang <audipchuang@fihtdc.com>" );
MODULE_DESCRIPTION( "TCA6507 driver" );
MODULE_LICENSE( "GPL" );
