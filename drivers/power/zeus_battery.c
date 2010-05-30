/* drivers/power/goldfish_battery.c
 *
 * Power supply driver for the goldfish emulator
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
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

#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <linux/reboot.h>
#include <linux/delay.h>

#define T_FIH
#ifdef T_FIH	///+T_FIH
#include <linux/gasgauge_bridge.h>

#define FLAG_BATTERY_POLLING
#define FLAG_CHARGER_DETECT
#define CHR_EN 33
#endif	// T_FIH	///-T_FIH

/* FIH_ADQ, Kenny { */
#include "linux/pmlog.h"
/* } FIH_ADQ, Kenny */

/*+++FIH_ADQ+++*/
enum {
	CHARGER_STATE_UNKNOWN,		
	CHARGER_STATE_CHARGING,		
	CHARGER_STATE_DISCHARGING,	
	CHARGER_STATE_NOT_CHARGING,	
	CHARGER_STATE_FULL,
	CHARGER_STATE_LOW_POWER,
};
extern void tca6507_charger_state_report(int state);
static int g_charging_state_last = CHARGER_STATE_UNKNOWN;
static void polling_reset_func();
void temperature_detect();
/* FIH_ADQ, Kenny { */
static void gasgauge_param_reset();
/* } FIH_ADQ, Kenny */
/*+++FIH_ADQ+++*/

struct goldfish_battery_data {
	uint32_t reg_base;
	int irq;
	spinlock_t lock;

	struct power_supply battery;
	///struct power_supply ac;
};

#define GOLDFISH_BATTERY_READ(data, addr)   (readl(data->reg_base + addr))
#define GOLDFISH_BATTERY_WRITE(data, addr, x)   (writel(x, data->reg_base + addr))
///extern int check_USB_type;

/* temporary variable used between goldfish_battery_probe() and goldfish_battery_open() */
static struct goldfish_battery_data *battery_data;
#ifdef T_FIH	///+T_FIH
static int g_charging_state = CHARGER_STATE_NOT_CHARGING;
#endif	// T_FIH	///-T_FIH

/* FIH_ADQ, Kenny { */
extern int check_USB_type;
static uint8_t charging_tcount = 0;
/* } FIH_ADQ, Kenny */

enum {
	/* status register */
	BATTERY_INT_STATUS	    = 0x00,
	/* set this to enable IRQ */
	BATTERY_INT_ENABLE	    = 0x04,

	BATTERY_AC_ONLINE       = 0x08,
	BATTERY_STATUS          = 0x0C,
	BATTERY_HEALTH          = 0x10,
	BATTERY_PRESENT         = 0x14,
	BATTERY_CAPACITY        = 0x18,

	BATTERY_STATUS_CHANGED	= 1U << 0,
	AC_STATUS_CHANGED   	= 1U << 1,
	BATTERY_INT_MASK        = BATTERY_STATUS_CHANGED | AC_STATUS_CHANGED,
};


///static int goldfish_ac_get_property(struct power_supply *psy,
///			enum power_supply_property psp,
///			union power_supply_propval *val)
///{
///	struct goldfish_battery_data *data = container_of(psy,
///		struct goldfish_battery_data, ac);
///	int ret = 0;
///
///	switch (psp) {
///	case POWER_SUPPLY_PROP_ONLINE:
///                // printk(KERN_INFO "<ubh> goldfish_ac_get_property : POWER_SUPPLY_PROP_ONLINE : type(%d)\r\n", check_USB_type); 
///                 ///if (check_USB_type == 2)
///                 ///val->intval = 1;
///                 ///else
///                 ///val->intval = 0;
///		//val->intval = GOLDFISH_BATTERY_READ(data, BATTERY_AC_ONLINE);
///		break;
///	default:
///		ret = -EINVAL;
///		break;
///	}
///	return ret;
///}
static struct power_supply * g_ps_battery;

/// +++ FIH_ADQ +++ , MichaelKao 2009.06.08
///add for low battery LED blinking in suspend mode 
void Battery_power_supply_change(void)
{
	power_supply_changed(g_ps_battery);
	/* FIH_ADQ, Kenny { */
	printk(KERN_INFO "One uevent from suspend\r\n");
	/* } FIH_ADQ, Kenny */
}
EXPORT_SYMBOL(Battery_power_supply_change);
/// --- FIH_ADQ ---

