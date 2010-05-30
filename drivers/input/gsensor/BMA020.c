#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <mach/gpio.h>
#include <mach/msm_smd.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <asm/uaccess.h>

#include "BMA020.h"

#include <linux/miscdevice.h>
#include <linux/akm8976.h>

#define DRIVER_VERSION "v1.0"
#define PORTRAIT            0x01
#define REVERSE_PORTRAIT    0x02
#define LANDSCAPE           0x04
#define REVERSE_LANDSCAPE   0x08
#define FACE_UP             0x10
#define FACE_DOWN           0x20




#define gsensor_name  "bma020"
static struct i2c_client *this_client;
//static struct input_dev *bma020_input_dev = NULL;

MODULE_VERSION(DRIVER_VERSION);
MODULE_AUTHOR("Chih-Chia Hung, FIH");
MODULE_DESCRIPTION("BMA020 Sensor Controller");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:bma020");

static int gsensor_bma020_remove(struct i2c_client *client);
static int gsensor_bma020_command(struct i2c_client *client, unsigned int cmd, void *arg);
static int gsensor_bma020_suspend(struct i2c_client *client, pm_message_t mesg);
static int gsensor_bma020_resume(struct i2c_client *client);
static int gsensor_bma020_probe(struct i2c_client *client);

// BMA020 file operation 
static int bma020_dev_open( struct inode * inode, struct file * file );
static ssize_t bma020_dev_read( struct file * file, char __user * buffer, size_t size, loff_t * f_pos );
static int bma020_dev_ioctl( struct inode * inode, struct file * filp, unsigned int cmd, unsigned long arg );
static int bma020_dev_release( struct inode * inode, struct file * filp );

static int bma020_aot_open(struct inode *inode, struct file *file);
static int bma020_aot_release(struct inode *inode, struct file *file);
static int bma020_aot_ioctl(struct inode *inode, struct file *file,
	      unsigned int cmd, unsigned long arg);

static int bma020_pffd_open(struct inode *inode, struct file *file);
static int bma020_pffd_release(struct inode *inode, struct file *file);
static int bma020_pffd_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);

//function
static int func_bma020_init(void);

#define GPIO_BMA020_INTR 38
long GRAVITY_EARTH =  9.80665;

static struct work_struct bma020_irqwork;
static struct work_struct bma020_accwork;
//static struct work_struct bma020_volctrl;
//static struct work_struct bma020_hallsensor;

int hall_sensor_pin_val = 0;
int mflag = 0;
int aflag = 0;
int mvflag = 0;
int delay = 0;

static DECLARE_WAIT_QUEUE_HEAD(data_ready_wq);
static DECLARE_WAIT_QUEUE_HEAD(open_wq);
static atomic_t open_count;
static atomic_t open_flag;
static atomic_t reserve_open_flag;

static const struct i2c_device_id i2cgsensor_idtable[] = {
       { gsensor_name, 0 },
       { }
};

MODULE_DEVICE_TABLE(i2c, i2cgsensor_idtable);

static struct i2c_driver gensor_bma020_driver = {
	.probe		= gsensor_bma020_probe,
	.remove		= gsensor_bma020_remove,
	.command	= gsensor_bma020_command,
#ifndef CONFIG_ANDROID_POWER
	.suspend	= gsensor_bma020_suspend,
	.resume		= gsensor_bma020_resume,
#endif
	.id_table = i2cgsensor_idtable,
	.driver = {
		.name = "bma020",
		.owner = THIS_MODULE,
	},
};

int g_gsensor_power_mode;
int g_client_count;
static spinlock_t g_lock;

/* Function prototypes */

//static const struct input_device_id gsensor_dev_ids[] = {
//	{ .driver_info = 1 },	/* Matches all devices */
//	{ },			/* Terminating zero entry */
//};

/*
static struct input_handler gensor_BMA020_handler = {
	.event		= gensor_BMA020_event,
	.connect	= gensor_BMA020_connect,
	.disconnect	= gensor_BMA020_disconnect,
	.fops		= &evdev_fops,
	.name		= "gsensor",
	.id_table	= gsensor_dev_ids,
};
*/

#define config_ctrl_reg(name,address) \
static ssize_t name##_show(struct device *dev, struct device_attribute *attr, \
			   char *buf) \
{ \
	struct i2c_client *client = to_i2c_client(dev); \
        return sprintf(buf, "%u\n", i2c_smbus_read_byte_data(client,address)); \
} \
static ssize_t name##_store(struct device *dev, struct device_attribute *attr, \
			    const char *buf,size_t count) \
{ \
	struct i2c_client *client = to_i2c_client(dev); \
	unsigned long val = simple_strtoul(buf, NULL, 10); \
	if (val > 0xff) \
		return -EINVAL; \
	i2c_smbus_write_byte_data(client,address, val); \
        return count; \
} \
static DEVICE_ATTR(name, S_IWUSR | S_IRUGO, name##_show, name##_store)


static struct file_operations bma020_dev_fops = {
	.open = bma020_dev_open,
	.read = bma020_dev_read,
	.ioctl = bma020_dev_ioctl,
	.release = bma020_dev_release,
};

static struct file_operations bma020_aot_fops = {
	.owner = THIS_MODULE,
	.open = bma020_aot_open,
	.release = bma020_aot_release,
	.ioctl = bma020_aot_ioctl,
};

static struct miscdevice bma020_cdev = {
	MISC_DYNAMIC_MINOR,
	"bma020",
	&bma020_dev_fops
};

static struct miscdevice bma020_aot_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "bma020_aot",
	.fops = &bma020_aot_fops,
};

static struct bma020_data {
	struct input_dev *input_dev;
	struct work_struct work;
//#ifdef CONFIG_ANDROID_POWER
//	android_early_suspend early_suspend;
//#endif
};


static int BMAI2C_RxData(char *rxData, int length)
{
	struct i2c_msg msgs[] = {
		{
		 .addr = this_client->addr,		// 0x38
		 .flags = 0,				// w:0	
		 .len = 1,				// len of register adress 
		 .buf = rxData,				// ex. 15h ==> wakeup
		 },
		{
		 .addr = this_client->addr,		// 0x38
		 .flags = I2C_M_RD,			// r:1
		 .len = length,				// len of data
		 .buf = rxData,				// data
		 },
	};

	if (i2c_transfer(this_client->adapter, msgs, 2) < 0) {
		printk(KERN_ERR "BMAI2C_RxData: transfer error\n");
		return -EIO;
	} else
		return 0;
}

