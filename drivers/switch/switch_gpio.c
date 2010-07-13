/*
 *  drivers/switch/switch_gpio.c
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
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/switch.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
/* FIH_ADQ, AudiPCHuang, 2009/05/06, { */
/* ZEUS_ANDROID_CR, For resolving plugged/unplugged stereo headset issue */
///+FIH_ADQ
#include <linux/delay.h>
///-FIH_ADQ
/* } FIH_ADQ, AudiPCHuang, 2009/05/06 */

/* FIH_ADQ, AudiPCHuang, 2009/07/17, { */
/* ADQ.B-2047, For resolving plugged/unplugged stereo headset issue */
#include <linux/completion.h>
#include <linux/wakelock.h>
/* } FIH_ADQ, AudiPCHuang, 2009/07/17 */

struct gpio_switch_data {
	struct switch_dev sdev;
	unsigned gpio;
	const char *name_on;
	const char *name_off;
	const char *state_on;
	const char *state_off;
	int irq;
	struct work_struct work;

/* FIH_ADQ, AudiPCHuang, 2009/05/06, { */
/* ZEUS_ANDROID_CR, For resolving plugged/unplugged stereo headset issue */	
///+FIH_ADQ
	bool bHeadsetInserted;
	struct wake_lock headset_wake_lock;
///-FIH_ADQ
/* } FIH_ADQ, AudiPCHuang, 2009/05/06 */
};

/* FIH_ADQ, AudiPCHuang, 2009/05/06, { */
/* ZEUS_ANDROID_CR, For resolving plugged/unplugged stereo headset issue */	
///+FIH_ADQ
struct gpio_switch_data *switch_data;

extern int msm_mic_en_proc(bool disable_enable);
extern bool qi2ckybd_get_hook_switch_value(void);
extern bool qi2ckybd_get_hook_switch_irq_status(void);
extern struct completion* get_hook_sw_release_completion(void);
extern struct completion* get_hook_sw_request_irq_completion(void);
extern struct completion* get_hook_sw_release_irq_completion(void);

bool gpio_switch_headset_insert(void) 
{
	return (bool)gpio_get_value(switch_data->gpio);
}

EXPORT_SYMBOL(gpio_switch_headset_insert);
///-FIH_ADQ
/* } FIH_ADQ, AudiPCHuang, 2009/05/06 */

static void gpio_switch_work(struct work_struct *work)
{
	int state;
	struct gpio_switch_data	*data =
		container_of(work, struct gpio_switch_data, work);

	wake_lock(&switch_data->headset_wake_lock);

	state = gpio_get_value(data->gpio);
	printk(KERN_INFO "gpio_switch_work: state = %d\n", state);
	switch_set_state(&data->sdev, state);
	
	msm_mic_en_proc((bool)state);
	
	data->bHeadsetInserted = (bool)state;

/* FIH_ADQ, AudiPCHuang, 2009/07/17, { */
/* ADQ.B-2047, For resolving plugged/unplugged stereo headset issue */
	if (state == 0) {
		complete(get_hook_sw_release_completion());
		if (qi2ckybd_get_hook_switch_irq_status())
			complete(get_hook_sw_release_irq_completion());
	} else if (state == 1) {
		if (!qi2ckybd_get_hook_switch_irq_status())
			complete(get_hook_sw_request_irq_completion());
	}
/* } FIH_ADQ, AudiPCHuang, 2009/07/17 */

	wake_unlock(&switch_data->headset_wake_lock);
}

static irqreturn_t gpio_irq_handler(int irq, void *dev_id)
{
	struct gpio_switch_data *switch_data =
	    (struct gpio_switch_data *)dev_id;

	wake_lock(&switch_data->headset_wake_lock);
	schedule_work(&switch_data->work);
	wake_unlock(&switch_data->headset_wake_lock);
	return IRQ_HANDLED;
}

static ssize_t switch_gpio_print_state(struct switch_dev *sdev, char *buf)
{
	struct gpio_switch_data	*switch_data =
		container_of(sdev, struct gpio_switch_data, sdev);
	const char *state;
	if (switch_get_state(sdev))
		state = switch_data->state_on;
	else
		state = switch_data->state_off;

	if (state)
		return sprintf(buf, "%s\n", state);
	return -1;
}

