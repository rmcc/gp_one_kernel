#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/ctype.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/spi/spi.h>
#include <linux/input.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/delay.h>

#include <linux/proc_fs.h>
#include "msm_fb.h"

//T_FIH,ADQ,JOE HSU -----
/* FIH_ADQ, Ming { */
struct spi_device	  *gLcdSpi;
/* } FIH_ADQ, Ming */

#define LCDSPI_PROC_FILE        "driver/lcd_spi"

static struct mutex               hx8352_dd_lock;
static struct list_head           dd_list;
static struct proc_dir_entry	  *lcdspi_proc_file;
static struct spi_device	  *g_spi;

#define HX8352_NAME                      "lcdc_spi"

struct driver_data {
        struct input_dev         *ip_dev;
        struct spi_device        *spi;
        char                      bits_per_transfer;
        struct work_struct        work_data;
        bool                      config;
        struct list_head          next_dd;
};

#define ARRAY_AND_SIZE(x)	(x), ARRAY_SIZE(x)
typedef struct
{
	char	reg;
	char	val;
} CONFIG_TABLE;

static const CONFIG_TABLE config_table1[] = {
// CMO 3.2" panel initial code with Gamma 2.2 refer to Zeus
	{ 0x83, 0x02},
	{ 0x85, 0x03},
	{ 0x8B, 0x00},
	{ 0x8C, 0x93},
	{ 0x91, 0x01},
	{ 0x83, 0x00},
// Gamma Setting
	{ 0x3E, 0x85},
	{ 0x3F, 0x50},
	{ 0x40, 0x00},
	{ 0x41, 0x77},
	{ 0x42, 0x24},
	{ 0x43, 0x35},
	{ 0x44, 0x00},
	{ 0x45, 0x77},
	{ 0x46, 0x10},
	{ 0x47, 0x5A},
	{ 0x48, 0x4D},
	{ 0x49, 0x40},
// Power Supply Setting
	{ 0x2B, 0xF9},
};
static const CONFIG_TABLE config_table2[] = {
	{ 0x1B, 0x14},
	{ 0x1A, 0x11},
	{ 0x1C, 0x0D},
	{ 0x1F, 0x64},//{ 0x1F, 0x5e},
};
static const CONFIG_TABLE config_table3[] = {
	{ 0x19, 0x0A},
	{ 0x19, 0x1A},
};
static const CONFIG_TABLE config_table4[] = {
	{ 0x19, 0x12},
};
static const CONFIG_TABLE config_table5[] = {
	{ 0x1E, 0x33},
};
static const CONFIG_TABLE config_table6[] = {
// Display ON Setting
	{ 0x3C, 0x60},
	{ 0x3D, 0x1C},
	{ 0x34, 0x38},
	{ 0x35, 0x38},
	{ 0x24, 0x38},
};
static const CONFIG_TABLE config_table7[] = {
	{ 0x24, 0x3C},
	{ 0x16, 0x1C},
	{ 0x3A, 0xA8},//{ 0x3a, 0xae},
	{ 0x01, 0x02},
	{ 0x55, 0x00},
};

static int lcd_spi_write(struct spi_device *spi, CONFIG_TABLE table[], int len)
{
	char reg_data[]= { 0x70, 0x00};
	char val_data[]= { 0x72, 0x00};
	int val;
	int i;
	//printk( "XXXXX lcd_spi_write len: %d XXXXX\n", len);
	for( i = 0 ; i < len ; i++ )
	{
		reg_data[ 1]= table[ i].reg;
		val_data[ 1]= table[ i].val;
		val = spi_write(spi, reg_data, sizeof( reg_data));
		if (val)
                        goto config_exit;
		val = spi_write(spi, val_data, sizeof( val_data));
		if (val)
                        goto config_exit;
		printk(KERN_INFO "[%s:%d] write reg= 0x%02X, val= 0x%02X\n",__FILE__, __LINE__, table[ i].reg, table[ i].val);
	}
        return 0;

config_exit:
        printk(KERN_ERR "SPI write error :%s -- %d\n", __func__,__LINE__);
        return val;

}

static int lcd_spi_read(struct spi_device *spi, CONFIG_TABLE table[], int len)
{
	char reg_data[]= { 0x70, 0x00};
	int val;
	int i;
	//printk( "XXXXX lcd_spi_read len: %d XXXXX\n", len);
	for( i = 0 ; i < len ; i++ )
	{
		reg_data[ 1]= table[ i].reg;
		val = spi_write(spi, reg_data, sizeof( reg_data));
		if (val)
                        goto config_exit;
		printk(KERN_INFO "[%s:%d] reg= 0x%02X, orig val= 0x%02X, read val= 0x%02X\n",__FILE__, __LINE__, table[ i].reg, table[ i].val, spi_w8r8(spi, 0x73));
	}
	 return 0;

config_exit:
        printk(KERN_ERR "SPI write error :%s -- %d\n", __func__,__LINE__);
        return val;
}


