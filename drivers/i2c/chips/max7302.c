/*
 *     max7302.c - MAX7302 LED EXPANDER for BACKLIGHT and LEDs CONTROLLER
 *
 *     Copyright (C) 2008 Piny CH Wu <pinychwu@tp.cmcs.com.tw>
 *     Copyright (C) 2008 Chi Mei Communication Systems Inc.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; version 2 of the License.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/sysctl.h>
#include <linux/i2c.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <linux/major.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/ioctl.h>
// +++FIH_ADQ +++
#include <linux/i2c/max7302.h>
#include <mach/gpio.h>
#include <mach/msm_iomap.h>
#include <mach/msm_smd.h>
// ---FIH_ADQ ---

//I2C 
#define I2C_SLAVE_ADDR	0x4D

//GPIO
#define GPIO_RST	84
typedef enum GPIO_HL_T {
  GPIO_LOW=0,
  GPIO_HIGH
} GPIO_HL;

int uiHWID = 0; // Teng Rui MAX8831

// +++FIH_ADQ +++
#define i2cmax7302_name "max7302"
// ---FIH_ADQ ---

//MAX7302
#define KPD_BL_PORT PORT_P2_REG		//keypad bl register
#define LCD_BL_PORT PORT_P3_REG		//lcd bl register
#define LED1_R_PORT	PORT_P4_REG		//led2001 R
#define LED1_G_PORT	PORT_P5_REG		//led2001 G
#define LED1_B_PORT	PORT_P6_REG		//led2001 B
#define LED2_PORT	PORT_P7_REG		//led2004
#define LED3_PORT	PORT_P8_REG		//led2002
#define LED4_PORT	PORT_P9_REG		//led2005

/* FIH, AudiPCHuang, 200/02/19, { */
/* ZEUS_ANDROID_CR, Set the PORT12 for EVT2 keypad backlight*/
//MAX8831 -EVT2 or later
#define KPD_BL_PORT_EVT2 PORT_P12_REG
/* } FIH, AudiPCHuang, 2009/02/19 */

static DEFINE_MUTEX(g_mutex);

struct cdev * bl_max7302_cdev = NULL;
struct i2c_client *bl_max7302_i2c = NULL;
static struct proc_dir_entry *bl_max7302_proc_file = NULL;
static struct max7302_chip_data *chip_data = NULL;

/****************Driver general function**************/
int check_max7302_exist(void)
{
  	if((!bl_max7302_i2c) || (!bl_max7302_cdev) || (!chip_data))
	{
	  	printk(KERN_ERR "Error!! Driver max7302 not exist!!\n");
	  	return -1;
	}
	return 0;
}
EXPORT_SYMBOL(check_max7302_exist);

/*************I2C functions*******************/
static int i2c_rx( u8 * rxdata, int length )
{
	struct i2c_msg msgs[] =
	{
		{
			.addr = I2C_SLAVE_ADDR,
			.flags = 0,
			.len = 1,
			.buf = rxdata,
		},
		{
			.addr = I2C_SLAVE_ADDR,
			.flags = I2C_M_RD,
			.len = length,
			.buf = rxdata,
		},
	};

	int ret;
	if( ( ret = i2c_transfer( bl_max7302_i2c->adapter, msgs, 2 ) ) < 0 )
	{
		printk( KERN_INFO "i2c rx failed %d\n", ret );
		return -EIO;
	}

	return 0;
}

static int i2c_tx( u8 * txdata, int length )
{
	struct i2c_msg msg[] =
	{
		{
			.addr = I2C_SLAVE_ADDR,
			.flags = 0,
			.len = length,
			.buf = txdata,
		},
	};

	if( i2c_transfer( bl_max7302_i2c->adapter, msg, 1 ) < 0 )
	{
		printk( KERN_INFO "i2c tx failed\n" );
		return -EIO;
	}

	return 0;
}