static int goldfish_battery_get_property(struct power_supply *psy,
				 enum power_supply_property psp,
				 union power_supply_propval *val)
{
	int buf;
	int ret = 0;
	/* FIH_ADQ, Kenny { */
	int batt_vol;
	/* } FIH_ADQ, Kenny */

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		//printk(KERN_INFO "<ubh> goldfish_battery_get_property : POWER_SUPPLY_PROP_STATUS\r\n");
		// "Unknown", "Charging", "Discharging", "Not charging", "Full"
                if (g_charging_state != CHARGER_STATE_LOW_POWER)
		val->intval = g_charging_state;
                else 
                val->intval = CHARGER_STATE_NOT_CHARGING;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		//GetBatteryInfo(BATT_AVCURRENT_INFO, &buf);
		//printk(KERN_INFO "<ubh> goldfish_battery_get_property : POWER_SUPPLY_PROP_HEALTH : AVC(%d)\r\n", buf);
		// "Unknown", "Good", "Overheat", "Dead", "Over voltage", "Unspecified failure"
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		//GetBatteryInfo(BATT_CURRENT_INFO, &buf);
		//printk(KERN_INFO "<ubh> goldfish_battery_get_property : POWER_SUPPLY_PROP_PRESENT : C(%d)\r\n", buf);
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		//printk(KERN_INFO "<ubh> goldfish_battery_get_property : POWER_SUPPLY_PROP_TECHNOLOGY\r\n");
		// "Unknown", "NiMH", "Li-ion", "Li-poly", "LiFe", "NiCd", "LiMn"
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
        case POWER_SUPPLY_PROP_VOLTAGE_NOW:
   		ret = GetBatteryInfo(BATT_VOLTAGE_INFO, &buf);
		if (ret >=0 ) {
			val->intval = buf*1000;
		} else {
			val->intval = 0;
		}
		break;
        case POWER_SUPPLY_PROP_TEMP:
		ret = GetBatteryInfo(BATT_TEMPERATURE_INFO, &buf);
		if (ret >=0 ) {
			val->intval = buf;
		} else {
			val->intval = 0;
		}
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		// +++ADQ_FIH+++ 
		ret = GetBatteryInfo(BATT_CAPACITY_INFO, &buf);
		if (ret < 0){
			printk(KERN_INFO "POWER_SUPPLY_PROP_CAPACITY : Get data failed\n");
			power_supply_changed(g_ps_battery);
			ret = 0;
		}
		else{
			val->intval = buf;
			
			if((g_charging_state == CHARGER_STATE_CHARGING) && (val->intval != 255)){
			    temperature_detect();
			    /* FIH_ADQ, Kenny { */
		        gasgauge_param_reset();
		        /* } FIH_ADQ, Kenny */
		    }
			
			//GetBatteryInfo(BATT_VOLTAGE_INFO, &buf);
			//buf = buf * 100 / 4200;
			//printk(KERN_INFO "<ubh> goldfish_battery_get_property : POWER_SUPPLY_PROP_CAPACITY : Cap(%d) state = %d\r\n", val->intval,g_charging_state);
#if 1
			printk("<8>" "batt : %d\%_%d\n", val->intval, g_charging_state);
			/* FIH_ADQ, Kenny { */
			if(GetBatteryInfo(BATT_VOLTAGE_INFO, &batt_vol) >= 0)
			    pmlog("batt : %d\%_%dmV_%d\n", val->intval, batt_vol, g_charging_state);
			else
			    pmlog("batt : %d\%_%d\n", val->intval, g_charging_state);
			/* } FIH_ADQ, Kenny */
#else
			GetBatteryInfo(BATT_CURRENT_INFO, &buf);
			printk("<8>" "batt : %d\%_%d_%d\n", val->intval, buf, g_charging_state);
#endif
			// +++ADQ_FIH+++
			//
			//if( val->intval > 120 ){
			//	kernel_power_off();
			//}
			if ((val->intval > 94) && (g_charging_state == CHARGER_STATE_CHARGING)){//full
				g_charging_state = CHARGER_STATE_FULL;
				printk(KERN_INFO "goldfish_battery_get_property : set the charging status to full\n");
			}
			else if (g_charging_state == CHARGER_STATE_FULL) {	 		//charging
			  	if (val->intval < 95) {
			  	  	g_charging_state = CHARGER_STATE_CHARGING;
			  	  	printk(KERN_INFO "goldfish_battery_get_property : set the charging status to charging\n");
				}
			}
			else if (g_charging_state == CHARGER_STATE_NOT_CHARGING){ 		//low_power
			    /* FIH_ADQ, Kenny { */
			    charging_tcount = 0;
			    /* } FIH_ADQ, Kenny */
				if (val->intval < 15) 
					g_charging_state = CHARGER_STATE_LOW_POWER; 
			}

			if(g_charging_state == CHARGER_STATE_FULL && val->intval > 94) 		//recharging
			{
				if(val->intval < 97){
					polling_reset_func();	
					printk( "charing ic reset val=%d state=%d\n ", val->intval, g_charging_state);
				}
				val->intval = 100;	
				printk( "batt : %d\%_%d\n", val->intval, g_charging_state);
			}

			if (val->intval < 95){
				val->intval = val->intval*100/95;
				/* FIH_ADQ, Kenny { */
				if((val->intval == 0) && (batt_vol >= 3400) && (g_charging_state != CHARGER_STATE_CHARGING)){
				    val->intval = 1;
				    printk( "batt : 0%% but voltage %dmV >= 3400mV, don't enter power-off, keep report 1%%\n", batt_vol);
				}
				else if((val->intval == 1) && (batt_vol < 3400) && (g_charging_state != CHARGER_STATE_CHARGING)){
				    val->intval = 0;
				    printk( "batt : 1%% but voltage %dmV < 3400mV, enter power-off, report 0%%\n", batt_vol);
				}
				/* } FIH_ADQ, Kenny */ 
			}else{
				val->intval = 100;
			}

			if ((g_charging_state_last != g_charging_state))
				tca6507_charger_state_report(g_charging_state);   

			g_charging_state_last = g_charging_state;

			// ---ADQ_FIH---
		}
		// ---ADQ_FIH--- 
		break;
	default:
		printk(KERN_ERR "goldfish_battery_get_property : psp(%d)\n", psp);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static enum power_supply_property goldfish_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
//	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
        POWER_SUPPLY_PROP_TEMP,
        POWER_SUPPLY_PROP_VOLTAGE_NOW,

};