static int BMAI2C_TxData(char *txData, int length)
{

	struct i2c_msg msg[] = {
		{
		 .addr = this_client->addr,
		 .flags = 0,
		 .len = length,
		 .buf = txData,
		 },
	};

	if (i2c_transfer(this_client->adapter, msg, 1) < 0) {
		printk(KERN_ERR "BMAI2C_TxData: transfer error\n");
		return -EIO;
	} else
		return 0;
}


static void bma020_acc_work_func(struct work_struct *work)
{
#if 0
	char buffer[6];
	bma020acc_t acc;

	struct bma020_data *bma020 = i2c_get_clientdata(this_client);
	short y_z_change_tmp = 0;
	
	while(aflag)
	{
		y_z_change_tmp = 0;
		memset(buffer, 0, 6);
		buffer[0] = 0x02;
		BMAI2C_RxData(buffer, 6);		
		acc.x = BMA020_GET_BITSLICE(buffer[0],ACC_X_LSB) | BMA020_GET_BITSLICE(buffer[1],ACC_X_MSB)<<ACC_X_LSB__LEN;
		acc.x = acc.x << (sizeof(short)*8-(ACC_X_LSB__LEN+ACC_X_MSB__LEN));
		acc.x = acc.x >> (sizeof(short)*8-(ACC_X_LSB__LEN+ACC_X_MSB__LEN));

		acc.y = BMA020_GET_BITSLICE(buffer[2],ACC_Y_LSB) | BMA020_GET_BITSLICE(buffer[3],ACC_Y_MSB)<<ACC_Y_LSB__LEN;
		acc.y = acc.y << (sizeof(short)*8-(ACC_Y_LSB__LEN + ACC_Y_MSB__LEN));
		acc.y = acc.y >> (sizeof(short)*8-(ACC_Y_LSB__LEN + ACC_Y_MSB__LEN));
	
		acc.z = BMA020_GET_BITSLICE(buffer[4],ACC_Z_LSB) | BMA020_GET_BITSLICE(buffer[5],ACC_Z_MSB)<<ACC_Z_LSB__LEN;
		acc.z = acc.z << (sizeof(short)*8-(ACC_Z_LSB__LEN+ACC_Z_MSB__LEN));
		acc.z = acc.z >> (sizeof(short)*8-(ACC_Z_LSB__LEN+ACC_Z_MSB__LEN));
		
		y_z_change_tmp = acc.y;
		acc.y = acc.z;
		acc.z = y_z_change_tmp;
	
		input_report_abs(bma020->input_dev, ABS_X, acc.x);
		input_report_abs(bma020->input_dev, ABS_Y, acc.y);
		input_report_abs(bma020->input_dev, ABS_Z, acc.z);
		//printk(KERN_INFO "x:%d, y:%d, z:%d\n", acc.x, acc.y, acc.z);

		input_sync(bma020->input_dev);
		msleep(150);
	}
#endif
}

