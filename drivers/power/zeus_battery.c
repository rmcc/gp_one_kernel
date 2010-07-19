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

#ifdef CONFIG_USB_GADGET_MSM_72K 
#include <mach/msm_hsusb.h>
#endif

#include <asm/gpio.h>
#define USBSET 97
#define CHR_EN 33
#define CHR_1A 57

#include <linux/gasgauge_bridge.h>

#define FLAG_BATTERY_POLLING
#define FLAG_CHARGER_DETECT

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
static void polling_reset_func(void);
void temperature_detect(void);
/* FIH_ADQ, Kenny { */
static void gasgauge_param_reset(void);
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

static int g_charging_state = CHARGER_STATE_NOT_CHARGING;
static int g_health = POWER_SUPPLY_HEALTH_UNKNOWN;

/* FIH_ADQ, Kenny { */
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

static enum power_supply_property zeus_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,

};

static enum power_supply_property zeus_power_properties[] = {
    POWER_SUPPLY_PROP_ONLINE,
};

static char *supply_list[] = {
    "battery",
};

typedef enum {
    CHARGER_BATTERY = 0,
    CHARGER_USB,
    CHARGER_AC
} charger_type_t;

charger_type_t current_charger = CHARGER_BATTERY;

static int zeus_power_get_property(struct power_supply *psy,
                    enum power_supply_property psp,
                    union power_supply_propval *val)
{
    switch (psp) {
    case POWER_SUPPLY_PROP_ONLINE:
        if (psy->type == POWER_SUPPLY_TYPE_MAINS)
            val->intval = (current_charger ==  CHARGER_AC ? 1 : 0);
        else if (psy->type == POWER_SUPPLY_TYPE_USB)
            val->intval = (current_charger ==  CHARGER_USB ? 1 : 0);
        else
            val->intval = 0;
        break;
    default:
        return -EINVAL;
    }

    return 0;
}

static int zeus_battery_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val);

static struct power_supply zeus_power_supplies[] = {
    {
        .name = "battery",
        .type = POWER_SUPPLY_TYPE_BATTERY,
        .properties = zeus_battery_props,
        .num_properties = ARRAY_SIZE(zeus_battery_props),
        .get_property = zeus_battery_get_property,
    },
    {
        .name = "usb",
        .type = POWER_SUPPLY_TYPE_USB,
        .supplied_to = supply_list,
        .num_supplicants = ARRAY_SIZE(supply_list),
        .properties = zeus_power_properties,
        .num_properties = ARRAY_SIZE(zeus_power_properties),
        .get_property = zeus_power_get_property,
    },
    {
        .name = "ac",
        .type = POWER_SUPPLY_TYPE_MAINS,
        .supplied_to = supply_list,
        .num_supplicants = ARRAY_SIZE(supply_list),
        .properties = zeus_power_properties,
        .num_properties = ARRAY_SIZE(zeus_power_properties),
        .get_property = zeus_power_get_property,
    },
};

/// +++ FIH_ADQ +++ , MichaelKao 2009.06.08
///add for low battery LED blinking in suspend mode 
void Battery_power_supply_change(void)
{
	power_supply_changed(&zeus_power_supplies[CHARGER_BATTERY]);
}
EXPORT_SYMBOL(Battery_power_supply_change);
/// --- FIH_ADQ ---