/*****************gpio function****************/
static int gpio_init_setting(void)
{
  	int ret = 0,GPIO84;
	
	//request gpio
	ret = gpio_request(GPIO_RST, "max7302-84");
	if(ret) {
	  	printk(KERN_ERR "%s: GPIO-%d request failed\n", __func__, GPIO_RST);
		return ret;
	}
	
	//check value before set
	GPIO84 = gpio_get_value( GPIO_RST );
	if(GPIO84) {
		printk(KERN_INFO "the status of  max7302 is power on\n ");	
		gpio_set_value( GPIO_RST, GPIO_LOW );
		mdelay(100);
	}else{
		printk(KERN_INFO "the status of  max7302 is power off\n ");		
	}
	//set value
	gpio_set_value( GPIO_RST, GPIO_HIGH );
/*	
	mdelay(10);
	//check value after set
	ret = gpio_get_value( GPIO_RST );
	if(ret) {
	  	printk(KERN_ERR "%s: GPIO-%d second get value failed\n", __func__, GPIO_RST);
		return ret;
	}
*/  
	gpio_free( GPIO_RST );

	return ret;
}

/*****************backlight & led general control function*************/
static int pwm_level_check(int port, int data)
{
	int gataBuf;
	
	gataBuf = data;
  	if(data <= 0)
	  	data = PWM_STATIC_LOW;
	else if(data > 100)
	  	data = PWM_STATIC_HIGH;
	else
	  	data = PWM_LEVEL(data);

	if (uiHWID < CMCS_HW_VER_EVT2)
	{
	  	if(data <= 0)
			data = 0x00;
		else if(data > 31)
			data = 0x70;
		else
	            	data = ( (31-data) | 0x41);
	            	
		if (port == KPD_BL_PORT || port == LED2_PORT || port == LED3_PORT || port == LED4_PORT) {
		  	if(gataBuf <= 0)
			  	gataBuf = PWM_STATIC_LOW;
			else if(gataBuf > 31)
			  	gataBuf = PWM_STATIC_HIGH;
			else
			  	gataBuf = PWM_LEVEL(data);	
			data = gataBuf;		
		}		
	}
	
	return data;
}

static int blk_level_check(int data)
{
  	if(data <= 0)
	  	data = BLK_STATIC_LOW;
	else if(data > 15)
	  	data = BLK_STATIC_HIGH;
	else
	  	data = BLK_LEVEL(data);

	return data;
}

static int blk_prd_check(int data)
{
  	if(data < 0)
	  	data = 0;
	else if(data > 7)
	  	data = 7;

	return data;
}

int max7302_port_set_level(int port, int level)
{
 	int ret = 0;
	struct max7302_i2c_data value;

	mutex_lock(&g_mutex);	
	if (uiHWID < CMCS_HW_VER_EVT2)
	{
		port = ((port > LED4_PORT)||(port < KPD_BL_PORT)) ? 0 : port;
		if (0 == port)
		{
			mutex_unlock(&g_mutex);

		  	printk(KERN_NOTICE "port number invalid\n");
			return -1;
		}
	}
	
	level = pwm_level_check(port, level);

	//set lcd bl port on
	value.reg = port;
	value.data = level;
	ret = i2c_tx((u8*)&value, 2);
	
	if (ret >= 0) {
		chip_data->reg[port] = level;
	}
	mutex_unlock(&g_mutex);

	if(ret)
	{
	  	printk(KERN_INFO "set i2c tx on failed\n");
		return ret;
	}
	return level; //This might for record it.
}
EXPORT_SYMBOL(max7302_port_set_level);

static void max8831_enable(int port, int data)
{
 	int ret = 0;
	struct max7302_i2c_data value;
	
	//set lcd bl port on
	value.reg = port;
	value.data = data;
	ret = i2c_tx((u8*)&value, 2);
	if(ret)
		printk(KERN_INFO "Enable MAX8831 failed\n");
}

static int max7302_set_conf26(int blk_prd, int rst_timer, int rst_por)
{
  	int ret = 0;
	struct max7302_i2c_data value;

	mutex_lock(&g_mutex);
	blk_prd = blk_prd_check(blk_prd);

	value.reg = CONF_26_REG;
	value.data = CONF_26_DATA(blk_prd, rst_timer, rst_por);
	ret = i2c_tx((u8*)&value, 2);	
	if(ret)
	{
	  	mutex_unlock(&g_mutex);

	  	printk(KERN_ERR "set i2c tx on failed\n");
		return ret;
	}
	
	chip_data->reg26 = value.data;
	chip_data->blk_prd = blk_prd;
	mutex_unlock(&g_mutex);

	return ret;
}