static void bma020_work_func(struct work_struct *work)
{
#if 0
	char buffer[6];
	bma020acc_t acc;
	struct bma020_data *bma020 = i2c_get_clientdata(this_client);
	short y_z_change_tmp = 0;
	//printk(KERN_INFO "bma020_work_func+\n");
	

	memset(buffer, 0, 6);
	buffer[0] = 0x02;
	BMAI2C_RxData(buffer, 6);

	if(aflag && mflag)
	{
		//printk (KERN_INFO "bma020 buffer[0]:0x%x, buffer[1]:0x%x, buffer[2]:0x%x, buffer[3]:0x%x buffer[4]:0x%x buffer[5]:0x%x\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
		
		acc.x = BMA020_GET_BITSLICE(buffer[0],ACC_X_LSB) | BMA020_GET_BITSLICE(buffer[1],ACC_X_MSB)<<ACC_X_LSB__LEN;
		acc.x = acc.x << (sizeof(short)*8-(ACC_X_LSB__LEN+ACC_X_MSB__LEN));
		acc.x = acc.x >> (sizeof(short)*8-(ACC_X_LSB__LEN+ACC_X_MSB__LEN));

		acc.y = BMA020_GET_BITSLICE(buffer[2],ACC_Y_LSB) | BMA020_GET_BITSLICE(buffer[3],ACC_Y_MSB)<<ACC_Y_LSB__LEN;
		acc.y = acc.y << (sizeof(short)*8-(ACC_Y_LSB__LEN + ACC_Y_MSB__LEN));
		acc.y = acc.y >> (sizeof(short)*8-(ACC_Y_LSB__LEN + ACC_Y_MSB__LEN));
	
	
		acc.z = BMA020_GET_BITSLICE(buffer[4],ACC_Z_LSB) | BMA020_GET_BITSLICE(buffer[5],ACC_Z_MSB)<<ACC_Z_LSB__LEN;
		acc.z = acc.z << (sizeof(short)*8-(ACC_Z_LSB__LEN+ACC_Z_MSB__LEN));
		acc.z = acc.z >> (sizeof(short)*8-(ACC_Z_LSB__LEN+ACC_Z_MSB__LEN));

		y_z_change_tmp = acc.y;
		acc.y = acc.z;
		acc.z = y_z_change_tmp;

		input_report_abs(bma020->input_dev, ABS_X, acc.x);
		input_report_abs(bma020->input_dev, ABS_Y, acc.y);
		input_report_abs(bma020->input_dev, ABS_Z, acc.z);
		
		input_report_abs(bma020->input_dev, ABS_RX, acc.x);
		input_report_abs(bma020->input_dev, ABS_RY, acc.y);
		input_report_abs(bma020->input_dev, ABS_RZ, acc.z);
	}
	else if(aflag)
	{

		acc.x = BMA020_GET_BITSLICE(buffer[0],ACC_X_LSB) | BMA020_GET_BITSLICE(buffer[1],ACC_X_MSB)<<ACC_X_LSB__LEN;
		acc.x = acc.x << (sizeof(short)*8-(ACC_X_LSB__LEN+ACC_X_MSB__LEN));
		acc.x = acc.x >> (sizeof(short)*8-(ACC_X_LSB__LEN+ACC_X_MSB__LEN));

		acc.y = BMA020_GET_BITSLICE(buffer[2],ACC_Y_LSB) | BMA020_GET_BITSLICE(buffer[3],ACC_Y_MSB)<<ACC_Y_LSB__LEN;
		acc.y = acc.y << (sizeof(short)*8-(ACC_Y_LSB__LEN + ACC_Y_MSB__LEN));
		acc.y = acc.y >> (sizeof(short)*8-(ACC_Y_LSB__LEN + ACC_Y_MSB__LEN));
	
		acc.z = BMA020_GET_BITSLICE(buffer[4],ACC_Z_LSB) | BMA020_GET_BITSLICE(buffer[5],ACC_Z_MSB)<<ACC_Z_LSB__LEN;
		acc.z = acc.z << (sizeof(short)*8-(ACC_Z_LSB__LEN+ACC_Z_MSB__LEN));
		acc.z = acc.z >> (sizeof(short)*8-(ACC_Z_LSB__LEN+ACC_Z_MSB__LEN));
		
		y_z_change_tmp = acc.y;
		acc.y = acc.z;
		acc.z = y_z_change_tmp;
	
		input_report_abs(bma020->input_dev, ABS_X, acc.x);
		input_report_abs(bma020->input_dev, ABS_Y, acc.y);
		input_report_abs(bma020->input_dev, ABS_Z, acc.z);
		//printk(KERN_INFO "x:%d, y:%d, z:%d\n", acc.x, acc.y, acc.z);
	}

	
	//test code, if unnecessary please remove them +
       {
		y_z_change_tmp = 0;
		memset(buffer, 0, 6);
		buffer[0] = 0x02;
		BMAI2C_RxData(buffer, 6);		
		acc.x = BMA020_GET_BITSLICE(buffer[0],ACC_X_LSB) | BMA020_GET_BITSLICE(buffer[1],ACC_X_MSB)<<ACC_X_LSB__LEN;
		acc.x = acc.x << (sizeof(short)*8-(ACC_X_LSB__LEN+ACC_X_MSB__LEN));
		acc.x = acc.x >> (sizeof(short)*8-(ACC_X_LSB__LEN+ACC_X_MSB__LEN));

		acc.y = BMA020_GET_BITSLICE(buffer[2],ACC_Y_LSB) | BMA020_GET_BITSLICE(buffer[3],ACC_Y_MSB)<<ACC_Y_LSB__LEN;
		acc.y = acc.y << (sizeof(short)*8-(ACC_Y_LSB__LEN + ACC_Y_MSB__LEN));
		acc.y = acc.y >> (sizeof(short)*8-(ACC_Y_LSB__LEN + ACC_Y_MSB__LEN));
	
		acc.z = BMA020_GET_BITSLICE(buffer[4],ACC_Z_LSB) | BMA020_GET_BITSLICE(buffer[5],ACC_Z_MSB)<<ACC_Z_LSB__LEN;
		acc.z = acc.z << (sizeof(short)*8-(ACC_Z_LSB__LEN+ACC_Z_MSB__LEN));
		acc.z = acc.z >> (sizeof(short)*8-(ACC_Z_LSB__LEN+ACC_Z_MSB__LEN));
		
		y_z_change_tmp = acc.y;
		acc.y = acc.z;
		acc.z = y_z_change_tmp;
	
		input_report_abs(bma020->input_dev, ABS_X, acc.x);
		input_report_abs(bma020->input_dev, ABS_Y, acc.y);
		input_report_abs(bma020->input_dev, ABS_Z, acc.z);
		printk(KERN_INFO "x:%d, y:%d, z:%d\n", acc.x, acc.y, acc.z);

       }
       //test code, if unnecessary please remove them -

	input_sync(bma020->input_dev);

	enable_irq(this_client->irq);

#endif
}


#define BMA020_GET_BITSLICE(regvar, bitname)\
			(regvar & bitname##__MSK) >> bitname##__POS


#define BMA020_SET_BITSLICE(regvar, bitname, val)\
		  (regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK)  

static irqreturn_t bma020_irqhandler(int irq, void *dev_id)
{
	//printk(KERN_INFO "bma020_irqhandler+\n");
	disable_irq(this_client->irq);
	schedule_work(&bma020_irqwork);

	return IRQ_HANDLED;
}


static int gsensor_bma020_remove(struct i2c_client *client)
{
	struct bma020_data *bma020 = i2c_get_clientdata(client);
	printk(KERN_INFO "[BMA020.c] gensor_BMA020_remove +\n");
	free_irq(client->irq, bma020);
	input_unregister_device(bma020->input_dev);
	i2c_detach_client(client);
	kfree(bma020);
	return 0;
}

static int
gsensor_bma020_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
	printk(KERN_INFO "[BMA020.c] gensor_BMA020_command\n");
	return 0;
}

static int gsensor_bma020_suspend(struct i2c_client *client, pm_message_t mesg)
{
	printk(KERN_INFO "[BMA020.c] gensor_BMA020_suspend+ \n");
    bma020_sleep();
	return 0;
}

static int gsensor_bma020_resume(struct i2c_client *client)
{
	int iRet = 0;
	printk(KERN_INFO "[BMA020.c] gensor_BMA020_resume+ \n");
    if (0 < g_client_count)
        bma020_wakeup();
    spin_lock(&g_lock);
    if (0 > g_client_count)
        g_client_count = 0;
    spin_unlock(&g_lock);
	return iRet;
}

