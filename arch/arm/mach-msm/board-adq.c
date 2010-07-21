/* linux/arch/arm/mach-msm/board-adq.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2008-2009, Code Aurora Forum. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/bootmem.h>

#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/flash.h>
#include <asm/setup.h>

#include <asm/mach/mmc.h>
#include <mach/vreg.h>
#include <mach/mpp.h>
#include <mach/gpio.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <mach/msm_rpcrouter.h>
#include <mach/msm_hsusb.h>
#include <mach/rpc_pmapp.h>
#include <mach/rpc_hsusb.h>
#include <mach/rpc_server_handset.h>

#include <mach/msm_serial_hs.h>

#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/i2c.h>
#include <linux/android_pmem.h>
#include <mach/camera.h>
/* FIH, AudiPCHuang, 2009/03/27, { */
/* ZEUS_ANDROID_CR, I2C Configuration for Keypad Controller */
///+FIH_ADQ
#include <mach/msm_i2ckbd.h>
///-FIH_ADQ
/* } FIH, AudiPCHuang, 2009/03/27 */

#include "devices.h"
#include "socinfo.h"

#ifdef CONFIG_USB_ANDROID
#include <linux/usb/android.h>
#endif
#include "pm.h"

//FIH_ADQ,JOE HSU
#ifdef CONFIG_SPI_GPIO
#include <linux/spi/spi.h>
#include <linux/spi/spi_bitbang.h>
#include <linux/spi/spi_gpio.h>
#endif

/* FIH_ADQ, AudiPCHuang, 2009/03/30, { */
/* ZEUS_ANDROID_CR, For TC6507 LED Expander*/
///+FIH_ADQ
#include <mach/tca6507.h>
///-FIH_ADQ
/* } FIH_ADQ, AudiPCHuang, 2009/03/30 */

//added by henry.wang
#include <linux/switch.h>
// --- FIH_ADQ ---

/* FIH, SungSCLee, 2009/05/21, { */
#include "smd_private.h"
#include "proc_comm.h"
#define FIH_NV_LENGTH 4
#define FIH_ADB_DEVICE_ID_LENGTH 12
/*} FIH, SungSCLee, 2009/05/21,  */


#ifdef CONFIG_ANDROID_RAM_CONSOLE
#define MSM_RAM_CONSOLE_SIZE    128 * SZ_1K
#else
#define MSM_RAM_CONSOLE_SIZE    0
#endif

#define MSM_PMEM_AUDIO_SIZE 0x120000
/* Using upper 1/2MB of Apps Bootloader memory*/
#define MSM_PMEM_AUDIO_START_ADDR   0x80000ul

#define MSM_PMEM_MDP_SIZE	0xA00000  /* 10 MB */
#define MSM_PMEM_ADSP_SIZE	0x1200000 - MSM_RAM_CONSOLE_SIZE /* 18 MB */
#define MSM_FB_SIZE		0x100000
#define MSM_PMEM_BASE		0x00200000


#define MSM_PMEM_LIMIT		0x01F00000 // must match PHYS_OFFSET at mach/memory.h		//CHIH CHIA, change from 0x02000000 to 0x01F00000
#define MSM_PMEM_MDP_BASE	MSM_PMEM_BASE
#define MSM_PMEM_ADSP_BASE	MSM_PMEM_MDP_BASE + MSM_PMEM_MDP_SIZE 
#define MSM_RAM_CONSOLE_BASE	MSM_PMEM_ADSP_BASE + MSM_PMEM_ADSP_SIZE
#define MSM_FB_BASE		MSM_RAM_CONSOLE_BASE + MSM_RAM_CONSOLE_SIZE


#if ((MSM_FB_BASE + MSM_FB_SIZE) > MSM_PMEM_LIMIT)
#error out of PMEM boundary
#endif
/* } FIH_ADQ, Ming */

///FIH+++
#define WIFI_CONTROL_MASK   0x10000000
#define MODULE_TURN_ON      0x01
#define MODULE_TURN_OFF     0x02
static int wifi_status = 0;
static int bt_status = 0;
static DEFINE_SPINLOCK(wif_bt_lock);

#ifdef CONFIG_USB_FUNCTION
static struct usb_mass_storage_platform_data usb_mass_storage_pdata = {
	.nluns          = 0x02,
	.buf_size       = 16384,
	.vendor         = "GOOGLE",
	.product        = "Mass storage",
	.release        = 0xffff,
};

static struct platform_device mass_storage_device = {
	.name           = "usb_mass_storage",
	.id             = -1,
	.dev            = {
		.platform_data          = &usb_mass_storage_pdata,
	},
};
#endif

static struct resource smc91x_resources[] = {
	[0] = {
		.start	= 0x9C004300,
		.end	= 0x9C004400,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= MSM_GPIO_TO_INT(132),
		.end	= MSM_GPIO_TO_INT(132),
		.flags	= IORESOURCE_IRQ,
	},
};


static struct platform_device smc91x_device = {
	.name		= "smc91x",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(smc91x_resources),
	.resource	= smc91x_resources,
};

#ifdef CONFIG_USB_ANDROID
/* dynamic composition */
static struct usb_composition usb_func_composition[] = {
	{
		/* MSC */
		.product_id         = 0xC005,
		.functions          = 0x02,
		.adb_product_id     = 0xC002,
		.adb_functions      = 0x12
	},
#ifdef CONFIG_USB_F_SERIAL
	{
		/* MODEM */
		.product_id         = 0xC003,
		.functions          = 0x06,
		.adb_product_id     = 0x901E,
		.adb_functions      = 0x16,
	},
#endif
#ifdef CONFIG_USB_ANDROID_DIAG
	{
		/* DIAG */
		.product_id         = 0xC000,
		.functions          = 0x04,
		.adb_product_id     = 0x901D,
		.adb_functions      = 0x14,
	},
#endif
#if defined(CONFIG_USB_ANDROID_DIAG) && defined(CONFIG_USB_F_SERIAL)
	{
		/* DIAG + MODEM */
		.product_id         = 0x9004,
		.functions          = 0x64,
		.adb_product_id     = 0x901F,
		.adb_functions      = 0x0614,
	},
	{
		/* DIAG + MODEM + NMEA*/
		.product_id         = 0x9016,
		.functions          = 0x764,
		.adb_product_id     = 0x9020,
		.adb_functions      = 0x7614,
	},
	{
		/* DIAG + MODEM + NMEA + MSC */
		.product_id         = 0x9017,
		.functions          = 0x2764,
		.adb_product_id     = 0x9018,
		.adb_functions      = 0x27614,
	},
#endif
#ifdef CONFIG_USB_ANDROID_CDC_ECM
	{
		/* MSC + CDC-ECM */
		.product_id         = 0xC004,
		.functions          = 0x82,
		.adb_product_id     = 0xC001,
		.adb_functions      = 0x812,
	},
#endif
#ifdef CONFIG_USB_ANDROID_RMNET
	{
		/* DIAG + RMNET */
		.product_id         = 0x9021,
		.functions          = 0x94,
		.adb_product_id     = 0x9022,
		.adb_functions      = 0x914,
	},
#endif
#ifdef CONFIG_USB_ANDROID_RNDIS
	{
		/* RNDIS */
		.product_id         = 0xF00E,
		.functions          = 0xA,
		.adb_product_id     = 0x9024,
		.adb_functions      = 0x1A,
	},
#endif
};

static struct usb_mass_storage_platform_data mass_storage_pdata = {
	.nluns      = 1,
	.vendor     = "GOOGLE",
	.product    = "Mass Storage",
	.release    = 0xFFFF,
};
static struct platform_device mass_storage_device = {
	.name           = "usb_mass_storage",
	.id             = -1,
	.dev            = {
		.platform_data          = &mass_storage_pdata,
	},
};

static struct android_usb_platform_data android_usb_pdata = {
	.vendor_id      = 0x0489,
	.version        = 0x0100,
	.compositions   = usb_func_composition,
	.num_compositions = ARRAY_SIZE(usb_func_composition),
	.product_name       = "Android USB Gadget",
	.manufacturer_name = "Qualcomm Incorporated",
	.nluns = 1,
};