///static enum power_supply_property goldfish_ac_props[] = {
///	POWER_SUPPLY_PROP_ONLINE,
///};


#ifdef T_FIH	///+T_FIH
#ifdef FLAG_BATTERY_POLLING
static struct timer_list polling_timer;
//static struct timer_list reset_chgen;


#define BATTERY_POLLING_TIMER  60000//300000 10000

static void polling_timer_func(unsigned long unused)
{
	power_supply_changed(g_ps_battery);
	/* FIH_ADQ, Kenny { */
    printk(KERN_INFO "One uevent from polling\r\n");
    /* } FIH_ADQ, Kenny */
	mod_timer(&polling_timer,
		  jiffies + msecs_to_jiffies(BATTERY_POLLING_TIMER));
}
#endif	// FLAG_BATTERY_POLLING

#ifdef FLAG_CHARGER_DETECT
#include <asm/gpio.h>

/* FIH_ADQ, Kenny { */
static void gasgauge_param_reset(){
    int rc, batt_scalar, batt_vol=0, batt_cur=0;
    
    if(GetBatteryInfo(BATT_VOLTAGE_INFO, &batt_vol) < 0){
	    printk(KERN_INFO "batt: Get voltage failed!\n");
	}
    
    if(GetBatteryInfo(BATT_CURRENT_INFO, &batt_cur) < 0){
	    printk(KERN_INFO "batt: Get current failed!\n");
	}
    
    if(GetBatteryInfo(BATT_AS_REGISTER, &batt_scalar) >= 0){
        printk(KERN_INFO "batt: voltage=%dmV, current=%dmA, scalar=0x%x\n", batt_vol, (batt_cur*7813)/100000, batt_scalar);
        if(batt_scalar != 128){
            //reset age scalar
            if(SetBatteryInfo(BATT_AS_REGISTER, 128) >= 0){
                printk(KERN_INFO "batt: Change scalar from %d to 128\n", batt_scalar);
            }else{
                printk(KERN_INFO "batt: Fail to change scalar from %d to 128\n", batt_scalar);
            }
        }
    }else{
        printk(KERN_INFO "batt: Get age scalar failed!\n");
    }

	if((batt_vol >= 4100) && check_USB_type == 2){
	    rc = gpio_request(57, "CHR_1A");
        if (rc)	printk(KERN_ERR "CHR_1A setting failed!\n");
        rc = gpio_get_value(57);
        if(rc == 1){
            gpio_set_value(57,0);
            printk(KERN_INFO "batt: voltage > 4.1V and AC charging, set the charging current to 500mA!!\n");
        }
        gpio_free(57);
	}
	
	if(charging_tcount < 6){
	    charging_tcount++;
	    printk(KERN_INFO "batt: charging tcount=%d!!\n",charging_tcount);
	}
	
	if((batt_vol < 4100) && (batt_cur < 0) && (charging_tcount == 6)){
	    printk(KERN_INFO "batt: Reset charger IC due to voltage < 4.1mV and current < 0!!\n");
	    polling_reset_func();
	    charging_tcount = 0;
	}
	
}
/* } FIH_ADQ, Kenny */