static int gsensor_bma020_probe(struct i2c_client *client)
{
	struct bma020_data *bma020;
	int err;
	int rc;

	int bma_irqpin  = GPIO_BMA020_INTR;

 
	printk(KERN_INFO "[BMA020.c] gensor_BMA020_probe+\n");

	gpio_tlmm_config( GPIO_CFG( bma_irqpin, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA ), GPIO_ENABLE );

	rc = gpio_request(bma_irqpin, "gpio_bma_irqpin");
	if (rc) {
		printk(KERN_INFO "gpio_request failed on pin %d (rc=%d)\n",
			bma_irqpin, rc);
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		printk(KERN_INFO "[BMA020.c] exit_check_functionality_failed\n");
		goto exit_check_functionality_failed;
	}


	bma020 = kzalloc(sizeof(struct bma020_data), GFP_KERNEL);
	if (!bma020) {
		err = -ENOMEM;
		printk(KERN_INFO "[BMA020.c] exit_alloc_data_failed\n");
		goto exit_alloc_data_failed;
	}


	//register input driver
	//bma020_input_dev
	bma020->input_dev = input_allocate_device();
	if (!bma020->input_dev) {
		err = -ENOMEM;
		printk("[BMA020.c] init input device params\n");
		goto exit_input_dev_alloc_failed;
	}

	set_bit(EV_ABS, bma020->input_dev->evbit);
	bma020->input_dev->evbit[0] = BIT(EV_KEY) | BIT(EV_REP) | BIT(EV_SW) | BIT(EV_ABS);
	set_bit(SW_LID, bma020->input_dev->swbit);
	/* yaw */
	input_set_abs_params(bma020->input_dev, ABS_RX, 0, 360, 0, 0);
	/* pitch */
	input_set_abs_params(bma020->input_dev, ABS_RY, -180, 180, 0, 0);
	/* roll */
	input_set_abs_params(bma020->input_dev, ABS_RZ, -90, 90, 0, 0);
	/* x-axis acceleration */
	input_set_abs_params(bma020->input_dev, ABS_X, -1872, 1872, 0, 0);
	/* y-axis acceleration */
	input_set_abs_params(bma020->input_dev, ABS_Y, -1872, 1872, 0, 0);
	/* z-axis acceleration */
	input_set_abs_params(bma020->input_dev, ABS_Z, -1872, 1872, 0, 0);
	/* temparature */
	input_set_abs_params(bma020->input_dev, ABS_THROTTLE, -30, 85, 0, 0);
	/* status of magnetic sensor */
	input_set_abs_params(bma020->input_dev, ABS_RUDDER, -32768, 3, 0, 0);
	/* status of acceleration sensor */
	input_set_abs_params(bma020->input_dev, ABS_WHEEL, -32768, 3, 0, 0);
	/* step count */
	input_set_abs_params(bma020->input_dev, ABS_GAS, 0, 65535, 0, 0);
	/* x-axis of raw magnetic vector */
	input_set_abs_params(bma020->input_dev, ABS_HAT0X, -2048, 2032, 0, 0);
	/* y-axis of raw magnetic vector */
	input_set_abs_params(bma020->input_dev, ABS_HAT0Y, -2048, 2032, 0, 0);
	/* z-axis of raw magnetic vector */
	input_set_abs_params(bma020->input_dev, ABS_BRAKE, -2048, 2032, 0, 0);



	bma020->input_dev->name = "compass";

	err = input_register_device(bma020->input_dev);
	//end register input driver

	if (err) {
		printk(KERN_ERR
		       "bma020: Unable to register input device: %s\n",
		       bma020->input_dev->name);
		goto exit_input_register_device_failed;
	}

// Kenny temporarily remove +++
#if 0
	INIT_WORK(&bma020_irqwork, bma020_work_func);
	INIT_WORK(&bma020_accwork, bma020_acc_work_func);
#endif
// Kenny temporarily remove ---

	i2c_set_clientdata(client, bma020);

	//mutex_init(&sense_data_mutex);
	client->irq = MSM_GPIO_TO_INT(GPIO_BMA020_INTR);
	this_client = client;



// Kenny temporarily remove +++
#if 0
	printk(KERN_INFO "[BMA020.c] request_irq + \n");
	//register irq
	
	rc = request_irq(client->irq, &bma020_irqhandler,
			     IRQF_TRIGGER_RISING, gsensor_name, bma020);
	if (rc < 0) {
		printk(KERN_ERR
		       "Could not register for  %s interrupt "
		       "(rc = %d)\n", gsensor_name, rc);
		rc = -EIO;
	} else {
		printk(KERN_DEBUG "Register %s interrupt finished (rc = %d)\n", gsensor_name, rc);	
	}
	printk(KERN_INFO "[BMA020.c] request_irq - \n");
#endif
// Kenny temporarily remove ---


	err = misc_register(&bma020_cdev);

	if (err) {
		printk(KERN_INFO "gensor_bma020_probe: bma020_cdev failed.\n");
		/* return ret; */
		goto exit_misc_device_register_failed;
	}

	err = misc_register(&bma020_aot_device);
	if (err) {
		printk(KERN_ERR
		       "gensor_bma020_probe: bma020_aot_device register failed\n");
		goto exit_misc_device_register_failed;
	}


	func_bma020_init();

	printk(KERN_INFO "[BMA020.c] gensor_BMA020_probe-\n");

exit_check_functionality_failed:
exit_alloc_data_failed:
exit_input_dev_alloc_failed:
exit_input_register_device_failed:
exit_misc_device_register_failed:

	return 0;
}