static struct platform_device android_usb_device = {
	.name   = "android_usb",
	.id             = -1,
	.dev            = {
		.platform_data = &android_usb_pdata,
	},
};
#endif

#ifdef CONFIG_USB_FUNCTION
static struct usb_function_map usb_functions_map[] = {
	{"mass_storage", 0},
	{"diag", 1},
	{"adb", 2},
	{"modem", 3},
	{"nmea", 4},
	{"ethernet", 5},
};

/* dynamic composition */
static struct usb_composition usb_func_composition[] = {
	// +++ FIH_ADQ +++ , added by henry.wang for FIH VID/PID
	{
		.product_id         = 0xC000,
		.functions	    = 0x0F, /* 001111 */
	},
	// 0xC001 composition is removed diag port
	{
		.product_id         = 0xC001,
		.functions	    = 0x25, /* 100101 */
	},

	// 0xC002 composition is only diag port
	{
		.product_id         = 0xC002,
		.functions	    = 0x01, /* 000001 */
	},

	// 0xC003 composition is add modem port
	{
		.product_id         = 0xC003,
		.functions	    = 0x0C, /* 001100 */
	},
	// 0xC004 Power off (mass storage) Chargering
	{
		.product_id         = 0xC004,
		.functions	    = 0x01, /* 100100 */
	},
	// 0xC005 NMEA + ADB
	{
		.product_id         = 0xC005,
		.functions	    = 0x14, /* 100100 */
	},
};

static struct msm_hsusb_platform_data msm_hsusb_pdata = {
	.version	= 0x0100,
	.phy_info	= (USB_PHY_INTEGRATED | USB_PHY_MODEL_65NM),
	.vendor_id          = 0x489,
	.product_name   = "Qualcomm HSUSB Device",
	.serial_number      = "1234567890ABCD",
	.manufacturer_name  = "Qualcomm Incorporated",
	.compositions	= usb_func_composition,
	.num_compositions = ARRAY_SIZE(usb_func_composition),
	.function_map   = usb_functions_map,
	.num_functions	= ARRAY_SIZE(usb_functions_map),
};

#endif

// +++ FIH_ADQ +++, added by henry.wang
#define SND(desc, num) { .name = #desc, .id = num }
static struct snd_endpoint snd_endpoints_list[] = {
	SND(HANDSET, 0),
	SND(HEADSET, 3),
	SND(SPEAKER, 6),
	SND(BT, 12),
	SND(HEADSET_AND_SPEAKER, 24), //FIH_ADQ ADQ.B-1224 KarenLiao: 6370 porting, Add for ADQ.B-225: [Audio] Ringtone can't be output from Headset and Speaker in Headset mode.
	SND(CURRENT, 26),
};
#undef SND

static struct msm_snd_endpoints msm_device_snd_endpoints = {
	.endpoints = snd_endpoints_list,
	.num = sizeof(snd_endpoints_list) / sizeof(struct snd_endpoint)
};

static struct platform_device msm_device_snd = {
	.name = "msm_snd",
	.id = -1,
	.dev    = {
		.platform_data = &msm_device_snd_endpoints
	},
};

#define DEC0_FORMAT ((1<<MSM_ADSP_CODEC_MP3)| \
		(1<<MSM_ADSP_CODEC_AAC)|(1<<MSM_ADSP_CODEC_WMA)| \
		(1<<MSM_ADSP_CODEC_WMAPRO)|(1<<MSM_ADSP_CODEC_AMRWB)| \
		(1<<MSM_ADSP_CODEC_AMRNB)|(1<<MSM_ADSP_CODEC_WAV)| \
		(1<<MSM_ADSP_CODEC_ADPCM)|(1<<MSM_ADSP_CODEC_YADPCM)| \
		(1<<MSM_ADSP_CODEC_EVRC)|(1<<MSM_ADSP_CODEC_QCELP))
#define DEC1_FORMAT ((1<<MSM_ADSP_CODEC_WAV)|(1<<MSM_ADSP_CODEC_ADPCM)| \
		(1<<MSM_ADSP_CODEC_YADPCM)|(1<<MSM_ADSP_CODEC_QCELP))
#define DEC2_FORMAT ((1<<MSM_ADSP_CODEC_WAV)|(1<<MSM_ADSP_CODEC_ADPCM)| \
		(1<<MSM_ADSP_CODEC_YADPCM)|(1<<MSM_ADSP_CODEC_QCELP))

#ifdef CONFIG_ARCH_MSM7X25
#define DEC3_FORMAT 0
#define DEC4_FORMAT 0
#else
#define DEC3_FORMAT ((1<<MSM_ADSP_CODEC_WAV)|(1<<MSM_ADSP_CODEC_ADPCM)| \
		(1<<MSM_ADSP_CODEC_YADPCM)|(1<<MSM_ADSP_CODEC_QCELP))
#define DEC4_FORMAT (1<<MSM_ADSP_CODEC_MIDI)
#endif

static unsigned int dec_concurrency_table[] = {
	/* Audio LP */
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DMA)), 0,
	0, 0, 0,

	/* Concurrency 1 */
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC4_FORMAT),

	/* Concurrency 2 */
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC4_FORMAT),

	/* Concurrency 3 */
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC4_FORMAT),

	/* Concurrency 4 */
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC4_FORMAT),

	/* Concurrency 5 */
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC4_FORMAT),

	/* Concurrency 6 */
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC4_FORMAT),
};

#define DEC_INFO(name, queueid, decid, nr_codec) { .module_name = name, \
	.module_queueid = queueid, .module_decid = decid, \
	.nr_codec_support = nr_codec}

static struct msm_adspdec_info dec_info_list[] = {
	DEC_INFO("AUDPLAY0TASK", 13, 0, 11), /* AudPlay0BitStreamCtrlQueue */
	DEC_INFO("AUDPLAY1TASK", 14, 1, 4),  /* AudPlay1BitStreamCtrlQueue */
	DEC_INFO("AUDPLAY2TASK", 15, 2, 4),  /* AudPlay2BitStreamCtrlQueue */
	DEC_INFO("AUDPLAY3TASK", 16, 3, 0),  /* AudPlay3BitStreamCtrlQueue */
#if 0
	DEC_INFO("AUDPLAY4TASK", 17, 4, 0),  /* AudPlay4BitStreamCtrlQueue */
#endif
};

static struct msm_adspdec_database msm_device_adspdec_database = {
	.num_dec = ARRAY_SIZE(dec_info_list),
	.num_concurrency_support = (ARRAY_SIZE(dec_concurrency_table) / \
			ARRAY_SIZE(dec_info_list)),
	.dec_concurrency_table = dec_concurrency_table,
	.dec_info_list = dec_info_list,
};

static struct platform_device msm_device_adspdec = {
	.name = "msm_adspdec",
	.id = -1,
	.dev    = {
		.platform_data = &msm_device_adspdec_database
	},
};


#ifdef CONFIG_USB_MSM_OTG_72K
static int hsusb_rpc_connect(int connect)
{
	if (connect)
		return msm_hsusb_rpc_connect();
	else
		return msm_hsusb_rpc_close();
}
#endif

#if defined(CONFIG_USB_MSM_OTG_72K) || defined(CONFIG_USB_EHCI_MSM)
static int msm_hsusb_rpc_phy_reset(void __iomem *addr)
{
	return msm_hsusb_phy_reset();
}
#endif

#ifdef CONFIG_USB_MSM_OTG_72K
static struct msm_otg_platform_data msm_otg_pdata = {
	.rpc_connect    = hsusb_rpc_connect,
	.phy_reset      = msm_hsusb_rpc_phy_reset,
	.pmic_notif_init         = msm_pm_app_rpc_init,
	.pmic_notif_deinit       = msm_pm_app_rpc_deinit,
	.pmic_register_vbus_sn   = msm_pm_app_register_vbus_sn,
	.pmic_unregister_vbus_sn = msm_pm_app_unregister_vbus_sn,
	.pmic_enable_ldo         = msm_pm_app_enable_usb_ldo,
};
#ifdef CONFIG_USB_GADGET
static struct msm_hsusb_gadget_platform_data msm_gadget_pdata;
#endif
#endif