static int lcd_spi_read2(struct spi_device *spi, CONFIG_TABLE table[], int len)
{
	char reg_data[]= { 0x70, 0x00};
	int val;
	int i;
	reg_data[1]=0x00;
	val = spi_write(spi, reg_data, sizeof( reg_data));
	if (val)
                    goto config_exit;
	printk(KERN_INFO "[%s:%d] reg= 0x%02X,  read val= 0x%02X\n",__FILE__, __LINE__, reg_data[1],  spi_w8r8(spi, 0x73));
	 return 0;

config_exit:
        printk(KERN_ERR "SPI write error :%s -- %d\n", __func__,__LINE__);
        return val;
}



static int hx8352_config(struct driver_data *dd, struct spi_device *spi)
{
	lcd_spi_write( spi, ARRAY_AND_SIZE( config_table1));
	mdelay( 10);
	lcd_spi_write( spi, ARRAY_AND_SIZE( config_table2));
	mdelay( 20);
	lcd_spi_write( spi, ARRAY_AND_SIZE( config_table3));
	mdelay( 40);
	lcd_spi_write( spi, ARRAY_AND_SIZE( config_table4));
	mdelay( 40);
	lcd_spi_write( spi, ARRAY_AND_SIZE( config_table5));
	mdelay( 100);
	lcd_spi_write( spi, ARRAY_AND_SIZE( config_table6));
	mdelay( 40);
	lcd_spi_write( spi, ARRAY_AND_SIZE( config_table7));
	
	lcd_spi_read( spi, ARRAY_AND_SIZE( config_table1));
	lcd_spi_read( spi, ARRAY_AND_SIZE( config_table2));
	lcd_spi_read( spi, ARRAY_AND_SIZE( config_table3));
	lcd_spi_read( spi, ARRAY_AND_SIZE( config_table4));
	lcd_spi_read( spi, ARRAY_AND_SIZE( config_table5));
	lcd_spi_read( spi, ARRAY_AND_SIZE( config_table6));
	lcd_spi_read( spi, ARRAY_AND_SIZE( config_table7));
	
	//lcd_spi_read2( spi, ARRAY_AND_SIZE( config_table7));
	
	
}

static int hx8352_SPI_probe(struct spi_device *spi)
{
	struct driver_data *dd;
        int                 rc=0;

	printk(KERN_INFO "^^ :%s -- %d\n", __func__,__LINE__);
	dd = kzalloc(sizeof(struct driver_data), GFP_KERNEL);
        if (!dd) {
                rc = -ENOMEM;
                goto probe_exit;
        }
	
	mutex_lock(&hx8352_dd_lock);
        list_add_tail(&dd->next_dd, &dd_list);
        mutex_unlock(&hx8352_dd_lock);
        dd->spi = spi;
	g_spi= spi;
/* FIH_ADQ, Ming { */
gLcdSpi = spi;
/* } FIH_ADQ, Ming */
	
	printk(KERN_INFO "^^ :%s -- %d\n", __func__,__LINE__);
/* FIH_ADQ, Ming { */
/* Assume that bootloader already did it. */
///	rc = hx8352_config(dd,spi);
/* } FIH_ADQ, Ming */
	 if (rc)
                goto probe_err_cfg;

	return 0;
probe_err_cfg:
        mutex_lock(&hx8352_dd_lock);
        list_del(&dd->next_dd);
        mutex_unlock(&hx8352_dd_lock);
probe_exit_alloc:
        kfree(dd);
probe_exit:
        return rc;	
}

static struct spi_driver hx8352_drv = {
        .driver = {
                .name  = HX8352_NAME,
        },
        .probe         = hx8352_SPI_probe,
};

static unsigned int atoi( char* str)
{
        unsigned int i= 0, radius= 10, val= 0;

        // hex
        if( ( str[ i] == '0') && ( str[ i+ 1] == 'x'))
        {
                radius= 16;
                i+= 2;
        }

        while( str[ i] != '\0')
        {
                val*= radius;

                if( ( str[ i] >= '0') && ( str[ i] <= '9'))
                        val+= str[ i]- '0';
                else if( ( str[ i] >= 'A') && ( str[ i] <= 'F'))
                        val+= 10+ str[ i]- 'A';
                else if( ( str[ i] >= 'a') && ( str[ i] <= 'f'))
                        val+= 10+ str[ i]- 'a';
                else                    // not a recognized char
                {
                        val/= radius;
                        break;
                }
                i++;
        }

        return val;
}

