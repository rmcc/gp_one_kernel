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

#include <linux/module.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gasgauge_bridge.h>
#include <asm/io.h>

#include "ow2428.c"

static int __devinit gasgauge_bridge_probe(struct i2c_client *client,
											const struct i2c_device_id *id)
{
	ZeusDS2482_init(client);
	return 0;
}

int gg_dev_suspended = 0;

int GetBatteryInfo(enum _battery_info_type_ info, int * data)
{
	short s;
	int i = 0;
	uint8_t c;

	/* The mutex lock isn't good enough: battery calls accumulate, and on
     * resume they take a while to flush. With enough pending replies, the
     * resume process ends up crashing the phone due to timeouts 
     * zeus_battery also looks directly at dev_suspended, so calls shouldn't 
     * even get this far */
	if (gg_dev_suspended == 1) {
		*data = 1;
		return 0;
	}

	if (!OpenBAT()) {
		switch (info) {
		case BATT_CAPACITY_INFO:
			BATReadMemory8(DS2780_RARC, &c);
			// The register value is in 4.88mV units and the 5 lowest bits are not used
			*data = c;
			break;
		case BATT_VOLTAGE_INFO:
			BATReadMemory16(DS2780_VRH, &s);
			// The register value is in 4.886mV units and the 5 lowest bits are not used
			*data = ((s >> 5) * 4886) / 1000;
			break;
		case BATT_TEMPERATURE_INFO:
			BATReadMemory16(DS2780_TRH, &s);
			// The register value is in 0.125C units and the 5 lowest bits are not used
			// We return in 0.1C units
			*data = ((s >> 5) * 125) / 100;
			break;
		case BATT_CURRENT_INFO:
			BATReadMemory16(DS2780_CRH, &s);
			/* Current: Unit= 1.5625uV x Rsnsp(67)=104.68 */
			*data = ((s * 15625) / 10000) * 67;
			break;
		case BATT_AVCURRENT_INFO:
			BATReadMemory16(DS2780_CRAH, &s);
			/* Current: Unit= 1.5625uV x Rsnsp(67)=104.68 */
			*data = ((s * 15625) / 10000) * 67;
			break;
		case BATT_STATUS_REGISTER:
			BATReadMemory8(DS2780_STATUS, &c);
			*data = c;
			break;
		/* FIH_ADQ, Kenny { */
		case BATT_ACR_REGISTER:
			BATReadMemory16(DS2780_ACRH, &s);
			//unit: 312.5uAh
			*data = s;
			break;
		case BATT_AS_REGISTER:
			BATReadMemory8(DS2780_AS, &c);
			*data = c;
			//unit: 0.78%
			break;
		case BATT_FULL_REGISTER:
			BATReadMemory16(DS2780_FULLH, &s);
			*data = s;
			//unit: 61ppm(divide by 2)
			break;
		case BATT_FULL40_REGISTER:
			BATReadMemory16(DS2780_FULL40H, &s);
			//unit: 6.25uV
			*data = s;
			break;
		/* } FIH_ADQ, Kenny */
		default:
			i = -1;
		}
		CloseBAT();
		return i;
	}
	return -2;
}

/* FIH_ADQ, Kenny { */
int SetBatteryInfo(enum _battery_info_type_ info, int  data)
{
	short s;
	int i = 0;
	uint8_t c;

	if (!OpenBAT()) {
		switch (info) {
		case BATT_ACR_REGISTER:
		    s = data;
			BATWriteMemory16(DS2780_ACRH, s);
			break;
		case BATT_AS_REGISTER:
		    c = data;
			BATWriteMemory8(DS2780_AS, c);
			break;
		default:
			i = -1;
		}
		CloseBAT();
		return i;
	}
	return -2;
}
/* } FIH_ADQ, Kenny */

EXPORT_SYMBOL_GPL(GetBatteryInfo);

static int gasgauge_bridge_remove(struct i2c_client *client)
{
	printk(KERN_INFO "<ubh> gasgauge_bridge_remove\n");
	return 0;
}

static int gasgauge_bridge_suspend(struct device *dev)
{
	printk(KERN_INFO "<ubh> gasgauge_bridge_suspend\n");
	gg_dev_suspended = 1;
	mutex_lock(&g_ow2428_suspend_lock);
	return 0;
}

#include <mach/rpc_hsusb.h>
extern void zeus_update_usb_status(enum chg_type chgtype);
static int gasgauge_bridge_resume(struct device *dev)
{
	mutex_unlock(&g_ow2428_suspend_lock);
    //zeus_update_usb_status(USB_CHG_TYPE__INVALID);
	gg_dev_suspended = 0;
	printk(KERN_INFO "<ubh> gasgauge_bridge_resume\n");
	return 0;
}

static const struct i2c_device_id gasgauge_bridge_idtable[] = {
       { GASGAUGE_BRIDGE_NAME, 0 },
       { }
};

static struct dev_pm_ops gg_pm_ops = {
	.suspend	= gasgauge_bridge_suspend,
	.resume		= gasgauge_bridge_resume,
};

static struct i2c_driver gasgauge_bridge_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name	= GASGAUGE_BRIDGE_NAME,
		.pm = &gg_pm_ops,
	},
	.probe		= gasgauge_bridge_probe,
	.remove		= gasgauge_bridge_remove,
	.id_table = gasgauge_bridge_idtable,
};

static int __devinit gasgauge_bridge_init(void)
{
	mutex_init(&g_ow2428_suspend_lock);
	return i2c_add_driver(&gasgauge_bridge_driver);
}

static void __exit gasgauge_bridge_exit(void)
{
	i2c_del_driver(&gasgauge_bridge_driver);
}

module_init(gasgauge_bridge_init);
module_exit(gasgauge_bridge_exit);

MODULE_DESCRIPTION("1-wire bridge Driver");
MODULE_LICENSE("GPL");