static int func_bma020_init(void)
{
	char buffer[4];

	//chip id, alversion, ml version
	memset(buffer, 0, 4); 
	buffer[0] = 0x0;
	BMAI2C_RxData(buffer, 4);
	printk(KERN_INFO "bma020 chip id:0x%x\n", buffer[0] & 0x07);

	memset(buffer, 0, 4); 
	buffer[0] = 0x1;
	BMAI2C_RxData(buffer, 4);
	printk(KERN_INFO "al_version:0x%x, ml_version:0x%x\n", (buffer[0] & 0xf0)>>4 , buffer[0] & 0x0f);

	//15h
	memset(buffer, 0, 4);
	buffer[0] = 0x15;
	BMAI2C_RxData(buffer, 4);
	printk(KERN_INFO "bma020 15h buffer[0]:0x%x, buffer[1]:0x%x, buffer[2]:0x%x, buffer[3]:0x%x\n",
		buffer[0], buffer[1], buffer[2], buffer[3]);
		
	memset(buffer, 0, 4);
	buffer[0] = 0x15;
	buffer[1] = 0x40;
	BMAI2C_TxData(buffer,2);

	memset(buffer, 0, 4);
	buffer[0] = 0x10;
	BMAI2C_RxData(buffer, 4);
	printk(KERN_INFO "bma020 15h buffer[0]:0x%x, buffer[1]:0x%x, buffer[2]:0x%x, buffer[3]:0x%x\n",
		buffer[0], buffer[1], buffer[2], buffer[3]);

	//14h
	buffer[0] = 0x14;
	BMAI2C_RxData(buffer, 4);
	printk(KERN_INFO "bma020 14h buffer[0]:0x%x, buffer[1]:0x%x, buffer[2]:0x%x, buffer[3]:0x%x\n",
		buffer[0], buffer[1], buffer[2], buffer[3]);
		
	memset(buffer, 0, 4);
	buffer[0] = 0x14;
	buffer[1] = 0x08;
	BMAI2C_TxData(buffer,2);

	memset(buffer, 0, 4);
	buffer[0] = 0x14;
	BMAI2C_RxData(buffer, 4);
	printk(KERN_INFO "bma020 14h buffer[0]:0x%x, buffer[1]:0x%x, buffer[2]:0x%x, buffer[3]:0x%x\n",
		buffer[0], buffer[1], buffer[2], buffer[3]);


	//11h
	buffer[0] = 0x11;
	BMAI2C_RxData(buffer, 4);
	printk(KERN_INFO "bma020 11h buffer[0]:0x%x, buffer[1]:0x%x, buffer[2]:0x%x, buffer[3]:0x%x\n",
		buffer[0], buffer[1], buffer[2], buffer[3]);
		
	memset(buffer, 0, 4);
	buffer[0] = 0x11;
	buffer[1] = 0x0;
	BMAI2C_TxData(buffer,2);

	memset(buffer, 0, 4);
	buffer[0] = 0x11;
	BMAI2C_RxData(buffer, 4);
	printk(KERN_INFO "bma020 11h buffer[0]:0x%x, buffer[1]:0x%x, buffer[2]:0x%x, buffer[3]:0x%x\n",
		buffer[0], buffer[1], buffer[2], buffer[3]);


	//10h
	buffer[0] = 0x10;
	BMAI2C_RxData(buffer, 4);
	printk(KERN_INFO "bma020 10h buffer[0]:0x%x, buffer[1]:0x%x, buffer[2]:0x%x, buffer[3]:0x%x\n",
		buffer[0], buffer[1], buffer[2], buffer[3]);
		
	memset(buffer, 0, 4);
	buffer[0] = 0x10;
	buffer[1] = 0x3;
	BMAI2C_TxData(buffer,2);

	memset(buffer, 0, 4);
	buffer[0] = 0x10;
	BMAI2C_RxData(buffer, 4);
	printk(KERN_INFO "bma020 10h buffer[0]:0x%x, buffer[1]:0x%x, buffer[2]:0x%x, buffer[3]:0x%x\n",
		buffer[0], buffer[1], buffer[2], buffer[3]);

	
	//0bh
	memset(buffer, 0, 4);
	buffer[0] = 0x0b;
	BMAI2C_RxData(buffer, 4);
	printk (KERN_INFO "bma020 0bh buffer[0]:0x%x, buffer[1]:0x%x, buffer[2]:0x%x, buffer[3]:0x%x\n",
		buffer[0], buffer[1], buffer[2], buffer[3]);

	memset(buffer, 0, 4);
	buffer[0] = 0x0b;
	buffer[1] = 0x40; //any_motion | laert
	BMAI2C_TxData(buffer,2);

	memset(buffer, 0, 4);
	buffer[0] = 0x0b;
	BMAI2C_RxData(buffer, 4);
	printk (KERN_INFO "bma020 0bh buffer[0]:0x%x, buffer[1]:0x%x, buffer[2]:0x%x, buffer[3]:0x%x\n",
		buffer[0], buffer[1], buffer[2], buffer[3]);



    bma020_set_range(BMA020_RANGE_2G);

    bma020_set_bandwidth(BMA020_BW_25HZ);

    bma020_sleep();

    spin_lock_init(&g_lock);

    g_client_count = 0;

	return 0;

}

static int bma020_dev_open( struct inode * inode, struct file * file )
{
	printk( KERN_INFO "BMA020: dev_open.\n" );
    spin_lock(&g_lock);
    if (0 > g_client_count)
        g_client_count = 0;
    ++g_client_count;
    spin_unlock(&g_lock);
    printk(KERN_INFO "BMA020: g_client_count = %d\n", g_client_count);
    bma020_wakeup();
	return 0;
}


static int bma020_dev_release( struct inode * inode, struct file * filp )
{
	printk( KERN_INFO "BMA020: dev_release.\n");
    // Workaround! Because hal layer would call open again in close function,
    // We need to minus 2 here when close;
    spin_lock(&g_lock);
    g_client_count -= 2;
    if (0 > g_client_count)
        g_client_count = 0;
    spin_unlock(&g_lock);
    printk(KERN_INFO "BMA020: g_client_count = %d\n", g_client_count);

    if (0 >= g_client_count)
        bma020_sleep();
    
	return 0;
}

static ssize_t bma020_dev_read( struct file * file, char __user * buffer, size_t size, loff_t * f_pos )
{
	char data[6];
	bma020acc_t acc;
    int result;
    
	//printk( KERN_INFO "BMA020: dev_read\n" );    

    memset(data, 0, 6);
    data[0] = 0x02;
    result = BMAI2C_RxData(data, 6);	

    acc.x = BMA020_GET_BITSLICE(data[0],ACC_X_LSB) | BMA020_GET_BITSLICE(data[1],ACC_X_MSB)<<ACC_X_LSB__LEN;
    acc.x = acc.x << (sizeof(short)*8-(ACC_X_LSB__LEN+ACC_X_MSB__LEN));
    acc.x = acc.x >> (sizeof(short)*8-(ACC_X_LSB__LEN+ACC_X_MSB__LEN));

    acc.y = BMA020_GET_BITSLICE(data[2],ACC_Y_LSB) | BMA020_GET_BITSLICE(data[3],ACC_Y_MSB)<<ACC_Y_LSB__LEN;
    acc.y = acc.y << (sizeof(short)*8-(ACC_Y_LSB__LEN + ACC_Y_MSB__LEN));
    acc.y = acc.y >> (sizeof(short)*8-(ACC_Y_LSB__LEN + ACC_Y_MSB__LEN));

    acc.z = BMA020_GET_BITSLICE(data[4],ACC_Z_LSB) | BMA020_GET_BITSLICE(data[5],ACC_Z_MSB)<<ACC_Z_LSB__LEN;
    acc.z = acc.z << (sizeof(short)*8-(ACC_Z_LSB__LEN+ACC_Z_MSB__LEN));
    acc.z = acc.z >> (sizeof(short)*8-(ACC_Z_LSB__LEN+ACC_Z_MSB__LEN));

    //printk(KERN_INFO "===> x:%d, y:%d, z:%d\n", acc.x, acc.y, acc.z);

    ((bma020acc_t*)buffer)->x = acc.x;
    ((bma020acc_t*)buffer)->y = acc.y;
    ((bma020acc_t*)buffer)->z = acc.z;

    /*
    printk(KERN_INFO "---> x:%d, y:%d, z:%d\n", 
            ((bma020acc_t*)buffer)->x, 
            ((bma020acc_t*)buffer)->y,
            ((bma020acc_t*)buffer)->z);
    */

    return result;    
}