static ssize_t lcdspi_proc_read(struct file *filp,
                char *buffer, size_t length, loff_t *offset)
{
        printk( KERN_INFO "ccc\n");

        return 0;
}

static ssize_t lcdspi_proc_write(struct file *filp,
                const char *buff, size_t len, loff_t *off)
{
        char msg[ 4096], tmp[ 10], tx_buf[ 2];
        unsigned int addr= 0, value= 0, i= 0;

        if( len > 4096)
                len = 4096;

        if( copy_from_user( msg, buff, len))
                return -EFAULT;

	if( strncmp("default", msg, 7) == 0)
	{
		hx8352_config( NULL, g_spi);
		i+= 8;
	}

        while( i< len)
        {
                // get 0x70..
                while( i< len)
                {
                        if( ( msg[ i] == '0') && ( msg[ i+ 1] == 'x'))
                        {
                                strncpy( tmp, msg+ i, 6);
                                tmp[ 6]= '\0';
                                i+= 6;
                                addr= atoi( tmp);
				tx_buf[ 0]= addr >> 8;
				tx_buf[ 1]= addr & 0xff;
				spi_write( g_spi, tx_buf, 2);
                                break;
                        }
                        i++;
                }

                // get 0x72..
                while( i<= len)
                {
                        // get value, not set value
                        if( ( msg[ i] == '0') && ( msg[ i+ 1] == 'x'))
                        {
                                strncpy( tmp, msg+ i, 6);
                                tmp[ 6]= '\0';
                                i+= 6;
                                value= atoi( tmp);
				tx_buf[ 0]= value >> 8;
				tx_buf[ 1]= value & 0xff;
				spi_write( g_spi, tx_buf, 2);
                                break;
                        }

                        i++;
                }
		printk("SPI - cmd= 0x%04X, data= 0x%04X\n", addr, value);
        }

        return len;
}

static struct file_operations lcdspi_proc_fops = {
        .owner          = THIS_MODULE,
        .read           = lcdspi_proc_read,
        .write          = lcdspi_proc_write,
};

static int create_lcdspi_proc_file(void)
{
        lcdspi_proc_file = create_proc_entry( LCDSPI_PROC_FILE, 0644, NULL);
        if (!lcdspi_proc_file) {
                printk(KERN_INFO "Create proc file for LCD SPI failed\n");
                return -ENOMEM;
        }

        printk( KERN_INFO "LCD SPI proc OK\n" );
        lcdspi_proc_file->proc_fops = &lcdspi_proc_fops;
        return 0;
}

static int __init lcdc_hx8352_init(void)
{
	int ret;
	struct msm_panel_info pinfo;

	pinfo.xres = 240;
	pinfo.yres = 400;
	pinfo.type = LCDC_PANEL;
	pinfo.pdest = DISPLAY_1;
	pinfo.wait_cycle = 0;
	pinfo.bpp = 16;
	pinfo.fb_num = 2;
	pinfo.clk_rate = 6144000;
/* FIH_ADQ added by Guorui*/
	pinfo.bl_max=255;
	pinfo.bl_min=0;
/* FIH_ADQ */

	pinfo.lcdc.h_back_porch = 5;
	pinfo.lcdc.h_front_porch = 5;
	pinfo.lcdc.h_pulse_width = 5;
	pinfo.lcdc.v_back_porch = 2;
	pinfo.lcdc.v_front_porch = 0;
	pinfo.lcdc.v_pulse_width = 2;
	pinfo.lcdc.border_clr = 0;	/* blk */
	pinfo.lcdc.underflow_clr = 0xff;	/* blue */
	pinfo.lcdc.hsync_skew = 0;	

	ret = lcdc_device_register(&pinfo);
	if (ret)
		printk(KERN_ERR "%s: failed to register device!\n", __func__);

        INIT_LIST_HEAD(&dd_list);
        mutex_init(&hx8352_dd_lock);

	create_lcdspi_proc_file();

        ret = spi_register_driver(&hx8352_drv);
        return ret;
}
module_init(lcdc_hx8352_init);

static void __exit lcdc_hx8352_exit(void)
{
        spi_unregister_driver(&hx8352_drv);
}
module_exit(lcdc_hx8352_exit);


//T_FIH,ADQ,JOE HSU +++++