static int max7302_set_conf27(int bus_t, int p3_oscout, int p2_oscin, int p1_int, int input_tran)
{
  	int ret = 0;
	struct max7302_i2c_data value;

	mutex_lock(&g_mutex);
	value.reg = CONF_27_REG;
	value.data = CONF_27_DATA(bus_t, p3_oscout, p2_oscin, p1_int, input_tran);
	ret = i2c_tx((u8*)&value, 2);
	if(ret)
	{
		mutex_unlock(&g_mutex);

	  	printk(KERN_ERR "set i2c tx onfailed\n");
		return ret;
	}
	
	chip_data->reg27 = value.data;
	mutex_unlock(&g_mutex);

	return ret;
}



/*****************LCD part***************/
int lcd_bl_set_intensity(int level)
{
  	int ret = 0;
	struct max7302_i2c_data value;

	if(check_max7302_exist())
	  	return -1;
	
	if (uiHWID < CMCS_HW_VER_EVT2)
	{
		level= level*32/100;
		ret = max7302_port_set_level(LCD_BL_PORT, level);	
		//printk(KERN_INFO "EVT1 Set BL %d", level);
	}else{
/* FIH_ADQ , added by guorui */	
		value.reg = 0x00;
		ret = i2c_rx((u8*)&value, 2);
	
		if(level == 0 && (value.reg & 0x01)) 
		{
			value.data = (value.reg & 0xFE);
			value.reg = 0x00;
			ret = i2c_tx((u8*)&value, 2);
			printk(KERN_INFO "close BL %d", value.data);
			return ret;
		}else if( (value.reg & 0x01) == 0){
			value.data = value.reg | 0x01;
			value.reg = 0x00;
			ret = i2c_tx((u8*)&value, 2);
			printk(KERN_INFO "Open BL %d\n", value.data);
		}
		ret = max7302_port_set_level(PORT_P11_REG, level);
		//printk(KERN_INFO "EVT2 set BL %d", level);
/* FIH_ADQ  */
	}
	if(ret < 0)
	  	return -1;

	return 0;
}
EXPORT_SYMBOL(lcd_bl_set_intensity);

int lcd_bl_get_intensity(void)
{
	int ret = 0;

	if(check_max7302_exist())
	  	return -1;

	mutex_lock(&g_mutex);
	ret = (int)chip_data->reg[LCD_BL_PORT];
	mutex_unlock(&g_mutex);
	return ret;
}
EXPORT_SYMBOL(lcd_bl_get_intensity);

/*****************KEYPAD part***************/
int kpd_bl_set_intensity(int level)
{
  	int ret = 0;
	struct max7302_i2c_data value;
	
	if(check_max7302_exist())
	  	return -1;

/* FIH, AudiPCHuang, 200/02/19, { */
/* ZEUS_ANDROID_CR, Set backlight pwm level by hardware version */
	if (uiHWID < CMCS_HW_VER_EVT2) {
		ret = max7302_port_set_level(KPD_BL_PORT, level);
	} else {	
		value.reg = 0x00;
		ret = i2c_rx((u8*)&value, 2);
	
		if(level == 0 && (value.reg & 0x02)) 
		{
			value.data = (value.reg & 0xFD);
			value.reg = 0x00;
			ret = i2c_tx((u8*)&value, 2);
			printk(KERN_INFO "close KBL %d", value.data);
			return ret;
		}else if( (value.reg & 0x02) == 0){
			value.data = value.reg | 0x02;
			value.reg = 0x00;
			ret = i2c_tx((u8*)&value, 2);
			printk(KERN_INFO "Open KBL %d\n", value.data);
		}
		ret = max7302_port_set_level(KPD_BL_PORT_EVT2, level);	
	}
/* } FIH, AudiPCHuang, 2009/02/19 */
	
	if(ret < 0)
	  	return -1;

	return 0;
}
EXPORT_SYMBOL(kpd_bl_set_intensity);

int kpd_bl_get_intensity(void)
{
  	int ret = 0;
	
	if(check_max7302_exist())
	  	return -1;

	mutex_lock(&g_mutex);
	ret = (int)chip_data->reg[KPD_BL_PORT];
	mutex_unlock(&g_mutex);

	return ret;
}
EXPORT_SYMBOL(kpd_bl_get_intensity);

/********************LED part************************/
int led_set_blk_t(unsigned int blk_prd)
{
  	int ret = 0;
	
	if(check_max7302_exist())
	  	return -1;

	if(blk_prd > 8)
	{
	  	printk(KERN_NOTICE "blink period level is too big, automatically assigned 7");
		blk_prd = 7;
	}
	
	ret = max7302_set_conf26(blk_prd, 1, 0);
	if(ret < 0)
	  	return -1;

	return 0;
}
EXPORT_SYMBOL(led_set_blk_t);