static int zeus_battery_get_property(struct power_supply *psy,
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
			// "Unknown", "Charging", "Discharging", "Not charging", "Full"
			if (g_charging_state != CHARGER_STATE_LOW_POWER)
				val->intval = g_charging_state;
			else 
				val->intval = CHARGER_STATE_NOT_CHARGING;
			break;
		case POWER_SUPPLY_PROP_HEALTH:
			//GetBatteryInfo(BATT_AVCURRENT_INFO, &buf);
			// "Unknown", "Good", "Overheat", "Dead", "Over voltage", "Unspecified failure"
			val->intval = g_health;
			break;
		case POWER_SUPPLY_PROP_PRESENT:
			//GetBatteryInfo(BATT_CURRENT_INFO, &buf);
			val->intval = 1;
			break;
		case POWER_SUPPLY_PROP_TECHNOLOGY:
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
				printk(KERN_ERR "POWER_SUPPLY_PROP_CAPACITY : Get data failed\n");
				power_supply_changed(&zeus_power_supplies[CHARGER_BATTERY]);
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

				GetBatteryInfo(BATT_VOLTAGE_INFO, &batt_vol);

				if ((val->intval > 94) && (g_charging_state == CHARGER_STATE_CHARGING)){//full
					g_charging_state = CHARGER_STATE_FULL;
				}
				else if (g_charging_state == CHARGER_STATE_FULL) {	 		//charging
					if (val->intval < 95) {
						g_charging_state = CHARGER_STATE_CHARGING;
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
			printk(KERN_ERR "zeus_battery_get_property : psp(%d)\n", psp);
			ret = -EINVAL;
			break;
	}

	return ret;
}

#ifdef FLAG_BATTERY_POLLING
static struct timer_list polling_timer;
//static struct timer_list reset_chgen;


#define BATTERY_POLLING_TIMER  60000//300000 10000

static void polling_timer_func(unsigned long unused)
{
	power_supply_changed(&zeus_power_supplies[CHARGER_BATTERY]);
	mod_timer(&polling_timer,
			jiffies + msecs_to_jiffies(BATTERY_POLLING_TIMER));
}
#endif	// FLAG_BATTERY_POLLING

#ifdef FLAG_CHARGER_DETECT

/* FIH_ADQ, Kenny { */
static void gasgauge_param_reset(){
	int rc, batt_scalar, batt_vol=0, batt_cur=0;

	if(GetBatteryInfo(BATT_VOLTAGE_INFO, &batt_vol) < 0){
		printk(KERN_ERR "batt: Get voltage failed!\n");
	}

	if(GetBatteryInfo(BATT_CURRENT_INFO, &batt_cur) < 0){
		printk(KERN_ERR "batt: Get current failed!\n");
	}

	if(GetBatteryInfo(BATT_AS_REGISTER, &batt_scalar) >= 0){
		if(batt_scalar != 128){
			//reset age scalar
			if(SetBatteryInfo(BATT_AS_REGISTER, 128) >= 0){
				printk(KERN_INFO "batt: Change scalar from %d to 128\n", batt_scalar);
			}else{
				printk(KERN_ERR "batt: Fail to change scalar from %d to 128\n", batt_scalar);
			}
		}
	}else{
		printk(KERN_ERR "batt: Get age scalar failed!\n");
	}

	if((batt_vol >= 4100) && current_charger == CHARGER_AC){
		rc = gpio_request(CHR_1A, "CHR_1A");
		if (rc)	printk(KERN_ERR "CHR_1A setting failed!\n");
		rc = gpio_get_value(CHR_1A);
		if(rc == 1){
			gpio_set_value(CHR_1A,0);
			printk(KERN_INFO "batt: voltage > 4.1V and AC charging, set the charging current to 500mA!!\n");
		}
		gpio_free(CHR_1A);
	} else if (batt_vol >= 4200) {
		g_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	} 

	if(charging_tcount < 6){
		charging_tcount++;
	}

	if((batt_vol < 4100) && (batt_cur < 0) && (charging_tcount == 6)){
		printk(KERN_INFO "batt: Reset charger IC due to voltage < 4.1mV and current < 0!!\n");
		polling_reset_func();
		charging_tcount = 0;
	}
	if (batt_vol == 0 && g_health < POWER_SUPPLY_HEALTH_DEAD) {
		g_health = POWER_SUPPLY_HEALTH_DEAD;
	} else if (batt_vol > 0 && 
				batt_vol < 4200 && g_health != POWER_SUPPLY_HEALTH_OVERHEAT) {
		g_health = POWER_SUPPLY_HEALTH_GOOD;
	}

}
/* } FIH_ADQ, Kenny */

void temperature_detect()
{
	int rc, ret=0, buf;

	ret = GetBatteryInfo(BATT_TEMPERATURE_INFO, &buf);

	if( buf > 450 || buf < 0 ){
		rc = gpio_request(CHR_EN, "CHR_EN");
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
			if (g_health < POWER_SUPPLY_HEALTH_OVERHEAT)
				g_health = POWER_SUPPLY_HEALTH_OVERHEAT;
			printk(KERN_INFO "Temperature is too high!\r\n");	
		}
		gpio_free(CHR_EN);
	}else{
		rc = gpio_request(CHR_EN, "CHR_EN");
		if(rc){
			printk(KERN_INFO "CHG_EN_REQUEST_FAIL\r\n");	
		}
		rc = gpio_get_value(CHR_EN);
		if(rc == 1){
			gpio_set_value(CHR_EN,0);
		}
		gpio_free(CHR_EN);
		if (g_health == POWER_SUPPLY_HEALTH_OVERHEAT)
			g_health = POWER_SUPPLY_HEALTH_GOOD;
	}
}