static void bma020_report_value(short *rbuf)
{
	char buffer[6];
	bma020acc_t acc;
	short y_z_change_tmp = 0;
	struct bma020_data *data = i2c_get_clientdata(this_client);
#if DEBUG
	printk("bma020_report_value: yaw = %d, pitch = %d, roll = %d\n", rbuf[0],
	       rbuf[1], rbuf[2]);
	printk("                    tmp = %d, m_stat= %d, g_stat=%d\n", rbuf[3],
	       rbuf[4], rbuf[5]);
	printk("          G_Sensor:   x = %d LSB, y = %d LSB, z = %d LSB\n",
	       rbuf[6], rbuf[7], rbuf[8]);
#endif


	memset(buffer, 0, 6);
	buffer[0] = 0x02;
	BMAI2C_RxData(buffer, 6);

	/* Report acceleration sensor information */
	if(aflag)
	{

		acc.x = BMA020_GET_BITSLICE(buffer[0],ACC_X_LSB) | BMA020_GET_BITSLICE(buffer[1],ACC_X_MSB)<<ACC_X_LSB__LEN;
		acc.x = acc.x << (sizeof(short)*8-(ACC_X_LSB__LEN+ACC_X_MSB__LEN));
		acc.x = acc.x >> (sizeof(short)*8-(ACC_X_LSB__LEN+ACC_X_MSB__LEN));

		acc.y = BMA020_GET_BITSLICE(buffer[2],ACC_Y_LSB) | BMA020_GET_BITSLICE(buffer[3],ACC_Y_MSB)<<ACC_Y_LSB__LEN;
		acc.y = acc.y << (sizeof(short)*8-(ACC_Y_LSB__LEN + ACC_Y_MSB__LEN));
		acc.y = acc.y >> (sizeof(short)*8-(ACC_Y_LSB__LEN + ACC_Y_MSB__LEN));
	
		acc.z = BMA020_GET_BITSLICE(buffer[4],ACC_Z_LSB) | BMA020_GET_BITSLICE(buffer[5],ACC_Z_MSB)<<ACC_Z_LSB__LEN;
		acc.z = acc.z << (sizeof(short)*8-(ACC_Z_LSB__LEN+ACC_Z_MSB__LEN));
		acc.z = acc.z >> (sizeof(short)*8-(ACC_Z_LSB__LEN+ACC_Z_MSB__LEN));
		
		y_z_change_tmp = acc.y;
		acc.y = acc.z;
		acc.z = y_z_change_tmp;
	
		input_report_abs(data->input_dev, ABS_X, acc.x);
		input_report_abs(data->input_dev, ABS_Y, acc.y);
		input_report_abs(data->input_dev, ABS_Z, acc.z);
		//input_report_abs(data->input_dev, ABS_WHEEL, rbuf[5]);
	}


// not support now
#if 0
	/* Report magnetic sensor information */
	if (atomic_read(&mflag)) {
		input_report_abs(data->input_dev, ABS_RX, rbuf[0]);
		input_report_abs(data->input_dev, ABS_RY, rbuf[1]);
		input_report_abs(data->input_dev, ABS_RZ, rbuf[2]);
		input_report_abs(data->input_dev, ABS_RUDDER, rbuf[4]);
	}

	/* Report temperature information */
	if (atomic_read(&tflag)) {
		input_report_abs(data->input_dev, ABS_THROTTLE, rbuf[3]);
	}

	if (atomic_read(&mvflag)) {
		input_report_abs(data->input_dev, ABS_HAT0X, rbuf[9]);
		input_report_abs(data->input_dev, ABS_HAT0Y, rbuf[10]);
		input_report_abs(data->input_dev, ABS_BRAKE, rbuf[11]);
	}
#endif

	input_sync(data->input_dev);
}

static int bma020_dev_ioctl( struct inode * inode, struct file * filp, unsigned int cmd, unsigned long arg )
{
	//printk( KERN_INFO "BMA020: dev_ioctl, cmd = 0x%x\n", cmd );
	void __user *argp = (void __user *)arg;
	
	switch(cmd)
	{
		case ECS_IOCTL_APP_SET_MFLAG:
		{	
			if (copy_from_user(&mflag, argp, sizeof(mflag)))
			{
				return -EFAULT;
			}
			printk( KERN_INFO "BMA020: ECS_IOCTL_APP_SET_MFLAG cmd=0x%x, mflag=%d\n", cmd, mflag );

			if (mflag < 0 || mflag > 1)
				return -EINVAL;
		}
		break;
		case ECS_IOCTL_APP_GET_MFLAG:
		{
			if (copy_to_user(argp, &mflag, sizeof(mflag)))
				return -EFAULT;
			printk( KERN_INFO "BMA020: ECS_IOCTL_APP_GET_MFLAG cmd=0x%x, mflag:%d\n", cmd, mflag );
		}
		break;
		case ECS_IOCTL_APP_SET_AFLAG:
		{	
			if (copy_from_user(&aflag, argp, sizeof(aflag)))
			{
				return -EFAULT;
			}
			printk( KERN_INFO "BMA020: ECS_IOCTL_APP_SET_AFLAG cmd=0x%x, aflag=%d\n", cmd, aflag );
			if (aflag < 0 || aflag > 1)
			{
				aflag = 0;
				return -EINVAL;
			}
			
			if(aflag)
			{
				msleep(300); // wait for last work finish
				schedule_work(&bma020_accwork);
			}
		}
		break;
		case ECS_IOCTL_APP_GET_AFLAG:
		{	
			if (copy_to_user(argp, &aflag, sizeof(aflag)))
				return -EFAULT;

			printk( KERN_INFO "BMA020: ECS_IOCTL_APP_GET_AFLAG cmd=0x%x, aflag:%d\n", cmd , aflag);
		}
		break;
		case ECS_IOCTL_APP_SET_TFLAG:
		printk( KERN_INFO "BMA020: ECS_IOCTL_APP_SET_TFLAG cmd=0x%x\n", cmd );
		break;
		case ECS_IOCTL_APP_GET_TFLAG:
		printk( KERN_INFO "BMA020: ECS_IOCTL_APP_GET_TFLAG cmd=0x%x\n", cmd );
		break;
		case ECS_IOCTL_APP_RESET_PEDOMETER:
		printk( KERN_INFO "BMA020: ECS_IOCTL_APP_RESET_PEDOMETER cmd=0x%x\n", cmd );
		break;
		case ECS_IOCTL_APP_SET_DELAY:
		{
			if (copy_from_user(&delay, argp, sizeof(delay)))
			{
				return -EFAULT;
			}
			printk( KERN_INFO "BMA020: ECS_IOCTL_APP_SET_DELAY cmd=0x%x, delay=%d\n", cmd, delay );
		}
		break;
		case ECS_IOCTL_APP_GET_DELAY:
		printk( KERN_INFO "BMA020: ECS_IOCTL_APP_GET_DELAY\n" );	
		break;
		case ECS_IOCTL_APP_SET_MVFLAG:
		{
			if (copy_from_user(&mvflag, argp, sizeof(mvflag)))
			{
				return -EFAULT;
			}
			printk( KERN_INFO "BMA020: ECS_IOCTL_APP_SET_MVFLAG cmd=0x%x, mvflag=%d\n", cmd, mvflag );
			if (mvflag < 0 || mvflag > 1)
				return -EINVAL;
			
		}
		break;
		case ECS_IOCTL_APP_GET_MVFLAG:
		{
			if (copy_to_user(argp, &mvflag, sizeof(mvflag)))
				return -EFAULT;
			printk( KERN_INFO "BMA020: ECS_IOCTL_APP_GET_MVFLAG cmd=0x%x, mvflag:%d\n", cmd, mvflag );
		}
		break;
		case ECS_IOCTL_SET_STEP_CNT:
		printk( KERN_INFO "BMA020: ECS_IOCTL_SET_STEP_CNT cmd=0x%x\n", cmd );
		break;
	      /*
		case ECS_IOCTL_APP_REPORT:
		{
			//short* report_type = arg;
			printk(KERN_INFO "BMA020: ECS_IOCTL_APP_REPORT cmd=0x%x, \n", cmd);
		}
		break;
	      */
	      	default:
		printk( KERN_INFO "BMA020: default cmd=0x%x\n", cmd );
		break;
	}

	return 0;
}