int led_rgb_set_intensity(int R, int G, int B)
{
  	int ret1 = 0, ret2 = 0, ret3 = 0;
	
	if (R >= 0) {
	  	ret1 = max7302_port_set_level(LED1_R_PORT, R);
	}

	if (G >= 0) {
	  	ret2 = max7302_port_set_level(LED1_G_PORT, G);
	}

	if (B >= 0)
	{
	  	ret3 = max7302_port_set_level(LED1_B_PORT, B);
	}

	if((ret1 < 0) || (ret2 < 0) || (ret3 < 0))
		return -1;

  	return 0;
}
EXPORT_SYMBOL(led_rgb_set_intensity);

void led_rgb_get_intensity(int *R, int *G, int *B)
{
  	mutex_lock(&g_mutex);
	*R = (int)chip_data->reg[LED1_R_PORT];
	*G = (int)chip_data->reg[LED1_G_PORT];
	*B = (int)chip_data->reg[LED1_B_PORT];
	mutex_unlock(&g_mutex);
}
EXPORT_SYMBOL(led_rgb_get_intensity);

int led_234_set_intensity(int L2, int L3, int L4)
{
  	int ret1 = 0, ret2 = 0, ret3 = 0;
	
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

void led_234_get_intensity(int *L2, int *L3, int *L4)
{
  	mutex_lock(&g_mutex);
	*L2 = (int)chip_data->reg[LED2_PORT];
	*L3 = (int)chip_data->reg[LED3_PORT];
	*L4 = (int)chip_data->reg[LED4_PORT];
	mutex_unlock(&g_mutex);
}
EXPORT_SYMBOL(led_234_get_intensity);

/***************max7302 backlight init***************/
static int max7302_bl_init(void)
{
	int ret = 0;
	
	chip_data = (struct max7302_chip_data*)kzalloc(sizeof(struct max7302_chip_data), GFP_KERNEL);
	if(!chip_data)
	{
	  	printk(KERN_ERR "allocate max7302 chip data failed\n");
		return -ENOSPC;
	}

	uiHWID = FIH_READ_HWID_FROM_SMEM();     

	printk(KERN_INFO "max7302_bl_init, uiHWID: %d\n", uiHWID);

	if (uiHWID >= CMCS_HW_VER_EVT2)
	{
		max8831_enable(PORT_P0_REG,3);
		ret = max7302_port_set_level(PORT_P11_REG, 101);
		if(ret<0)
		{
		  	printk(KERN_ERR "set LCD backlight PORT_P11_REG failed\n");
			return ret;
		}	
		
		ret = max7302_port_set_level(PORT_P12_REG, 0);
		if(ret<0)
		{
		  	printk(KERN_ERR "set Keypad backlight PORT_P12_REG failed\n");
			return ret;
		}	
		return 0;
	}
	else
	{
		//set cofig 26 reg
		ret = max7302_set_conf26(0, 1, 0);
	 	if(ret)
		{
		  	printk(KERN_ERR "set config 26 register failed\n");
			return ret;
		}

		//set config 27 reg
		ret = max7302_set_conf27(1, 1, 1, 1, 0);
		if(ret)
		{
		  	printk(KERN_ERR "set config 27 register failed\n");
			return ret;
		}

		//set lcd bl port (port3) on
		ret = lcd_bl_set_intensity(32);
		if(ret)
		{
		  	printk(KERN_ERR "set LCD backlight on failed\n");
			return ret;
		}

		//set keypad bl port off
		ret = kpd_bl_set_intensity(0);
		if(ret)
		{
		  	printk(KERN_ERR "set Keypad backlight off failed\n");
			return ret;
		}
		
		//set led rgb port off
		ret = led_rgb_set_intensity(32, 32, 32);
		if(ret)
		{
		  	printk(KERN_ERR "set LED1 RGB off failed\n");
			return ret;
		}

		//set led 234 port off
		ret = led_234_set_intensity(32, 32, 32);
		if(ret)
		{
		  	printk(KERN_ERR "set LED 234 off failed\n");
			return ret;
		}
		
	  	return ret;
	}	
}
///+FIH Sung Chuan
int lcd_backlight_switch(bool turn_on) 
{
	int ret = 0;
			
	if (uiHWID >= CMCS_HW_VER_EVT2)
	{
		if (turn_on) {
			max8831_enable(PORT_P0_REG,3);
			ret = max7302_port_set_level(PORT_P11_REG, 101);
			if (ret < 0) {
				printk(KERN_ERR "[EVT2 or later version] turn lcd backlight on failed\n");
				return ret;
			}
		} else {
			ret = max7302_port_set_level(PORT_P11_REG, 0);
			if (ret < 0) {
				printk(KERN_ERR "[EVT2 or later version] turn lcd backlight off failed\n");
				return ret;
			}
		}
	} else {
		if (turn_on) {
			ret = lcd_bl_set_intensity(32);
			if (ret) {
				printk(KERN_ERR "turn lcd backlight on failed\n");
				return ret;
			}
		} else {
			ret = lcd_bl_set_intensity(0);
			if (ret) {
				printk(KERN_ERR "turn lcd backlight off failed\n");
				return ret;
			}		
		}
	}
	
	return 0;
}
EXPORT_SYMBOL(lcd_backlight_switch); //EXPORT lcd_backlight_switch for other kernel modules
///+FIH Sung Chuan
/******************Proc operation********************/
static int max7302_seq_open(struct inode *inode, struct file *file)
{
  	return single_open(file, NULL, NULL);
}

static ssize_t max7302_seq_write(struct file *file, const char *buff,
								size_t len, loff_t *off)
{
	char str[64];
	int param = -1;
	int param2 = -1;
	int param3 = -1;
	char cmd[32];

	if(copy_from_user(str, buff, sizeof(str)))
	  	return -EFAULT;

//	if(sscanf(str, "%s %d", cmd, &param) == -1)
	{
	  	if(sscanf(str, "%s %d %d %d", cmd, &param, &param2, &param3) == -1)
		{
		  	printk("parameter format: <type> <value>\n");
	 		return -EINVAL;
		}
	}

	if(!strnicmp(cmd, "lcd", 3))
	  	cmd[0] = 'c';
	else if(!strnicmp(cmd, "keypad", 5))
	  	cmd[0] = 'k';
	else if(!strnicmp(cmd, "rgb", 3))
	  	cmd[0] = 'r';
	else if(!strnicmp(cmd, "led", 3))
	  	cmd[0] = 'l';
	else
	  	cmd[0] = '?';

	switch(cmd[0])
	{
	  	case 'c':
		  	lcd_bl_set_intensity(param);
		  	break;

		case 'k':
			kpd_bl_set_intensity(param);
			break;

		case 'r':
			led_rgb_set_intensity(param, param2, param3);
			break;

		case 'l':
			led_234_set_intensity(param, param2, param3);
			break;

		default:
			printk(KERN_NOTICE "type parameter error\n");
			break;
	}

	return len;
}

static struct file_operations max7302_seq_fops = 
{
  	.owner 		= THIS_MODULE,
	.open  		= max7302_seq_open,
	.write 		= max7302_seq_write,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int max7302_create_proc(void)
{
  	bl_max7302_proc_file = create_proc_entry("driver/max7302", 0666, NULL);
	
	if(!bl_max7302_proc_file){
	  	printk(KERN_INFO "create proc file for MAX7302 failed\n");
		return -ENOMEM;
	}

	printk(KERN_INFO "MAX7302 proc ok\n");
	bl_max7302_proc_file->proc_fops = &max7302_seq_fops;
	return 0;
}

/*******************File control function******************/
// devfs
static int max7302_dev_open( struct inode * inode, struct file * file )
{
	printk( KERN_INFO "MAX7302 open\n" );

	if( ( file->f_flags & O_ACCMODE ) == O_WRONLY )
	{
		printk( KERN_INFO "MAX7302's device node is readonly\n" );
		return -1;
	}
	else
		return 0;
}

static ssize_t max7302_dev_read( struct file * file, char __user * buffer, size_t size, loff_t * f_pos )
{
  	struct max7302_chip_data chip;
	printk( KERN_INFO "MAX7302 read\n" );

	mutex_lock(&g_mutex);
	chip = *chip_data;
	mutex_unlock(&g_mutex);
    
	if(copy_to_user(buffer, &chip, sizeof(struct max7302_chip_data)))
	{
	  	printk(KERN_ERR "File read process failed\n");
		return -EFAULT;
	}

	/*printk(KERN_INFO "chip.reg26  =CONFIG26:0x%2x\n", chip.reg26);
	printk(KERN_INFO "chip.reg27  =CONFIG27:0x%2x\n", chip.reg27);
	printk(KERN_INFO "chip.reg[2] =KPD_BL  :0x%2x\n", chip.reg[2]);
	printk(KERN_INFO "chip.reg[3] =LCD_BL  :0x%2x\n", chip.reg[3]);
	printk(KERN_INFO "chip.reg[4] =LED1_R  :0x%2x\n", chip.reg[4]);
	printk(KERN_INFO "chip.reg[5] =LED1_G  :0x%2x\n", chip.reg[5]);
	printk(KERN_INFO "chip.reg[6] =LED1_B  :0x%2x\n", chip.reg[6]);
	printk(KERN_INFO "chip.reg[7] =LED2    :0x%2x\n", chip.reg[7]);
	printk(KERN_INFO "chip.reg[8] =LED3    :0x%2x\n", chip.reg[8]);
	printk(KERN_INFO "chip.reg[9] =LED4    :0x%2x\n", chip.reg[9]);
	printk(KERN_INFO "chip.blk_prd=BLK_PRD :0x%2x\n", chip.blk_prd);*/
	{
 	int ret = 0;
	struct max7302_i2c_data value;
	
	//set lcd bl port on
	value.reg = 0x00;
	value.data = 0x00;
	ret = i2c_rx((u8*)&value, 2);
	printk(KERN_INFO "Read MAX7302 0x00 = %d %d\n",value.data,ret);
	}	
	return 0;
}

static ssize_t max7302_dev_write(struct file *filp, const char __user *buff, size_t count, loff_t *f_pos)
{
	char str[64];
	int param = -1;
	int param2 = -1;
	int param3 = -1;
	char cmd[32];

 	int ret = 0;
	struct max7302_i2c_data value;
	printk(KERN_INFO "MAX7302 write\n");

	if(copy_from_user(str, buff, sizeof(str)))
	  	return -EFAULT;

//	if(sscanf(str, "%s %d", cmd, &param) == -1)
	{
	  	if(sscanf(str, "%s %d %d %d", cmd, &param, &param2, &param3) == -1)
		{
		  	printk("parameter format: <type> <value>\n");
	 		return -EINVAL;
		}
	}
	
	value.reg = 0x00;
	value.data = 0x00;
	ret = i2c_rx((u8*)&value, 2);
	printk(KERN_NOTICE "Read MAX7302 0x00 =%d %d\n", value.data , ret);
	
	if(!strnicmp(cmd, "lcd", 3))
	  	cmd[0] = 'c';
	else if(!strnicmp(cmd, "keypad", 5))
	  	cmd[0] = 'k';
	else if(!strnicmp(cmd, "rgb", 3))
	  	cmd[0] = 'r';
	else if(!strnicmp(cmd, "led", 3))
	  	cmd[0] = 'l';
	else
	  	cmd[0] = '?';

	switch(cmd[0])
	{
	  	case 'c':
		  	lcd_bl_set_intensity(param);
		  	break;

		case 'k':
			kpd_bl_set_intensity(param);
			break;

		case 'r':
			led_rgb_set_intensity(param, param2, param3);
			break;

		case 'l':
			led_234_set_intensity(param, param2, param3);
			break;

		case '?':
			//lcd_bl_set_intensity(0);
		  	break;

		default:
			printk(KERN_NOTICE "type parameter error\n");
			break;
	}

	return 1;
}

static int max7302_dev_ioctl( struct inode * inode, struct file * filp, unsigned int cmd, unsigned long arg )
{
  	struct int_v{ 
	  	int p1; 
		int p2; 
		int p3;
	} write_v, read_v;
	int read_p = 0, write_p = 0;

	printk( KERN_INFO "MAX7302 ioctl, cmd = %d\n", cmd );

	switch (cmd)
	{
	  	case MAX7302_RST:
		  	max7302_bl_init();
			break;
		case MAX7302_G_LCD:
			read_p = lcd_bl_get_intensity();
			if(!copy_to_user((int __user*)arg, &read_p, sizeof(int)))
			{
			  	printk(KERN_ERR "Set user-space data error\n");
				return -1;
			}else
			  	printk(KERN_INFO "Get LCD backlight value = 0x%2x\n", read_p);
			break;

		case MAX7302_S_LCD:
			if(!copy_from_user(&write_p, (int __user*)arg, sizeof(int)))
			{
			  	printk(KERN_ERR "Get user-space data error\n");
				return -1;
			}
			read_p = lcd_bl_set_intensity(write_p);
			if(read_p)
			  	printk(KERN_NOTICE "Set LCD backlight failed\n");
			else
				printk(KERN_INFO "Set LCD backlight value = 0x%2x\n", write_p);
			break;

		case MAX7302_G_KPD:
			read_p = kpd_bl_get_intensity();
			if(!copy_to_user((int __user*)arg, &read_p, sizeof(int)))
			{
			  	printk(KERN_ERR "Set user-space data error\n");
				return -1;
			}else
			  	printk(KERN_INFO "Get KEYPAD backlight value = 0x%2x\n", read_p);
			break;

		case MAX7302_S_KPD:
			if(!copy_from_user(&write_p, (int __user*)arg, sizeof(int)))
			{
			  	printk(KERN_ERR "Get user-space data error\n");
				return -1;
			}
			read_p = kpd_bl_set_intensity(write_p);
			if(read_p)
			  	printk(KERN_NOTICE "Set KEYPAD backlight failed\n");
			else
				printk(KERN_INFO "Set KEYPAD backlight value = 0x%2x\n", write_p);
			break;

		case MAX7302_G_LED_RGB:
			led_rgb_get_intensity(&read_v.p1, &read_v.p2, &read_v.p3);
			if(!copy_to_user((struct int_v  __user*)arg, &read_v, sizeof(struct int_v)))
			{
			  	printk(KERN_ERR "Set user-space data error\n");
				return -1;
			}else
			  	printk(KERN_INFO "Get LED R=0x%2x G=0x%2x B=0x%2x\n", read_v.p1, read_v.p2, read_v.p3);
			break;

		case MAX7302_S_LED_RGB:
			if(!copy_from_user(&write_v, (struct int_v  __user*)arg, sizeof(struct int_v)))
			{
			  	printk(KERN_ERR "Get user-space data error\n");
				return -1;
			}
			read_p = led_rgb_set_intensity(write_v.p1, write_v.p2, write_v.p3);
			if(read_p)
			  	printk(KERN_NOTICE "Set KEYPAD backlight failed\n");
			else
				printk(KERN_INFO "Set LED R=0x%2x G=0x%2x B=0x%2x\n", write_v.p1, write_v.p2, write_v.p3);
			break;

		case MAX7302_G_LED_234:
			led_234_get_intensity(&read_v.p1, &read_v.p2, &read_v.p3);
			if(!copy_to_user((struct int_v  __user*)arg, &read_v, sizeof(struct int_v)))
			{
			  	printk(KERN_ERR "Set user-space data error\n");
				return -1;
			}else
			  	printk(KERN_INFO "Get LED R=0x%2x G=0x%2x B=0x%2x\n", read_v.p1, read_v.p2, read_v.p3);
			break;

		case MAX7302_S_LED_234:
			if(!copy_from_user(&write_v, (struct int_v  __user*)arg, sizeof(struct int_v)))
			{
			  	printk(KERN_ERR "Get user-space data error\n");
				return -1;
			}
			read_p = led_234_set_intensity(write_v.p1, write_v.p2, write_v.p3);
			if(read_p)
			  	printk(KERN_NOTICE "Set KEYPAD backlight failed\n");
			else
				printk(KERN_INFO "Set LED R=0x%2x G=0x%2x B=0x%2x\n", write_v.p1, write_v.p2, write_v.p3);
			break;

		default:
			printk(KERN_NOTICE "IO-Control: wrong command\n");
			return -1;
	}
	return 0;
}

static int max7302_dev_release( struct inode * inode, struct file * filp )
{
	printk(KERN_INFO "MAX7302 release\n");
	return 0;
}

static const struct file_operations max7302_dev_fops = {
	.open = max7302_dev_open,
	.read = max7302_dev_read,
	.write = max7302_dev_write,
	.ioctl = max7302_dev_ioctl,
	.release = max7302_dev_release,
};

/******************Driver Functions*******************/
static int max7302_probe(struct i2c_client *client)
{
  	int ret = 0;

	bl_max7302_i2c = client;
	printk( KERN_INFO "Driver probe: %s\n", __func__ );

	//GPIO-84 config
	gpio_tlmm_config(GPIO_CFG(84, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);

	//setting reset pin 
	ret = gpio_init_setting();
	if(ret) {
	  	printk(KERN_ERR "%s: GPIO setting failed\n", __func__);
		return ret;
	}
	
	ret = max7302_bl_init();
	if(ret)
		printk(KERN_INFO "max7302 backlight init failed\n");

	return ret;
}

static int max7302_remove(struct i2c_client *client)
{
	int ret = 0;
	
	mutex_destroy(&g_mutex);
	
	return ret;
}

static int max7302_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int ret = 0;
	struct max7302_i2c_data value;

	if (uiHWID < CMCS_HW_VER_EVT2){

	}else{	
		ret = gpio_request(GPIO_RST, "max7302-84");
		if(ret) {
		  	printk(KERN_ERR "%s: GPIO-%d request failed\n", __func__, GPIO_RST);
			return ret;
		}
		value.reg = 0x00;
		value.data = 0x00;
		ret = i2c_tx((u8*)&value, 2);	
		gpio_set_value( GPIO_RST, GPIO_LOW);

		gpio_free( GPIO_RST );
	}

        return ret;
}

static int max7302_resume(struct i2c_client *client, pm_message_t mesg)
{
	int ret = 0;
	struct max7302_i2c_data value;
		
	if (uiHWID < CMCS_HW_VER_EVT2){
	
	}else{	
		ret = gpio_request(GPIO_RST, "max7302-84");
		if(ret) {
		  	printk(KERN_ERR "%s: GPIO-%d request failed\n", __func__, GPIO_RST);
			return ret;
		}
		gpio_set_value( GPIO_RST, GPIO_HIGH);

		value.reg = 0x00;
		value.data = 0x03;
		ret = i2c_tx((u8*)&value, 2);

		gpio_free(GPIO_RST);
	}
		
	return ret;
}

// +++ FIH_ADQ +++
static const struct i2c_device_id i2cmax7302_idtable[] = {
       { i2cmax7302_name, 0 },
       { }
};
// --- FIH_ADQ ---

static struct i2c_driver max7302_driver = {
	.driver = {
		.name	= "max7302",
	},
	.probe		= max7302_probe,
	.remove		= max7302_remove,
	.suspend 	= max7302_suspend,
	.resume 	= max7302_resume,
	.id_table = i2cmax7302_idtable,
};
static int __init max7302_init(void)
{
	int ret = 0;

	printk( KERN_INFO "Driver init: %s\n", __func__ );

	//allocate chip data
	chip_data = (struct max7302_chip_data*)kzalloc(sizeof(struct max7302_chip_data), 0);
	if(!chip_data)
	{
	  	printk(KERN_ERR "%s: Allocate max7302 chip data failed\n", __func__);
		goto chip_free;
	}

	// allocate devfs
	bl_max7302_cdev = cdev_alloc();
	if(!bl_max7302_cdev) {
	  	printk(KERN_ERR "%s: Char device allocate failed\n", __func__);
		goto dev_free;
	}
	bl_max7302_cdev->ops = & max7302_dev_fops;
	bl_max7302_cdev->owner = THIS_MODULE;

	// i2c
	ret = i2c_add_driver(&max7302_driver);
	if (ret) {
		printk(KERN_ERR "%s: Driver registration failed, module not inserted.\n", __func__);
		goto driver_del;
	}

	// add devfs
	ret = cdev_add( bl_max7302_cdev, MKDEV( 234, 0 ), 1 ); //234 is unassigned
	if (ret) {
		printk(KERN_ERR "%s: Char device add failed\n", __func__);
		goto driver_del;
	}

	// create proc
	ret = max7302_create_proc();
	if(ret) {
	  	printk(KERN_ERR "%s: create proc file failed\n", __func__);
		goto driver_del;
	}

	//all successfully.
	return ret;

driver_del:
	i2c_del_driver(&max7302_driver);
dev_free:
	cdev_del(bl_max7302_cdev);
chip_free:
	kfree(chip_data);
	
	return -1;
}

static void __exit max7302_exit(void)
{
	printk( KERN_INFO "Driver exit: %s\n", __func__ );
	i2c_del_driver(&max7302_driver);
	cdev_del( bl_max7302_cdev );
	kfree(chip_data);
}

module_init(max7302_init);
module_exit(max7302_exit);

MODULE_AUTHOR( "Piny CH Wu <pinychwu@tp.cmcs.com.tw>" );
MODULE_DESCRIPTION( "MAX7302 driver" );
MODULE_LICENSE( "GPL" );