void temperature_detect()
{
	int rc, ret=0, buf;

	ret = GetBatteryInfo(BATT_TEMPERATURE_INFO, &buf);
	printk(KERN_INFO "BATT_TEMPERATURE_INFO %d\r\n",buf);

	if( buf > 450 || buf < 0 ){
		rc = gpio_request(CHR_EN, "CHG_EN");
		if(rc){
			printk(KERN_INFO "CHG_EN_REQUEST_FAIL\r\n");	
		}
		/* FIH_ADQ, Kenny { */
		charging_tcount = 0;
		/* } FIH_ADQ, Kenny */
		rc = gpio_get_value(CHR_EN);
		if(rc == 0){
			printk(KERN_INFO "Shutdown charging IC\r\n");	
			gpio_set_value(CHR_EN,1);
		}else{
			printk(KERN_INFO "Temperature is too high and user must be a bad man\r\n");	
		}
		gpio_free(CHR_EN);
	}else{
		rc = gpio_request(CHR_EN, "CHG_EN");
		if(rc){
			printk(KERN_INFO "CHG_EN_REQUEST_FAIL\r\n");	
		}
		rc = gpio_get_value(CHR_EN);
		if(rc == 1){
			printk(KERN_INFO "Restart charging IC\r\n");	
			gpio_set_value(CHR_EN,0);
		}else{
			printk(KERN_INFO "God bless you\r\n");	
		}
		gpio_free(CHR_EN);
	}
}

static void polling_reset_func()
{
	int rc;
	if(g_charging_state == CHARGER_STATE_CHARGING || g_charging_state == CHARGER_STATE_FULL)
	{
		disable_irq(MSM_GPIO_TO_INT(39));
		rc = gpio_request(CHR_EN,"CHG_EN");
		if(rc){
			printk(KERN_INFO "CHG_EN_REQUEST_FAIL\r\n");	
		}
		gpio_set_value(CHR_EN, 1);
		mdelay(100);
		gpio_set_value(CHR_EN, 0);
		gpio_free(CHR_EN);
		printk(KERN_INFO "Charging ic reset\r\n");
		enable_irq(MSM_GPIO_TO_INT(39));

		rc = gpio_request(CHR_EN,"CHG_FLT");
		rc = gpio_get_value(CHR_EN);
		printk(KERN_INFO "FLT pin = %d\r\n",rc);
		rc = gpio_get_value(39);	
		printk(KERN_INFO "CHR pin = %d\r\n",rc);
		gpio_free(CHR_EN);

	}
}

#define GPIO_CHR_DET 39		// Input power-good (USB port/adapter present indicator) pin
#define GPIO_CHR_FLT 32		// Over-voltage fault flag

static irqreturn_t chgdet_irqhandler(int irq, void *dev_id)
{
	g_charging_state = (gpio_get_value(GPIO_CHR_DET)) ? CHARGER_STATE_NOT_CHARGING : CHARGER_STATE_CHARGING;
	power_supply_changed(g_ps_battery);
	/* FIH_ADQ, Kenny { */
	printk(KERN_INFO "One uevent from interrupt\r\n");
	/* } FIH_ADQ, Kenny */
	return IRQ_HANDLED;
}
#endif	// FLAG_CHARGER_DETECT
#endif	// T_FIH	///-T_FIH