static int gpio_switch_probe(struct platform_device *pdev)
{
	struct gpio_switch_platform_data *pdata = pdev->dev.platform_data;
/* FIH_ADQ, AudiPCHuang, 2009/05/06, { */
/* ZEUS_ANDROID_CR, For resolving plugged/unplugged stereo headset issue */
///+FIH_ADQ
	//struct gpio_switch_data *switch_data;
///-FIH_ADQ
/* } FIH_ADQ, AudiPCHuang, 2009/05/06 */
	int ret = 0;
	int r; //FIH_ADQ KarenLiao @20090710: [ADQ.B-3374]: [Accessory][Audio path]Dwon link not switch when in call processing then plug in Headset.

	if (!pdata)
		return -EBUSY;

	switch_data = kzalloc(sizeof(struct gpio_switch_data), GFP_KERNEL);
	if (!switch_data)
		return -ENOMEM;

	switch_data->sdev.name = pdata->name;
	switch_data->gpio = pdata->gpio;
	switch_data->name_on = pdata->name_on;
	switch_data->name_off = pdata->name_off;
	switch_data->state_on = pdata->state_on;
	switch_data->state_off = pdata->state_off;
	switch_data->sdev.print_state = switch_gpio_print_state;
	wake_lock_init(&switch_data->headset_wake_lock, WAKE_LOCK_SUSPEND, "headset_work");

    ret = switch_dev_register(&switch_data->sdev);
	if (ret < 0)
		goto err_switch_dev_register;

	ret = gpio_request(switch_data->gpio, pdev->name);
	if (ret < 0)
		goto err_request_gpio;

	ret = gpio_direction_input(switch_data->gpio);
	if (ret < 0)
		goto err_set_gpio_input;

	INIT_WORK(&switch_data->work, gpio_switch_work);

	switch_data->irq = gpio_to_irq(switch_data->gpio);
	if (switch_data->irq < 0) {
		ret = switch_data->irq;
		goto err_detect_irq_num_failed;
	}

	// +++ FIH_ADQ +++ , modified by henry.wang
	//ret = request_irq(switch_data->irq, gpio_irq_handler,
	//		  IRQF_TRIGGER_LOW, pdev->name, switch_data);
	ret = request_irq(switch_data->irq, gpio_irq_handler,
			IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, pdev->name, switch_data);
	// --- FIH_ADQ ---
	
	if (ret < 0)
		goto err_request_irq;

   //+++ FIH_ADQ KarenLiao @20090710: [ADQ.B-3374]: [Accessory][Audio path]Dwon link not switch when in call processing then plug in Headset.
	r = enable_irq_wake(MSM_GPIO_TO_INT(switch_data->gpio));

	if (r < 0)
		printk(KERN_ERR "gpio_switch_prob: "
			"enable_irq_wake failed for HS detect\n");
			
   //--- FIH_ADQ KarenLiao @20090710: [ADQ.B-3374]: [Accessory][Audio path]Dwon link not switch when in call processing then plug in Headset.	

	/* Perform initial detection */
	gpio_switch_work(&switch_data->work);

	return 0;

err_request_irq:
err_detect_irq_num_failed:
err_set_gpio_input:
	gpio_free(switch_data->gpio);
err_request_gpio:
    switch_dev_unregister(&switch_data->sdev);
err_switch_dev_register:
	kfree(switch_data);

	return ret;
}

static int __devexit gpio_switch_remove(struct platform_device *pdev)
{
	struct gpio_switch_data *switch_data = platform_get_drvdata(pdev);

	cancel_work_sync(&switch_data->work);
	gpio_free(switch_data->gpio);
    switch_dev_unregister(&switch_data->sdev);
	wake_lock_destroy(&switch_data->headset_wake_lock);

	kfree(switch_data);

	return 0;
}

static struct platform_driver gpio_switch_driver = {
	.probe		= gpio_switch_probe,
	.remove		= __devexit_p(gpio_switch_remove),
	.driver		= {
	// +++ FIH_ADQ +++ , modified by henry.wang
		.name	= "switch_gpio",
	// --- FIH_ADQ ---
		.owner	= THIS_MODULE,
	},
};

static int __init gpio_switch_init(void)
{
	return platform_driver_register(&gpio_switch_driver);
}

static void __exit gpio_switch_exit(void)
{
	platform_driver_unregister(&gpio_switch_driver);
}

module_init(gpio_switch_init);
module_exit(gpio_switch_exit);

MODULE_AUTHOR("Mike Lockwood <lockwood@android.com>");
MODULE_DESCRIPTION("GPIO Switch driver");
MODULE_LICENSE("GPL");