static int bma020_aot_open(struct inode *inode, struct file *file)
{
	int ret = -1;
	printk( KERN_INFO "BMA020: bma020_aot_open + \n");
	if (atomic_cmpxchg(&open_count, 0, 1) == 0) {
		if (atomic_cmpxchg(&open_flag, 0, 1) == 0) {
			atomic_set(&reserve_open_flag, 1);
			wake_up(&open_wq);
			ret = 0;
		}
	}
	return ret;
}

static int bma020_aot_release(struct inode *inode, struct file *file)
{
	printk( KERN_INFO "BMA020: bma020_aot_release + \n");
	atomic_set(&reserve_open_flag, 0);
	atomic_set(&open_flag, 0);
	atomic_set(&open_count, 0);
	wake_up(&open_wq);
	return 0;
}


static int bma020_aot_ioctl(struct inode *inode, struct file *file,
	      unsigned int cmd, unsigned long arg)
{
	printk( KERN_INFO "BMA020: bma_fdop_ioctl, cmd = 0x%x\n", cmd );
	void __user *argp = (void __user *)arg;
	short value[12];
	switch(cmd)
	{
		case ECS_IOCTL_INIT:
		printk( KERN_INFO "BMA020: ECS_IOCTL_INIT cmd=0x%x\n", cmd );
		break;
		case ECS_IOCTL_WRITE:
		printk( KERN_INFO "BMA020: ECS_IOCTL_WRITE cmd=0x%x\n", cmd );
		break;
		case ECS_IOCTL_READ:
		printk( KERN_INFO "BMA020: ECS_IOCTL_READ cmd=0x%x\n", cmd );
		break;
		case ECS_IOCTL_RESET:
		printk( KERN_INFO "BMA020: ECS_IOCTL_RESET cmd=0x%x\n", cmd );
		break;
		case ECS_IOCTL_INT_STATUS:
		printk( KERN_INFO "BMA020: ECS_IOCTL_INT_STATUS cmd=0x%x\n", cmd );
		break;
		case ECS_IOCTL_FFD_STATUS:
		printk( KERN_INFO "BMA020: ECS_IOCTL_FFD_STATUS cmd=0x%x\n", cmd );
		break;
		case ECS_IOCTL_SET_MODE:
		printk( KERN_INFO "BMA020: ECS_IOCTL_SET_MODE cmd=0x%x\n", cmd );
		break;
		case ECS_IOCTL_GETDATA:
		printk( KERN_INFO "BMA020: ECS_IOCTL_GETDATA cmd=0x%x\n", cmd );
		break;
		case ECS_IOCTL_GET_NUMFRQ:
		printk( KERN_INFO "BMA020: ECS_IOCTL_GET_NUMFRQ cmd=0x%x\n", cmd );
		break;
		case ECS_IOCTL_SET_PERST:
		printk( KERN_INFO "BMA020: ECS_IOCTL_SET_PERST cmd=0x%x\n", cmd );
		break;
		case ECS_IOCTL_SET_G0RST:
		printk( KERN_INFO "BMA020: ECS_IOCTL_SET_G0RST cmd=0x%x\n", cmd );
		break;
		case ECS_IOCTL_SET_YPR:
		{	
			printk( KERN_INFO "BMA020: ECS_IOCTL_SET_YPR cmd=0x%x\n", cmd );
			if (copy_from_user(&value, argp, sizeof(value)))
				return -EFAULT;
			bma020_report_value(value);
		}
		break;
		case ECS_IOCTL_GET_OPEN_STATUS:
		printk( KERN_INFO "BMA020: ECS_IOCTL_GET_OPEN_STATUS cmd=0x%x\n", cmd );
		break;
		case ECS_IOCTL_GET_CLOSE_STATUS:
		printk( KERN_INFO "BMA020: ECS_IOCTL_GET_CLOSE_STATUS cmd=0x%x\n", cmd );
		break;
		case ECS_IOCTL_GET_CALI_DATA:
		printk( KERN_INFO "BMA020: ECS_IOCTL_GET_CALI_DATA cmd=0x%x\n", cmd );
		break;
		case ECS_IOCTL_GET_DELAY:
		{
			if (copy_to_user(argp, &delay, sizeof(delay)))
				return -EFAULT;
			printk( KERN_INFO "BMA020: ECS_IOCTL_GET_DELAY cmd=0x%x, delay:%d\n", cmd, delay );
		}
		break;
		case ECS_IOCTL_APP_SET_MODE:
		printk( KERN_INFO "BMA020: ECS_IOCTL_APP_SET_MODE cmd=0x%x\n", cmd );
		break;	
		default:
		return -ENOTTY;
	}
}