static void polling_reset_func()
{
	int rc;
	if(g_charging_state == CHARGER_STATE_CHARGING || g_charging_state == CHARGER_STATE_FULL)
	{
		disable_irq(MSM_GPIO_TO_INT(39));
		rc = gpio_request(CHR_EN,"CHR_EN");
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
	power_supply_changed(&zeus_power_supplies[CHARGER_BATTERY]);
	return IRQ_HANDLED;
}
#endif	// FLAG_CHARGER_DETECT


static int goldfish_battery_probe(struct platform_device *pdev)
{
	int ret, i;

    /* init power supplier framework */
    for (i = 0; i < ARRAY_SIZE(zeus_power_supplies); i++) {
        ret = power_supply_register(&pdev->dev, &zeus_power_supplies[i]);
        if (ret) {
            printk(KERN_ERR "Failed to register power supply (%d)\n", ret);
		    return ret;
		}
    }

#ifdef FLAG_BATTERY_POLLING
	setup_timer(&polling_timer, polling_timer_func, 0);
	mod_timer(&polling_timer,
			jiffies + msecs_to_jiffies(BATTERY_POLLING_TIMER));

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

	return 0;

}

static int goldfish_battery_remove(struct platform_device *pdev)
{
	int i;

#ifdef FLAG_CHARGER_DETECT
	free_irq(MSM_GPIO_TO_INT(GPIO_CHR_DET), NULL);
	gpio_free(GPIO_CHR_DET);
#endif	// FLAG_CHARGER_DETECT

#ifdef FLAG_BATTERY_POLLING
	del_timer_sync(&polling_timer);
#endif	// FLAG_BATTERY_POLLING

    for (i = 0; i < ARRAY_SIZE(zeus_power_supplies); i++) {
        power_supply_unregister(&zeus_power_supplies[i]);
    }

	return 0;
}

#ifdef CONFIG_USB_GADGET_MSM_72K 
void zeus_update_usb_status(enum chg_type chgtype) {

	/* Prepare enabler/USB/1A GPIOs */
	/* This is somehow wrong. Leave it off for now  */
	int rc = gpio_request(CHR_EN, "CHR_EN");
	if (rc) printk(KERN_ERR "%s: CHR_EN setting failed! rc = %d\n", __func__, rc);
	rc = gpio_request(USBSET, "USBSET");
	if (rc) printk(KERN_ERR "%s: USBSET setting failed! rc = %d\n", __func__, rc);
	rc = gpio_request(CHR_1A, "CHR_1A");
	if (rc) printk(KERN_ERR "%s: CHR_1A setting failed! rc = %d\n", __func__, rc);

	switch (chgtype) {
		case USB_CHG_TYPE__WALLCHARGER:
			/* Turn on charger IC */
			/* Set the charging current to 1A */
			gpio_set_value(CHR_EN,0);
			gpio_set_value(CHR_1A,1);
			gpio_set_value(USBSET,1);
			current_charger = CHARGER_AC;
			break;
		case USB_CHG_TYPE__CARKIT:
			/* Turn on charger IC */
			/* Set the charging current to 100mA */
			gpio_set_value(CHR_EN,0);
			gpio_set_value(CHR_1A,0);
			gpio_set_value(USBSET,1);
			current_charger = CHARGER_AC;
			break;
		case USB_CHG_TYPE__SDP:
			/* Turn on charger IC */
			/* Set the charging current to 100mA */
			gpio_set_value(CHR_EN,0);
			gpio_set_value(CHR_1A,0);
			gpio_set_value(USBSET,1);
			current_charger = CHARGER_USB;
			break;
		case USB_CHG_TYPE__INVALID:
		default:
			/* Turn off charger IC */
			gpio_set_value(CHR_EN,1);
			gpio_set_value(USBSET,0);
			current_charger = CHARGER_BATTERY;
			break;
	}
	gpio_free(CHR_EN);
	gpio_free(USBSET);
	gpio_free(CHR_1A);

	power_supply_changed(&zeus_power_supplies[current_charger]);
}
EXPORT_SYMBOL(zeus_update_usb_status);
#endif

static struct platform_driver goldfish_battery_device = {
	.probe		= goldfish_battery_probe,
	.remove		= goldfish_battery_remove,
	.driver = {
		.name = "goldfish-battery"
	}
};

static int __init goldfish_battery_init(void)
{
	/*+++FIH_ADQ+++*/
	gpio_tlmm_config(GPIO_CFG(33, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
	gpio_tlmm_config(GPIO_CFG(97, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
	gpio_tlmm_config(GPIO_CFG(57, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
	/*+++FIH_ADQ+++*/

	return platform_driver_register(&goldfish_battery_device);
}

static void __exit goldfish_battery_exit(void)
{
	platform_driver_unregister(&goldfish_battery_device);
}

module_init(goldfish_battery_init);
module_exit(goldfish_battery_exit);

MODULE_AUTHOR("Several");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Battery driver the FIH Zeus Battery");