static struct android_pmem_platform_data android_pmem_pdata = {
	.name = "pmem",
	.allocator_type = PMEM_ALLOCATORTYPE_BITMAP,
	.cached = 1,
};

static struct android_pmem_platform_data android_pmem_adsp_pdata = {
	.name = "pmem_adsp",
	.allocator_type = PMEM_ALLOCATORTYPE_BITMAP,
	.cached = 0,
};

static struct android_pmem_platform_data android_pmem_audio_pdata = {
	.name = "pmem_audio",
	.allocator_type = PMEM_ALLOCATORTYPE_BITMAP,
	.cached = 0,
};

static struct platform_device android_pmem_device = {
	.name = "android_pmem",
	.id = 0,
	.dev = { .platform_data = &android_pmem_pdata },
};

static struct platform_device android_pmem_adsp_device = {
	.name = "android_pmem",
	.id = 1,
	.dev = { .platform_data = &android_pmem_adsp_pdata },
};

static struct platform_device android_pmem_audio_device = {
	.name = "android_pmem",
	.id = 2,
	.dev = { .platform_data = &android_pmem_audio_pdata },
};

#ifdef CONFIG_SWITCH_GPIO
// +++ FIH_ADQ +++ , added by henry.wang
static struct gpio_switch_platform_data headset_sensor_device_data = {
	.name = "headset_sensor",
	.gpio = 40,
	.name_on = "",
	.name_off = "",
	.state_on = "1",
	.state_off = "0",
};

static struct platform_device headset_sensor_device = {
	.name = "switch_gpio",
	.id	= -1,
	.dev		= {
		.platform_data = &headset_sensor_device_data,
	},
};
// --- FIH_ADQ ---
#endif

#ifdef CONFIG_MSM_RPCSERVER_HANDSET
static struct msm_handset_platform_data hs_platform_data = {
    .hs_name = "7k_handset",
    .pwr_key_delay_ms = 500, /* 0 will disable end key */
};

static struct platform_device hs_device = {
    .name   = "msm-handset",
    .id     = -1,
    .dev    = {
        .platform_data = &hs_platform_data,
    },
};
#endif

#define LCDC_CONFIG_PROC          21
#define LCDC_UN_CONFIG_PROC       22
#define LCDC_API_PROG             0x30000066
//FIH_ADQ,JOE HSU
#ifdef CONFIG_SPI_GPIO
#define LCDC_API_VERS             0xAD12600D
#else
#define LCDC_API_VERS             0x450673F2
#endif

extern int android_set_sn(const char *kmessage, struct kernel_param *kp);
/* FIH, SungSCLee, 2009/05/21, { */
static int AsciiSwapChar(uint32_t fih_product_id,int p_count)
{
	char op1;
	uint32_t temp;
	int i, j = FIH_NV_LENGTH;
	char serial_number[256];

	if(p_count == FIH_ADB_DEVICE_ID_LENGTH)
		j = 2;	

	for(i = 0 ; i < j ; i++){
		if(i == 0){
			temp = fih_product_id & 0x000000ff;
		}
		else {	  		
			fih_product_id = (fih_product_id  >> 8);
			temp = fih_product_id;
			temp = temp & 0x000000ff;		
		}
		op1 = (char)temp;   		        
#ifdef CONFIG_USB_FUNCTION
		msm_hsusb_pdata.serial_number[i+p_count] = op1;
#else
		serial_number[i+p_count] = op1;
		serial_number[i+p_count+1] = '\0';
#endif
	}
#ifdef CONFIG_USB_ANDROID
	android_set_sn(serial_number,NULL);
#endif

	return 1;
}

static int msm_read_serial_number_from_nvitem(void)
{
	uint32_t smem_proc_comm_oem_cmd1 = PCOM_CUSTOMER_CMD1;
	uint32_t smem_proc_comm_oem_data1 = SMEM_PROC_COMM_OEM_PRODUCT_ID_READ;
	uint32_t smem_proc_comm_oem_data2 = NV_PRD_ID_I;
	uint32_t product_id[32];	
	int i, j = 0, nv_location;

	if(msm_proc_comm_oem_rw128b(smem_proc_comm_oem_cmd1, &smem_proc_comm_oem_data1, &smem_proc_comm_oem_data2, product_id) == 0)
	{      
		if(product_id[0] == 1){
			i = 1;
			nv_location = 1;
		}else if(product_id[0]== 0){ ///No NV_item value
			return 1;
		}else {	
			i = 0;    		
			nv_location = 0;
		}
		for(; i < FIH_NV_LENGTH + nv_location; i++){
			AsciiSwapChar(product_id[i],j);
			j = j+FIH_NV_LENGTH;
		}
	} 	
	return 1;

} 
/* } FIH, SungSCLee, 2009/05/21,  */

static struct resource msm_fb_resources[] = {
	{
		.start = MSM_FB_BASE,
		.end = MSM_FB_BASE + MSM_FB_SIZE - 1,
		.flags  = IORESOURCE_DMA,
	}
};

static struct platform_device msm_fb_device = {
	.name   = "msm_fb",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(msm_fb_resources),
	.resource       = msm_fb_resources,
};

#ifdef CONFIG_ANDROID_RAM_CONSOLE
static struct resource ram_console_resource[] = {
	{
		.start = MSM_RAM_CONSOLE_BASE,
		.end = MSM_RAM_CONSOLE_BASE + MSM_RAM_CONSOLE_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	}
};

static struct platform_device ram_console_device = {
	.name = "ram_console",
	.id = -1,
	.num_resources  = ARRAY_SIZE(ram_console_resource),
	.resource       = ram_console_resource,
};
#endif

///+++FIH_ADQ+++	godfrey
static struct resource bluesleep_resources[] = {
	{
		.name	= "gpio_host_wake",
		.start	= 42,
		.end	= 42,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "gpio_ext_wake",
		.start	= 37,
		.end	= 37,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "host_wake",
		.start	= MSM_GPIO_TO_INT(42),
		.end	= MSM_GPIO_TO_INT(42),
		.flags	= IORESOURCE_IRQ,
	},
};
///---FIH_ADQ---	godfrey

static struct platform_device msm_bluesleep_device = {
	.name = "bluesleep",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(bluesleep_resources),
	.resource	= bluesleep_resources,
};


///+++FIH_ADQ+++	godfrey
#ifdef CONFIG_AR6K
static uint32_t msm_ar6k_sdcc_setup_power(struct device *dv, unsigned int vdd)
{
	return 0;
}
//static uint32_t msm_sdcc_setup_power(struct device *dv, unsigned int vdd);
static void (*ar6k_wifi_status_cb)(int card_present, void *dev_id);
static void *ar6k_wifi_status_cb_devid;
static unsigned int  wifi_power_on = 0;

static int ar6k_wifi_status_register(void (*callback)(int card_present, void *dev_id), void *dev_id)
{
	if (ar6k_wifi_status_cb)
		return -EAGAIN;
	ar6k_wifi_status_cb = callback;
	ar6k_wifi_status_cb_devid = dev_id;
	return 0;
}

static unsigned int ar6k_wifi_status(struct device *dev)
{
	return wifi_power_on;
}


static struct mmc_platform_data ar6k_wifi_data = {
	.ocr_mask	    = MMC_VDD_28_29,
	.translate_vdd	= msm_ar6k_sdcc_setup_power,
	.status			= ar6k_wifi_status,
	.register_status_notify	= ar6k_wifi_status_register,
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
	.nonremovable   = 1,
	//.sdiowakeup_irq = 26,
#ifdef CONFIG_MMC_MSM_SDC2_DUMMY52_REQUIRED
	//.dummy52_required = 1,
#endif
	.msmsdcc_fmin   = 144000,
	.msmsdcc_fmid   = 24576000,
	.msmsdcc_fmax   = 49152000,
};
#endif

#ifdef CONFIG_BT
static struct platform_device msm_bt_power_device = {
	.name = "bt_power",
};