static int __devinit gsensor_bma020_init(void)
{
	int iRet;
	printk(KERN_INFO "gsensor_BMA020_init\n");

	// add a i2c driver
	iRet = i2c_add_driver(&gensor_bma020_driver);
	if(iRet)
	{
		printk(KERN_INFO "BMA020_init: i2c_add_driver failed.\n");
	}
	

	return iRet;
}

static void __exit gsensor_bma020_exit(void)
{
	printk(KERN_INFO "[BMA020.c] gsensor_BMA020_exit\n");
	i2c_del_driver(&gensor_bma020_driver);
}

module_init(gsensor_bma020_init);
module_exit(gsensor_bma020_exit);


int bma020_wakeup()
{
    printk(KERN_INFO "[BMA020.c] bma020_wakeup\n");
    g_gsensor_power_mode = BMA020_MODE_NORMAL;
    bma020_set_mode(BMA020_MODE_NORMAL);
    //bma020_soft_reset();
    mdelay(2);
    return 0;
}

int bma020_sleep()
{
    printk(KERN_INFO "[BMA020.c] bma020_sleep\n");
    g_gsensor_power_mode = BMA020_MODE_SLEEP;
    bma020_set_mode(BMA020_MODE_SLEEP);
    return 0;
}


int bma020_read_reg(unsigned char addr, unsigned char* data, unsigned char len)
{
	char buffer[10];

    if (10 < len)
        return -1;

    buffer[0] = addr;
    BMAI2C_RxData(buffer, len);
    strncpy(data, buffer, len);
    //data[0] = buffer[0];
    
    return 0;    
}


int bma020_write_reg(unsigned char addr, unsigned char* data, unsigned char len)
{
    char buffer[10];

    if (10 < len)
        return -1;

    buffer[0] = addr;
	//buffer[1] = data[0];
	strncpy(buffer+1, data, len);
	BMAI2C_TxData(buffer, len+1);
    return 0;
}


int bma020_set_mode(unsigned char mode)
{
    unsigned char data1, data2;

	if ( (0 == mode) || (2 == mode) || (3 == mode)) 
    {
        bma020_read_reg(WAKE_UP__REG, &data1, 1);
        data1  = BMA020_SET_BITSLICE(data1, WAKE_UP, mode);		  
        bma020_read_reg(SLEEP__REG, &data2, 1);
        data2  = BMA020_SET_BITSLICE(data2, SLEEP, (mode>>1));
        bma020_write_reg(WAKE_UP__REG, &data1, 1);
        bma020_write_reg(SLEEP__REG, &data2, 1);
	} 
	return 0;
}

int bma020_get_mode(unsigned char * mode)
{

    return 0;
}

int bma020_set_range(char range)
{
    unsigned char data;

    if (((int)range)<3 && ((int)range)>=0) 
    {
        bma020_read_reg(RANGE__REG, &data, 1);
        data = BMA020_SET_BITSLICE(data, RANGE, range);
        bma020_write_reg(RANGE__REG, &data, 1);
    }
   
    return 0;
}


int bma020_get_range(unsigned char* range)
{
    unsigned char data;

    bma020_read_reg(RANGE__REG, &data, 1);
    data = BMA020_GET_BITSLICE(data, RANGE);
    *range = data;

    return 0;
}

int bma020_set_bandwidth(char bw)
{
    unsigned char data;

    if (((int)bw)<=6 && ((int)bw)>=0)
    {
        bma020_read_reg(RANGE__REG, &data, 1);
        data = BMA020_SET_BITSLICE(data, BANDWIDTH, bw);
        bma020_write_reg(RANGE__REG, &data, 1);
    }

    return 0;
}


int bma020_get_bandwidth(unsigned char* bw)
{
    unsigned char data;

    bma020_read_reg(RANGE__REG, &data, 1);
    data = BMA020_GET_BITSLICE(data, BANDWIDTH);
    *bw = data;

    return 0;
}


int bma020_read_accel_xyzt(bma020acc_t* acc)
{
    unsigned char data[6];

    bma020_read_reg(ACC_X_LSB__REG, data, 6);

    acc->x = data[0];
    acc->y = data[1];
    acc->z = data[2];
    
	acc->x = BMA020_GET_BITSLICE(data[0],ACC_X_LSB) | BMA020_GET_BITSLICE(data[1],ACC_X_MSB)<<ACC_X_LSB__LEN;
	acc->x = acc->x << (sizeof(short)*8-(ACC_X_LSB__LEN+ACC_X_MSB__LEN));
	acc->x = acc->x >> (sizeof(short)*8-(ACC_X_LSB__LEN+ACC_X_MSB__LEN));

	acc->y = BMA020_GET_BITSLICE(data[2],ACC_Y_LSB) | BMA020_GET_BITSLICE(data[3],ACC_Y_MSB)<<ACC_Y_LSB__LEN;
	acc->y = acc->y << (sizeof(short)*8-(ACC_Y_LSB__LEN + ACC_Y_MSB__LEN));
	acc->y = acc->y >> (sizeof(short)*8-(ACC_Y_LSB__LEN + ACC_Y_MSB__LEN));
	
	
	acc->z = BMA020_GET_BITSLICE(data[4],ACC_Z_LSB) | BMA020_GET_BITSLICE(data[5],ACC_Z_MSB)<<ACC_Z_LSB__LEN;
	acc->z = acc->z << (sizeof(short)*8-(ACC_Z_LSB__LEN+ACC_Z_MSB__LEN));
	acc->z = acc->z >> (sizeof(short)*8-(ACC_Z_LSB__LEN+ACC_Z_MSB__LEN));


    return 0;
}


int bma020_soft_reset()
{
    unsigned char data;

    data = BMA020_SET_BITSLICE(data, SOFT_RESET, 1);
    bma020_write_reg(SOFT_RESET__REG, &data, 1);

    return 0;
}