static int goldfish_battery_probe(struct platform_device *pdev)
{
	int ret;
	struct goldfish_battery_data *data;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (data == NULL) {
		ret = -ENOMEM;
		goto err_data_alloc_failed;
	}
	spin_lock_init(&data->lock);

	data->battery.properties = goldfish_battery_props;
	data->battery.num_properties = ARRAY_SIZE(goldfish_battery_props);
	data->battery.get_property = goldfish_battery_get_property;
	data->battery.name = "battery";
	data->battery.type = POWER_SUPPLY_TYPE_BATTERY;

/*	data->ac.properties = goldfish_ac_props;
	data->ac.num_properties = ARRAY_SIZE(goldfish_ac_props);
	data->ac.get_property = goldfish_ac_get_property;
	data->ac.name = "ac";
	data->ac.type = POWER_SUPPLY_TYPE_MAINS;*/

	ret = power_supply_register(&pdev->dev, &data->battery);
	if (ret)
		goto err_battery_failed;
        ///ret = power_supply_register(&pdev->dev, &data->ac);
	///if (ret)
		///goto err_battery_failed;

	platform_set_drvdata(pdev, data);
	battery_data = data;

#ifdef T_FIH	///+T_FIH
#ifdef FLAG_BATTERY_POLLING
	setup_timer(&polling_timer, polling_timer_func, 0);
	mod_timer(&polling_timer,
		  jiffies + msecs_to_jiffies(BATTERY_POLLING_TIMER));

	g_ps_battery = &(data->battery);
#endif	// FLAG_BATTERY_POLLING

#ifdef FLAG_CHARGER_DETECT
	gpio_tlmm_config( GPIO_CFG(GPIO_CHR_DET, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA ), GPIO_ENABLE );
	ret = gpio_request(GPIO_CHR_DET, "gpio_keybd_irq");
	if (ret)
		printk(KERN_INFO "<ubh> goldfish_battery_probe 04. : IRQ init fails!!!\r\n");
	ret = gpio_direction_input(GPIO_CHR_DET);
	if (ret)
		printk(KERN_INFO "<ubh> goldfish_battery_probe 05. : gpio_direction_input fails!!!\r\n");
	ret = request_irq(MSM_GPIO_TO_INT(GPIO_CHR_DET), &chgdet_irqhandler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, pdev->name, NULL);
	if (ret)
		printk(KERN_INFO "<ubh> goldfish_battery_probe 06. : request_irq fails!!!\r\n");
#endif	// FLAG_CHARGER_DETECT
#endif	// T_FIH	///-T_FIH

	return 0;

err_battery_failed:
	kfree(data);
err_data_alloc_failed:
	return ret;
}

static int goldfish_battery_remove(struct platform_device *pdev)
{
	struct goldfish_battery_data *data = platform_get_drvdata(pdev);

#ifdef T_FIH	///+T_FIH
#ifdef FLAG_CHARGER_DETECT
	free_irq(MSM_GPIO_TO_INT(GPIO_CHR_DET), NULL);
	gpio_free(GPIO_CHR_DET);
#endif	// FLAG_CHARGER_DETECT

#ifdef FLAG_BATTERY_POLLING
	del_timer_sync(&polling_timer);
#endif	// FLAG_BATTERY_POLLING
#endif	// T_FIH	///-T_FIH

	power_supply_unregister(&data->battery);
	///power_supply_unregister(&data->ac);

	free_irq(data->irq, data);
	kfree(data);
	battery_data = NULL;
	return 0;
}

static struct platform_driver goldfish_battery_device = {
	.probe		= goldfish_battery_probe,
	.remove		= goldfish_battery_remove,
	.driver = {
		.name = "goldfish-battery"
	}
};

static int __init goldfish_battery_init(void)
{
	return platform_driver_register(&goldfish_battery_device);
}

static void __exit goldfish_battery_exit(void)
{
	platform_driver_unregister(&goldfish_battery_device);
}

module_init(goldfish_battery_init);
module_exit(goldfish_battery_exit);

MODULE_AUTHOR("Mike Lockwood lockwood@android.com");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Battery driver for the Goldfish emulator");