enum {
	BT_WAKE,
	BT_RFR,
	BT_CTS,
	BT_RX,
	BT_TX,
	BT_PCM_DOUT,
	BT_PCM_DIN,
	BT_PCM_SYNC,
	BT_PCM_CLK,
	BT_HOST_WAKE,
};

static unsigned bt_config_init[] = {
	GPIO_CFG(43, 2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),	/* RFR */
	GPIO_CFG(44, 2, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA),	/* CTS */
	GPIO_CFG(45, 2, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA),	/* Rx */
	GPIO_CFG(46, 3, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),	/* Tx */
	//GPIO_CFG(43, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), /* RFR */ 
	//GPIO_CFG(44, 1, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA), /* CTS */ 
	//GPIO_CFG(45, 1, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA), /* Rx */ 
	//GPIO_CFG(46, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), /* Tx */ 

	GPIO_CFG(36, 0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),   /* 3.3V */
	GPIO_CFG(41, 0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),   /* 1.8V */
	GPIO_CFG(34, 0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),   /* 1.2V */
	GPIO_CFG(27, 0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),   /* BT_RST */
	GPIO_CFG(37, 0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),   /* HOST_WAKE_BT */
	GPIO_CFG(42, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA),   /* BT_WAKE_HOST */
	GPIO_CFG(68, 1, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* PCM_DOUT */
	GPIO_CFG(69, 1, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA),	/* PCM_DIN */
	GPIO_CFG(70, 1, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* PCM_SYNC */
	GPIO_CFG(71, 1, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* PCM_CLK */
	GPIO_CFG(41, 0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),   /* 1.8V */
	GPIO_CFG(62, 2, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_8MA),   /* sd2 */
	GPIO_CFG(63, 2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),   /* sd2 */
	GPIO_CFG(64, 2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),   /* sd2 */
	GPIO_CFG(65, 2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),   /* sd2 */
	GPIO_CFG(66, 2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),   /* sd2 */
	GPIO_CFG(67, 2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),   /* sd2 */
	GPIO_CFG(49, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),   /* WLAN_INT_HOST */
	GPIO_CFG(96, 0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),   /* WLAN_PWD */
	GPIO_CFG(35, 0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),   /* WLAN_RESET */
	GPIO_CFG(35, 0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),   /* WLAN_RESET */
};


static void init_Bluetooth_gpio_table(void)
{
	int pin,rc;

	printk(KERN_INFO "Config Bluetooth GPIO\n");

	for (pin = 0; pin < ARRAY_SIZE(bt_config_init); pin++) {
		//printk(KERN_INFO " set gpio table entry %d\n",pin);
		rc = gpio_tlmm_config(bt_config_init[pin],GPIO_ENABLE);
		if (rc) {
			printk(KERN_ERR
					"%s: gpio_tlmm_config(%#x)=%d\n",
					__func__, bt_config_init[pin], rc);
		}
	}

	rc = gpio_request(96, "WIFI_PWD");
	if (rc)	printk(KERN_ERR "%s: WIFI_PWD 96 setting failed! rc = %d\n", __func__, rc);
	rc = gpio_request(36, "3.3V");
	if (rc)	printk(KERN_ERR "%s: 3.3V 36 setting failed! rc = %d\n", __func__, rc);
	rc = gpio_request(41, "1.8V");
	if (rc)	printk(KERN_ERR "%s: 1.8V 41 setting failed! rc = %d\n", __func__, rc);
	rc = gpio_request(34, "1.2V");
	if (rc)	printk(KERN_ERR "%s: 1.2V 34 setting failed! rc = %d\n", __func__, rc);
	rc = gpio_request(35, "WIFI_RST");
	if (rc)	printk(KERN_ERR "%s: WIFI_RST 35 setting failed! rc = %d\n", __func__, rc);
	rc = gpio_request(23, "WIFI_WARMRST");
	if (rc)	printk(KERN_ERR "%s: WIFI_WARMRST 23 setting failed! rc = %d\n", __func__, rc);
	rc = gpio_request(27, "BT_RST");
	if (rc)	printk(KERN_ERR "%s: BT_RST 27 setting failed! rc = %d\n", __func__, rc);

}





static int bluetooth_power(int on)
{
	int module_status=0,prev_status=0;
	bool bConfigWIFI;

	spin_lock(&wif_bt_lock);

	bConfigWIFI = (on & WIFI_CONTROL_MASK);

	if(bConfigWIFI)
	{
		prev_status = wifi_status;
		wifi_status = on & ~(WIFI_CONTROL_MASK); 
		if( wifi_status == prev_status )
		{
			printk(KERN_ERR "%s: WIFI already turn %s\n", __func__,  (wifi_status?"ON":"OFF") );
			spin_unlock(&wif_bt_lock);
			return 0;
		}
		if(wifi_status && !bt_status)
			module_status = MODULE_TURN_ON;
		else if(!wifi_status && !bt_status)
			module_status = MODULE_TURN_OFF;

	}else {
		prev_status = bt_status;
		bt_status = on;
		if( bt_status == prev_status )
		{
			printk(KERN_ERR "%s: BT already turn %s\n", __func__,  (bt_status?"ON":"OFF") );
			spin_unlock(&wif_bt_lock);
			return 0;
		}
		if(bt_status && !wifi_status)
			module_status = MODULE_TURN_ON;
		else if(!wifi_status && !bt_status)
			module_status = MODULE_TURN_OFF;
	}

	//power control before module on/off
	if(!bConfigWIFI &&  !bt_status) {     //Turn BT off
		printk(KERN_DEBUG "%s : Turn BT off.\n", __func__);
		gpio_direction_output(27,0);    
	}else if(!bConfigWIFI &&  bt_status){     //Turn BT on        
		printk(KERN_DEBUG "%s : Turn BT on.\n", __func__);
	}else if(bConfigWIFI && wifi_status) {  //Turn WIFI on
		printk(KERN_DEBUG "%s : Turn WIFI on.\n", __func__);
		//gpio_direction_output(23,1);
		gpio_direction_output(96,0);
		gpio_direction_output(35,0);
	}else if(bConfigWIFI && !wifi_status) {  //Turn WIFI OFF
		printk(KERN_DEBUG "%s : Turn WIFI off.\n", __func__);
#ifdef CONFIG_AR6K
		if(ar6k_wifi_status_cb) {
			wifi_power_on=0;
			ar6k_wifi_status_cb(0,ar6k_wifi_status_cb_devid);
		}else
			printk(KERN_ERR "!!!wifi_power Fail:  ar6k_wifi_status_cb_devid is NULL \n");
#else
		printk(KERN_DEBUG "%s : Driver disabled\n", __func__);
#endif

		gpio_direction_output(96,0);
		gpio_direction_output(35,0);
	}

	//Turn module on/off
	if(module_status == MODULE_TURN_ON) {   //turn module on
		printk(KERN_DEBUG "%s : Turn module(A22) on.\n", __func__);
		//FIH_ADQ.B.1741 turn on BT is too bad
		gpio_direction_output(36,1);
		//mdelay(10);
		gpio_direction_output(41,1);
		//mdelay(10);
		gpio_direction_output(34,1);
		//mdelay(10);
	}else if(module_status == MODULE_TURN_OFF) { //turn module off
		printk(KERN_DEBUG "%s : Turn module(A22) off.\n", __func__);
		gpio_direction_output(34,0);
		gpio_direction_output(41,0);
		gpio_direction_output(36,0);
	}

	if(!bConfigWIFI &&  !bt_status) {  //Turn BT off  
	}else if(!bConfigWIFI &&  bt_status){    //Turn BT on
		//FIH_ADQ.B.1741 turn on BT is too bad
		//gpio_direction_output(27,1);
		//mdelay(200);
		gpio_direction_output(27,0);
		mdelay(10);
		gpio_direction_output(27,1);
		mdelay(10);
	}else if(bConfigWIFI && wifi_status) { //Turn WIFI on
		gpio_direction_output(96,1);
		gpio_direction_output(35,1);
#ifdef CONFIG_AR6K
		if(ar6k_wifi_status_cb) {
			wifi_power_on=1;
			ar6k_wifi_status_cb(1,ar6k_wifi_status_cb_devid);
		}else
			printk(KERN_ERR "!!!wifi_power Fail:  ar6k_wifi_status_cb_devid is NULL \n");
	}else if(bConfigWIFI && !wifi_status) {  //Turn WIFI OFF        
	}
#endif

	spin_unlock(&wif_bt_lock);

	return 0;
}



static void __init bt_power_init(void) {
	msm_bt_power_device.dev.platform_data = &bluetooth_power;
}
#else
#define bt_power_init(x) do {} while (0)
#endif

///---FIH_ADQ---	godfrey

/* FIH_ADQ, AudiPCHuang, 2009/03/27, { */
/* ZEUS_ANDROID_CR, I2C Configuration for Keypad Controller */
///+FIH_ADQ
static struct msm_i2ckbd_platform_data FIH_kybd_data = {
	.gpioreset		= 82,
	.gpioirq		= 83,
	.gpio_vol_up		= 28,
	.gpio_vol_dn		= 19,
	.gpio_ring_switch	= 20,
	.gpio_hall_sensor	= 29,
	.gpio_hook_switch	= 94,
};
///-FIH_ADQ
/* } FIH_ADQ, AudiPCHuang, 2009/03/27 */

/* FIH_ADQ, AudiPCHuang, 2009/03/30, { */
/* ZEUS_ANDROID_CR, TC6507 LED Expander Platform Data */
static struct tca6507_platform_data tca6507_data = {
	.tca6507_reset = 84,
};
/* } FIH_ADQ, AudiPCHuang, 2009/03/30 */

static struct i2c_board_info i2c_devices[] = {
	// +++ FIH_ADQ +++ , backlight and led controller driver added by Teng Rui
	{
		I2C_BOARD_INFO("max8831", 0x4d),
	},
	// --- FIH_ADQ ---
	/* FIH, AudiPCHuang, 2009/03/30, { */
	/* ZEUS_ANDROID_CR, Register TC6507 LED Expander I2C Device*/
	{
		I2C_BOARD_INFO("tca6507", 0x8A >> 1),
		.platform_data = &tca6507_data,
	},
	/* } FIH, AudiPCHuang, 2009/03/30 */
	/* FIH_ADQ, AudiPCHuang, 2009/03/27, { */
	/* ZEUS_ANDROID_CR, I2C Configuration for Keypad Controller */
	///+FIH_ADQ
	{
		I2C_BOARD_INFO("stmpe1601", 0x40),
		.type		= "stmpe1601",
		.irq		= MSM_GPIO_TO_INT(83),
		.platform_data	= &FIH_kybd_data,
	},
	///-FIH_ADQ
	/* } FIH_ADQ, AudiPCHuang, 2009/03/27 */
	/* FIH_ADQ, Penho, 2009/03/13, { */
	/* ZEUS_ANDROID_CR, I2C Configuration for 1-wire brigde */
	///+FIH_ADQ
	{
		I2C_BOARD_INFO("gasgauge_bridge", 0x18),
	},
	///-FIH_ADQ
	/* } FIH_ADQ, Penho, 2009/03/13 */
	///+FIH_ADQ
	{
		I2C_BOARD_INFO("bma020", 0x38),
	},
	///-FIH_ADQ
	//FIH_ADQ ,JOE HSU
	{
		I2C_BOARD_INFO("mt9t013", 0x6C),
	},
	//   {
	//	I2C_BOARD_INFO("mt9d112", 0x78 >> 1),
	//   },
};

static uint32_t camera_off_gpio_table[] = {
	/* parallel CAMERA interfaces */
	GPIO_CFG(0,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT0 */
	GPIO_CFG(1,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT1 */
	GPIO_CFG(2,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT2 */
	GPIO_CFG(3,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT3 */
	GPIO_CFG(4,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT4 */
	GPIO_CFG(5,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT5 */
	GPIO_CFG(6,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT6 */
	GPIO_CFG(7,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT7 */
	GPIO_CFG(8,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT8 */
	GPIO_CFG(9,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT9 */
	GPIO_CFG(10, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT10 */
	GPIO_CFG(11, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT11 */
	GPIO_CFG(12, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* PCLK */
	GPIO_CFG(13, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* HSYNC_IN */
	GPIO_CFG(14, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* VSYNC_IN */
	GPIO_CFG(15, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), /* MCLK */
};

static uint32_t camera_on_gpio_table[] = {
	/* parallel CAMERA interfaces */
	//FIH_ADQ ,JOE HSU
#if 1
	GPIO_CFG(0,  1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_2MA), /* DAT0 */
	GPIO_CFG(1,  1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_2MA), /* DAT1 */
	GPIO_CFG(2,  1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_2MA), /* DAT2 */
#else
	GPIO_CFG(0,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT0 */
	GPIO_CFG(1,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT1 */
	GPIO_CFG(2,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT2 */
#endif
	GPIO_CFG(3,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT3 */
	GPIO_CFG(4,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT4 */
	GPIO_CFG(5,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT5 */
	GPIO_CFG(6,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT6 */
	GPIO_CFG(7,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT7 */
	GPIO_CFG(8,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT8 */
	GPIO_CFG(9,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT9 */
	GPIO_CFG(10, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT10 */
	GPIO_CFG(11, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT11 */
	GPIO_CFG(12, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_16MA), /* PCLK */
	GPIO_CFG(13, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* HSYNC_IN */
	GPIO_CFG(14, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* VSYNC_IN */
	GPIO_CFG(15, 1, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_16MA), /* MCLK */
};

static void config_gpio_table(uint32_t *table, int len)
{
	int n, rc;
	for (n = 0; n < len; n++) {
		rc = gpio_tlmm_config(table[n], GPIO_ENABLE);
		if (rc) {
			printk(KERN_ERR "%s: gpio_tlmm_config(%#x)=%d\n",
					__func__, table[n], rc);
			break;
		}
	}
}

static void config_camera_on_gpios(void)
{
	config_gpio_table(camera_on_gpio_table,
			ARRAY_SIZE(camera_on_gpio_table));
}

static void config_camera_off_gpios(void)
{
	config_gpio_table(camera_off_gpio_table,
			ARRAY_SIZE(camera_off_gpio_table));
}
//#define MSM_PROBE_INIT(name) name##_probe_init
//FIH_ADQ,JOE HSU
static struct msm_camera_sensor_info msm_camera_sensor[] = {
	{
		.sensor_reset = 0,
		.sensor_pwd   = 17,
		.vcm_pwd      = 0,
		.sensor_name  = "mt9t013",
		//		.flash_type		= MSM_CAMERA_FLASH_NONE,
		//		.sensor_probe = MSM_PROBE_INIT(mt9t013),
	},
};
//#undef MSM_PROBE_INIT
static struct msm_camera_device_platform_data msm_camera_device_data = {
	.camera_gpio_on  = config_camera_on_gpios,
	.camera_gpio_off = config_camera_off_gpios,
	//	.snum = 1,//2,
	//	.sinfo = &msm_camera_sensor[0],
	.snum = ARRAY_SIZE(msm_camera_sensor),
	.sinfo = &msm_camera_sensor[0],
	.ioext.mdcphy = MSM_MDC_PHYS,
	.ioext.mdcsz  = MSM_MDC_SIZE,
	.ioext.appphy = MSM_CLK_CTL_PHYS,
	.ioext.appsz  = MSM_CLK_CTL_SIZE,
};

static void __init msm_camera_add_device(void)
{
	msm_camera_register_device(NULL, 0, &msm_camera_device_data);
	config_camera_off_gpios();
}

//FIH_ADQ,JOE HSU -----
#ifdef CONFIG_SPI_GPIO
static unsigned spi_gpio_config_input[] = {
	GPIO_CFG(101, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),  /* clk */
	GPIO_CFG(102, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),  /* cs */
	GPIO_CFG(131, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),  /* miso */
	GPIO_CFG(132, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),  /* mosi */
};


static int __init spi_gpio_init(void)
{
	int rc = 0, pin;

	for (pin = 0; pin < ARRAY_SIZE(spi_gpio_config_input); pin++) {
		rc = gpio_tlmm_config(spi_gpio_config_input[pin], GPIO_ENABLE);
		if (rc) {
			printk(KERN_ERR
					"%s: gpio_tlmm_config(%#x)=%d\n",
					__func__, spi_gpio_config_input[pin], rc);
			return -EIO;
		}
	}

	rc = gpio_tlmm_config(GPIO_CFG(85, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
	if (rc) {
		printk(KERN_ERR
				"%s--%d: gpio_tlmm_config=%d\n",
				__func__,__LINE__, rc);
		return -EIO;
	}
	rc = gpio_tlmm_config(GPIO_CFG(103, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
	if (rc) {
		printk(KERN_ERR
				"%s--%d: gpio_tlmm_config=%d\n",
				__func__,__LINE__, rc);
		return -EIO;
	}

	rc = gpio_request(85, "cam_pwr");
	if (rc){
		printk(KERN_ERR "%s: cam_pwr setting failed! rc = %d\n", __func__, rc);
		return -EIO;
	}

	gpio_direction_output(85,1);
	printk(KERN_INFO "%s: (85) gpio_read = %d\n", __func__, gpio_get_value(85));
	gpio_free(85);

	/* FIH_ADQ, Ming { */
	/* Do not reset lcd for keeping bootloader image displaying */
	/*
	   rc = gpio_request(103, "lcd_reset");
	   if (rc){
	   printk(KERN_ERR "%s: lcd_reset setting failed! rc = %d\n", __func__, rc);
	   return -EIO;
	   }

	   gpio_direction_output(103,1);
	   mdelay(500);
	   printk(KERN_INFO "%s: (103) gpio_read = %d\n", __func__, gpio_get_value(103));

	   gpio_direction_output(103,0);
	   mdelay(500);
	   printk(KERN_INFO "%s: (103) gpio_read = %d\n", __func__, gpio_get_value(103));

	   gpio_direction_output(103,1);
	   mdelay(50);    	
	   printk(KERN_INFO "%s: (103) gpio_read = %d\n", __func__, gpio_get_value(103));

	   gpio_free(103);
	   */
	/* } FIH_ADQ, Ming */

	rc = gpio_request(101, "gpio_spi");
	if (rc){
		printk(KERN_ERR "%s: msm spi setting failed! rc = %d\n", __func__, rc);
		return -EIO;
	}
	rc = gpio_request(102, "gpio_spi");
	if (rc){
		printk(KERN_ERR "%s: msm spi setting failed! rc = %d\n", __func__, rc);
		return -EIO;
	}
	rc = gpio_request(131, "gpio_spi");
	if (rc){
		printk(KERN_ERR "%s: msm spi setting failed! rc = %d\n", __func__, rc);
		return -EIO;
	}
	rc = gpio_request(132, "gpio_spi");
	if (rc){
		printk(KERN_ERR "%s: msm spi setting failed! rc = %d\n", __func__, rc);
		return -EIO;
	}

	gpio_direction_output(101,1);
	printk(KERN_INFO "%s: (101) gpio_read = %d\n", __func__, gpio_get_value(101));
	gpio_direction_output(102,1);
	printk(KERN_INFO "%s: (102) gpio_read = %d\n", __func__, gpio_get_value(102));
	gpio_direction_input(131);
	gpio_direction_output(132,1);
	printk(KERN_INFO "%s: (132)gpio_read = %d\n", __func__, gpio_get_value(132));

	gpio_free(101);
	gpio_free(102);
	gpio_free(131);
	gpio_free(132);               

	return rc;
}

static struct spi_board_info lcdc_spi_devices[] = {

	{
		.modalias = "lcdc_spi",
		.max_speed_hz = 6000,
		.chip_select = 0,
		.controller_data = (void *) 102,
		.mode           = SPI_MODE_2,
	},
};

struct spi_gpio_platform_data lcdc_spigpio_platform_data = {

	.sck = 101,
	.mosi = 132,
	.miso = 131,
	.num_chipselect = 1,
};

static struct platform_device lcdc_spigpio_device = {

	.name = "spi_gpio",
	.dev = {
		.platform_data = &lcdc_spigpio_platform_data,
	},
};
#endif
//FIH_ADQ,JOE HSU +++++

/* FIH_ADQ, AudiPCHuang, 2009/04/02, { */
/* ZEUS_ANDROID_CR, Vibrator Device Structre */
///+FIH_ADQ
/*static struct platform_device pmic_rpc_device = {
	.name	= "pmic_rpc",
	.id		= -1,
};*/
///-FIH_ADQ
/* } FIH_ADQ, AudiPCHuang, 2009/04/02 */

static struct platform_device *devices[] __initdata = {
#ifdef CONFIG_ANDROID_RAM_CONSOLE
	&ram_console_device,
#endif
#if !defined(CONFIG_MSM_SERIAL_DEBUGGER)
	&msm_device_uart3,
#endif
	&msm_device_smd,
	&msm_device_dmov,
	&msm_device_nand,
#ifdef CONFIG_USB_MSM_OTG_72K
	&msm_device_otg,
#ifdef CONFIG_USB_GADGET
	&msm_device_gadget_peripheral,
#endif
#endif
#ifdef CONFIG_USB_FUNCTION
	&msm_device_hsusb_peripheral,
	&mass_storage_device,
#endif

#ifdef CONFIG_USB_ANDROID
	&mass_storage_device,
	&android_usb_device,
#endif
	&msm_device_i2c,
	&smc91x_device,
	&msm_device_tssc,
	&android_pmem_device,
	&android_pmem_adsp_device,
	&android_pmem_audio_device,
	//FIH_ADQ,JOE HSU 
#ifdef CONFIG_SPI_GPIO
	&lcdc_spigpio_device,
#endif		
	&msm_fb_device,
	///+++FIH_ADQ+++	godfrey
#ifdef CONFIG_BT
	&msm_bt_power_device,
#endif
	///---FIH_ADQ---	godfrey
	&msm_device_uart_dm1,
	&msm_bluesleep_device,
	// +++ FIH_ADQ +++ , added by henry.wang
#ifdef CONFIG_SWITCH_GPIO
	&headset_sensor_device,
#endif
#ifdef CONFIG_MSM_RPCSERVER_HANDSET
    &hs_device,
#endif
	// --- FIH_ADQ ---
	/* FIH_ADQ, AudiPCHuang, 2009/04/02, { */
	/* ZEUS_ANDROID_CR, Vibrator Device Structre */
	///+FIH_ADQ
	//&pmic_rpc_device,
	// added by henry.wang
	&msm_device_snd,
	&msm_device_adspdec,
	///-FIH_ADQ
	/* } FIH_ADQ, AudiPCHuang, 2009/04/02 */
};

static void __init msm_fb_add_devices(void)
{
	msm_fb_register_device("mdp", 0);
	msm_fb_register_device("pmdh", 0);
	msm_fb_register_device("lcdc", 0);
	/* FIH_ADQ, Penho, 2009/03/13, { */
	/* ZEUS_ANDROID_CR, register device for Battery Report */
	///+FIH_ADQ
	msm_fb_register_device("batt", 0);
	///-FIH_ADQ
	/* } FIH_ADQ, Penho, 2009/03/13 */
}

extern struct sys_timer msm_timer;

static void __init msm7x25_init_irq(void)
{
	msm_init_irq();
}

static struct msm_acpu_clock_platform_data msm7x25_clock_data = {
	.acpu_switch_time_us = 50,
	.max_speed_delta_khz = 256000,
	.vdd_switch_time_us = 62,
	.max_axi_khz = 128000,
};

void msm_serial_debug_init(unsigned int base, int irq,
		struct device *clk_device, int signal_irq);

static void sdcc_gpio_init(void)
{
	/* SDC1 GPIOs */
#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
	if (gpio_request(51, "sdc1_data_3"))
		pr_err("failed to request gpio sdc1_data_3\n");
	if (gpio_request(52, "sdc1_data_2"))
		pr_err("failed to request gpio sdc1_data_2\n");
	if (gpio_request(53, "sdc1_data_1"))
		pr_err("failed to request gpio sdc1_data_1\n");
	if (gpio_request(54, "sdc1_data_0"))
		pr_err("failed to request gpio sdc1_data_0\n");
	if (gpio_request(55, "sdc1_cmd"))
		pr_err("failed to request gpio sdc1_cmd\n");
	if (gpio_request(56, "sdc1_clk"))
		pr_err("failed to request gpio sdc1_clk\n");
#endif

	/* SDC2 GPIOs */
#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT
	if (gpio_request(62, "sdc2_clk"))
		pr_err("failed to request gpio sdc2_clk\n");
	if (gpio_request(63, "sdc2_cmd"))
		pr_err("failed to request gpio sdc2_cmd\n");
	if (gpio_request(64, "sdc2_data_3"))
		pr_err("failed to request gpio sdc2_data_3\n");
	if (gpio_request(65, "sdc2_data_2"))
		pr_err("failed to request gpio sdc2_data_2\n");
	if (gpio_request(66, "sdc2_data_1"))
		pr_err("failed to request gpio sdc2_data_1\n");
	if (gpio_request(67, "sdc2_data_0"))
		pr_err("failed to request gpio sdc2_data_0\n");
#endif

	/* SDC3 GPIOs */
#ifdef CONFIG_MMC_MSM_SDC3_SUPPORT
	if (gpio_request(88, "sdc3_clk"))
		pr_err("failed to request gpio sdc3_clk\n");
	if (gpio_request(89, "sdc3_cmd"))
		pr_err("failed to request gpio sdc3_cmd\n");
	if (gpio_request(90, "sdc3_data_3"))
		pr_err("failed to request gpio sdc3_data_3\n");
	if (gpio_request(91, "sdc3_data_2"))
		pr_err("failed to request gpio sdc3_data_2\n");
	if (gpio_request(92, "sdc3_data_1"))
		pr_err("failed to request gpio sdc3_data_1\n");
	if (gpio_request(93, "sdc3_data_0"))
		pr_err("failed to request gpio sdc3_data_0\n");
#endif

	/* SDC4 GPIOs */
#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
	/* ZEUS_ANDROID_CR, GPIO 19 and 20 are used for VOLUME Key and ringer switch now. Disable SDC4*/
#ifndef CONFIG_KEYBOARD_STMPE1601
	if (gpio_request(19, "sdc4_data_3"))
		pr_err("failed to request gpio sdc4_data_3\n");
	if (gpio_request(20, "sdc4_data_2"))
		pr_err("failed to request gpio sdc4_data_2\n");
	if (gpio_request(21, "sdc4_data_1"))
		pr_err("failed to request gpio sdc4_data_1\n");
	if (gpio_request(107, "sdc4_cmd"))
		pr_err("failed to request gpio sdc4_cmd\n");
	if (gpio_request(108, "sdc4_data_0"))
		pr_err("failed to request gpio sdc4_data_0\n");
	if (gpio_request(109, "sdc4_clk"))
		pr_err("failed to request gpio sdc4_clk\n");
#endif
#endif
}

static unsigned sdcc_cfg_data[][6] = {
	/* SDC1 configs */
	{
		GPIO_CFG(51, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		GPIO_CFG(52, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		GPIO_CFG(53, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		GPIO_CFG(54, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		GPIO_CFG(55, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		GPIO_CFG(56, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),
	},
	/* SDC2 configs */
	{
		GPIO_CFG(62, 2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),
		GPIO_CFG(63, 2, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		GPIO_CFG(64, 2, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		GPIO_CFG(65, 2, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		GPIO_CFG(66, 2, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		GPIO_CFG(67, 2, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	},
	//+[FIH_ADQ][IssueKeys:ADQ.B-617]	
#if 0	
	/* SDC3 configs */
	{
		GPIO_CFG(88, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),
		GPIO_CFG(89, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		GPIO_CFG(90, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		GPIO_CFG(91, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		GPIO_CFG(92, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		GPIO_CFG(93, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	},
#endif	
	//-[FIH_ADQ][IssueKeys:ADQ.B-617]
	/* SDC4 configs */
	/* FIH_ADQ, AudiPCHuang, 2009/03/27, { */
	/* ZEUS_ANDROID_CR, GPIO 19 and 20 are used for VOLUME Key and ringer switch now. Disable SDC4*/
	///+FIH_ADQ
#ifndef CONFIG_KEYBOARD_STMPE1601
	{
		GPIO_CFG(19, 3, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		GPIO_CFG(20, 3, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		GPIO_CFG(21, 4, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		GPIO_CFG(107, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		GPIO_CFG(108, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		GPIO_CFG(109, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),
	}
#endif
	///-FIH_ADQ
	/* } FIH_ADQ, AudiPCHuang, 2009/03/27 */
};

static unsigned long vreg_sts, gpio_sts;
static struct vreg *vreg_mmc;

static void msm_sdcc_setup_gpio(int dev_id, unsigned int enable)
{
	int i, rc;

	if (!(test_bit(dev_id, &gpio_sts)^enable))
		return;

	if (enable)
		set_bit(dev_id, &gpio_sts);
	else
		clear_bit(dev_id, &gpio_sts);

	for (i = 0; i < ARRAY_SIZE(sdcc_cfg_data[dev_id - 1]); i++) {
		rc = gpio_tlmm_config(sdcc_cfg_data[dev_id - 1][i],
				enable ? GPIO_ENABLE : GPIO_DISABLE);
		if (rc)
			printk(KERN_ERR "%s: gpio_tlmm_config(%#x)=%d\n",
					__func__, sdcc_cfg_data[dev_id - 1][i], rc);
	}
}


static uint32_t msm_sdcc_setup_power(struct device *dv, unsigned int vdd)
{
	int rc = 0;
	struct platform_device *pdev;

	pdev = container_of(dv, struct platform_device, dev);
	msm_sdcc_setup_gpio(pdev->id, !!vdd);

	if (vdd == 0) {
		if (!vreg_sts)
			return 0;

		clear_bit(pdev->id, &vreg_sts);

		if (!vreg_sts) {
			rc = vreg_disable(vreg_mmc);
			if (rc)
				printk(KERN_ERR "%s: return val: %d \n",
						__func__, rc);
		}
		return 0;
	}

	if (!vreg_sts) {
		rc = vreg_set_level(vreg_mmc, 2850);
		if (!rc)
			rc = vreg_enable(vreg_mmc);
		if (rc)
			printk(KERN_ERR "%s: return val: %d \n",
					__func__, rc);
	}
	set_bit(pdev->id, &vreg_sts);
	return 0;

}

#define GPIO_CARDDETECT_INTR    18

static unsigned int msm_sdcc_card_detect(struct device * dev_sdcc1)
{
	int rc= 0;
	gpio_request(GPIO_CARDDETECT_INTR,0);
	rc = gpio_get_value(GPIO_CARDDETECT_INTR);
	gpio_free(GPIO_CARDDETECT_INTR);  
	printk(KERN_INFO"%s: SD card detect (%d)\n",__func__, rc);      
	return rc;
}

static struct mmc_platform_data msm7x25_sdcc_data = {
	.ocr_mask	= MMC_VDD_28_29,
	.translate_vdd	= msm_sdcc_setup_power,
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
	.status_irq = MSM_GPIO_TO_INT(GPIO_CARDDETECT_INTR),
	.irq_flags = IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING,
	.status         = msm_sdcc_card_detect,
	.msmsdcc_fmin   = 144000,
	.msmsdcc_fmid   = 24576000,
	.msmsdcc_fmax   = 49152000,
	.nonremovable   = 0,
};

static void __init msm7x25_init_mmc(void)
{
	vreg_mmc = vreg_get(NULL, "mmc");
	if (IS_ERR(vreg_mmc)) {
		printk(KERN_ERR "%s: vreg get failed (%ld)\n",
				__func__, PTR_ERR(vreg_mmc));
		return;
	}
	sdcc_gpio_init();

	gpio_tlmm_config( GPIO_CFG( GPIO_CARDDETECT_INTR, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA ), GPIO_ENABLE );

#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
	msm_add_sdcc(1, &msm7x25_sdcc_data);
#endif

#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT
#ifdef CONFIG_AR6K
	msm_add_sdcc(2, &ar6k_wifi_data);
#else
	msm_add_sdcc(2, &msm7x25_sdcc_data);
#endif
#endif
#ifdef CONFIG_MMC_MSM_SDC3_SUPPORT
	msm_add_sdcc(3, &msm7x25_sdcc_data);
#endif
#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
	msm_add_sdcc(4, &msm7x25_sdcc_data);
#endif
}

static struct msm_pm_platform_data msm7x25_pm_data[MSM_PM_SLEEP_MODE_NR] = {
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE].latency = 16000,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN].latency = 12000,
	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].latency = 2000,
};

	static void
msm_i2c_gpio_config(int iface, int config_type)
{
	int gpio_scl = 60;
	int gpio_sda = 61;

	if (iface) { return; };

	if (config_type) {
		gpio_tlmm_config(GPIO_CFG(gpio_scl, 1, GPIO_INPUT,
					GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
		gpio_tlmm_config(GPIO_CFG(gpio_sda, 1, GPIO_INPUT,
					GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
	} else {
		gpio_tlmm_config(GPIO_CFG(gpio_scl, 0, GPIO_OUTPUT,
					GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
		gpio_tlmm_config(GPIO_CFG(gpio_sda, 0, GPIO_OUTPUT,
					GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
	}
}

static struct msm_i2c_platform_data msm_i2c_pdata = {
	.clk_freq = 100000,
	.rmutex = 0,
	.msm_i2c_config_gpio = msm_i2c_gpio_config,
};

static void __init msm_device_i2c_init(void)
{
	if (gpio_request(60, "i2c_pri_clk"))
		pr_err("failed to request gpio i2c_pri_clk\n");
	if (gpio_request(61, "i2c_pri_dat"))
		pr_err("failed to request gpio i2c_pri_dat\n");

	msm_i2c_pdata.pm_lat =
		msm7x25_pm_data[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN]
		.latency;

	msm_device_i2c.dev.platform_data = &msm_i2c_pdata;
}

///--- FIH_ADQ --- 6360

// +++ FIH_ADQ +++ , added by henry.wang

#ifdef CONFIG_SWITCH_GPIO
static void __init init_headset_sensor(void)
{
	gpio_direction_input(40);
}
#endif

// --- FIH_ADQ ---

/* FIH_ADQ, AudiPCHuang, 2009/03/30, { */
/* ZEUS_ANDROID_CR, Create proc entry for reading device information*/
///+FIH_ADQ
extern void adq_info_init(void);
void msm_init_pmic_vibrator(void);
///-FIH_ADQ
/* } FIH_ADQ, AudiPCHuang, 2009/03/30 */

static void __init msm7x25_init(void)
{
	if (socinfo_init() < 0)
		BUG();
	msm_clock_init(msm_clocks_7x25, msm_num_clocks_7x25);
#ifdef CONFIG_AR6K
	ar6k_wifi_status_cb=NULL; 
	ar6k_wifi_status_cb_devid=NULL;
#endif

#if defined(CONFIG_MSM_SERIAL_DEBUGGER)
	msm_serial_debug_init(MSM_UART3_PHYS, INT_UART3,
			&msm_device_uart3.dev, 1);
#endif
	msm_acpu_clock_init(&msm7x25_clock_data);

	msm_read_serial_number_from_nvitem();
#ifdef CONFIG_USB_FUNCTION
	msm_device_hsusb_peripheral.dev.platform_data = &msm_hsusb_pdata;
	msm_device_hsusb_host.dev.platform_data = &msm_hsusb_pdata;
#endif

#ifdef CONFIG_USB_MSM_OTG_72K
	msm_device_otg.dev.platform_data = &msm_otg_pdata;
#ifdef CONFIG_USB_GADGET
	msm_gadget_pdata.swfi_latency =
		msm7x25_pm_data
		[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].latency;
	msm_device_gadget_peripheral.dev.platform_data = &msm_gadget_pdata;
#endif
#endif

#ifdef CONFIG_SPI_GPIO
	spi_gpio_init();
#endif	
	platform_add_devices(devices, ARRAY_SIZE(devices));
	msm_device_i2c_init();
	i2c_register_board_info(0, i2c_devices, ARRAY_SIZE(i2c_devices));
	//FIH_ADQ,JOE HSU 
#ifdef CONFIG_SPI_GPIO
	spi_register_board_info(lcdc_spi_devices,ARRAY_SIZE(lcdc_spi_devices));
#endif		
	msm_fb_add_devices();
	msm_camera_add_device();

	msm7x25_init_mmc();

	// +++ FIH_ADQ +++ , added by henry.wang
#ifdef CONFIG_SWITCH_GPIO
	init_headset_sensor();
#endif
	// --- FIH_ADQ ---

	///+++FIH_ADQ+++ godfrey
	init_Bluetooth_gpio_table();
	bt_power_init();
	///---FIH_ADQ--- godfrey

	/* FIH_ADQ, AudiPCHuang, 2009/03/30, { */
	/* ZEUS_ANDROID_CR, Create proc entry for reading device information*/
	///+FIH_ADQ
	msm_init_pmic_vibrator();
	adq_info_init();
	///-FIH_ADQ
	/* } FIH_ADQ, AudiPCHuang, 2009/03/30 */	

	msm_pm_set_platform_data(msm7x25_pm_data,ARRAY_SIZE(msm7x25_pm_data));

}

static void __init msm_msm7x25_allocate_memory_regions(void)
{
	//void *addr, *addr_1m_aligned;
	unsigned long size;

	/* FIH_ADQ, Ming { */
	size = MSM_PMEM_MDP_SIZE;
	///	addr = alloc_bootmem(size);
	///	android_pmem_pdata.start = __pa(addr);
	android_pmem_pdata.start = MSM_PMEM_MDP_BASE;
	android_pmem_pdata.size = size;
	///	printk(KERN_INFO "allocating %lu bytes at %p (%lx physical)"
	///	       "for pmem\n", size, addr, __pa(addr));
	printk(KERN_INFO "allocating %lu bytes at ???? (%lx physical)"
			"for pmem\n", size, (unsigned long)MSM_PMEM_MDP_BASE);
	/* } FIH_ADQ, Ming */

	/* FIH_ADQ, Ming { */	       
	size = MSM_PMEM_ADSP_SIZE;
	///	addr = alloc_bootmem(size);
	///	android_pmem_adsp_pdata.start = __pa(addr);
	android_pmem_adsp_pdata.start = MSM_PMEM_ADSP_BASE;
	android_pmem_adsp_pdata.size = size;
	///	printk(KERN_INFO "allocating %lu bytes at %p (%lx physical)"
	///	       "for adsp pmem\n", size, addr, __pa(addr));
	printk(KERN_INFO "allocating %lu bytes at ???? (%lx physical)"
			"for adsp pmem\n", size, (unsigned long)MSM_PMEM_ADSP_BASE);
	/* } FIH_ADQ, Ming */

	size = MSM_PMEM_AUDIO_SIZE ;
	android_pmem_audio_pdata.start = MSM_PMEM_AUDIO_START_ADDR ;
	android_pmem_audio_pdata.size = size;
	pr_info("allocating %lu bytes (at %lx physical) for audio "
			"pmem arena\n", size , MSM_PMEM_AUDIO_START_ADDR);

}

static void __init msm7x25_map_io(void)
{
	msm_map_common_io();
	msm_msm7x25_allocate_memory_regions();

}

MACHINE_START(MSM7X25_SURF, "QCT MSM7x25 SURF")
#ifdef CONFIG_MSM_DEBUG_UART
.phys_io        = MSM_DEBUG_UART_PHYS,
	.io_pg_offst    = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params	= PHYS_OFFSET + 0x100,
	.map_io		= msm7x25_map_io,
	.init_irq	= msm7x25_init_irq,
	.init_machine	= msm7x25_init,
	.timer		= &msm_timer,
	MACHINE_END

