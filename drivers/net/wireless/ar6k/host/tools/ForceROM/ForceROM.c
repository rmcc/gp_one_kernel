#ifndef __KERNEL__
#  define __KERNEL__
#endif
#ifndef MODULE
#  define MODULE
#endif

//#define OMAP2420_SPI //define in the  Makefile
#define MODU 1
#define CHAN 2
enum CLKDIVIDER{CLK48M=0,CLK24M=1,CLK12M=2,CLK6M=3,CLK3M=4,CLK1M5=5,CLKM75=6};
#define CLK_FREQ CLK12M
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/config.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <asm/arch/hardware.h>
#ifdef OMAP2420_SPI
#include <asm/arch/gpio.h>
#endif
#include <asm/io.h>
#include <asm/dma.h>
#include "gspi.h"
#include "hw/rtc_reg.h"
#include "hw/gpio_reg.h"
#include "hw/mc_reg.h"
#include "hw/mbox_reg.h"
#define AR6000_SPI_STATUS_ADDRESS 0x0
#define AR6000_SPI_DMA_ADDR_ADDRESS 0x1000
#define AR6000_SPI_DMA_CNT_ENA_ADDRESS 0x2000
#define AR6000_SPI_CONFIG_ADDRESS 0x3000
#define AR6000_SPI_LOCAL_BUS_ADDRESS 0x470
#define AR6000_HOST_INTERFACE_SCRATCH_ADDRESS 0x460

#define STATUS_DMA_OVER_MASK 0x20 
#define STATUS_DMA_OVER_MASK_16BIT 0x2020 
#define LOCAL_BUS_IO_ENABLE_MASK 0x8
#define LOCAL_BUS_IO_ENABLE_16BIT_MASK 0x0808
#define LOCAL_BUS_KEEP_AWAKE_MASK 0x4
#define LOCAL_BUS_KEEP_AWAKE_16BIT_MASK 0x0404
#define CONFIG_BIGENDIAN 0x80
#define CONFIG_SPI_RESET 0x10
#define CONFIG_SPI_DATA_SIZE 0x3
#define CONFIG_SPI_DATA_SIZE8 0x0
#define CONFIG_SPI_DATA_SIZE16 0x1
#define CONFIG_SPI_DATA_SIZE32 0x2
 
#define SPI_READ_OPERATION 0x8000
#define SPI_INTERNAL_ADDRESS 0x4000
#define SPI_ERR_READY_MASK 0x1f
#define SPI_READY_MASK 0x1
#define SPI_READY_MASK_16BIT 0x101
#define WINDOW_READ_ADDR_ADDRESS 0x47c 
#define WINDOW_WRITE_ADDR_ADDRESS 0x478
#define WINDOW_DATA_ADDRESS 0x474
static u32 bConfigRegFirstAccess=1;
const u32 DATA_SIZE_MASK[3]={0xff,0xffff,0xffffffff};
u32 MCSPI1_VIRTUAL_BASE_ADDRESS;
u32 MCSPI2_VIRTUAL_BASE_ADDRESS;
u32 OMAP_CONTROL_PADCONF_VIRTUAL_BASE_ADDRESS; 

int d0swap=0;
MODULE_PARM(d0swap,"i");
MODULE_PARM_DESC(d0swap,"SPI D0 pin swap");
int debuglvl=1;
MODULE_PARM(debuglvl,"i");
MODULE_PARM_DESC(debuglvl,"Debug Level");
#define DBG(x...) do{   \
                        if(debuglvl>0){\
                            printk(x); \
                        } \
                        else{\
                            mdelay(2);\
                        }\
                    }while(0)
int ar_spim_init(void)
{
    u32 v = 0;
#ifdef OMAP5912_SPI
    OMAP_SPI_RD32(FUNC_MUX_CTRL_7, v); // Init SPIF pins
    v |= REG7_MASK;
    OMAP_SPI_WR32(FUNC_MUX_CTRL_7, v);

    OMAP_SPI_RD32(FUNC_MUX_CTRL_8, v);
    v |= REG8_MASK;
    OMAP_SPI_WR32(FUNC_MUX_CTRL_8, v);

    OMAP_SPI_WR32(OMAP_SPI_SCR_ADDR, SOFTRESET);
    mdelay(1);
    OMAP_SPI_WR32(OMAP_SPI_SET1_ADDR,EN_CLK);
    
    OMAP_SPI_WR32(OMAP_SPI_SET2_ADDR, MASTER_MODE);
#else
    
    OMAP_SPI_WR32(MODU,OMAP_MCSPI_SYSCONFIG,0x2);//reset SPI
    OMAP_SPI_RD32(MODU,OMAP_MCSPI_SYSSTATUS,v);
    while(!(v&0x1))
    {
        udelay(1);
	OMAP_SPI_RD32(MODU,OMAP_MCSPI_SYSSTATUS,v);
    }
    v=readl(OMAP_CONTROL_PADCONF_VIRTUAL_BASE_ADDRESS+0x0104);
    v&=0xffffff00;
    v|=0x18;//SPI1_CS2 IO configuration
    writel(v,OMAP_CONTROL_PADCONF_VIRTUAL_BASE_ADDRESS+0x0104);
    udelay(10);
    OMAP_SPI_WR32(MODU,OMAP_MCSPI_MODULCTRL,0);//Functional,Master,multiple channel
    OMAP_SPI_WR32(MODU,OMAP_MCSPI_CHCTRL(CHAN),MCSPI_CHCTRL_EN);	
    v=0;
    if(!d0swap)
    {
        v&=~MCSPI_CHCONF_IS;	//Clear Input Select bit,to select SOMI for input
	v&=~MCSPI_CHCONF_DPE1;  //data line1(SIMO) selected for transmission
	v|=MCSPI_CHCONF_DPE0;   //no transmission on line0(SOMI)
    }
    else
    {
        v|=MCSPI_CHCONF_IS;	
        v|=MCSPI_CHCONF_DPE1;
	v&=~MCSPI_CHCONF_DPE0;
    }
    v&=~MCSPI_CHCONF_WL(32);
    v|=MCSPI_CHCONF_WL(16);//default 16 bit mode
    v|=MCSPI_CHCONF_EPOL;		//SPI.CS held low during active state
    v|=MCSPI_CHCONF_CLKD(CLK_FREQ);	//
    v&=~MCSPI_CHCONF_POL;		//SPI.CLK is held high during the active state
    v&=~MCSPI_CHCONF_PHA;		//data are latched on the odd numbered edges
    DBG("CONF:%08X\n",v); 
    OMAP_SPI_WR32(MODU,OMAP_MCSPI_CHCONF(CHAN),v);
#endif    
    return 0;

}

EXPORT_SYMBOL(ar_spim_init);

int ar_spim_txw(u32 mode,u32 data)
{
u32 cfg=0;
u32 val=0;
u32 c=0;
#ifdef OMAP5912_SPI
    cfg=AD(EDEV1)|RD|WORD_LENGTH_CONF[mode]; //Enable device 1,Read/Write process activated
    val=data&DATA_SIZE_MASK[mode];
    OMAP_SPI_WR32(OMAP_SPI_TX_ADDR, val);
    OMAP_SPI_WR32(OMAP_SPI_CTRL_ADDR, cfg);
    OMAP_SPI_RD32(OMAP_SPI_DSR_ADDR, val);
    while (!(val & OMAP_SPI_DSR_TX_EMPTY)) 
    {
        if (c > 0x2000) 
        {
            DBG("Err: %s() No WR End Intr \n", __FUNCTION__);
            return -1;
        }
        c++;
        OMAP_SPI_RD32(OMAP_SPI_DSR_ADDR,val);
    }
#else
    OMAP_SPI_RD32(MODU,OMAP_MCSPI_CHCONF(CHAN),val);
    cfg=val&(~(MCSPI_CHCONF_WL(32)));
    cfg|=WORD_LENGTH_CONF[mode];
    if(val!=cfg)
    {
        OMAP_SPI_WR32(MODU,OMAP_MCSPI_CHCONF(CHAN),cfg);
    }
    OMAP_SPI_WR32(MODU,OMAP_MCSPI_TX(CHAN), data);
    OMAP_SPI_RD32(MODU,OMAP_MCSPI_CHSTAT(CHAN), val);
    while (!(val & MCSPI_CHSTAT_EOT)) 
    {
        if (c > 0x200) 
        {
            DBG("Err: %s() No WR End Intr \n", __FUNCTION__);
            return -1;
        }
        c++;
        OMAP_SPI_RD32(MODU,OMAP_MCSPI_CHSTAT(CHAN),val);
    }
    OMAP_SPI_RD32(MODU,OMAP_MCSPI_CHSTAT(CHAN), val);
    if(val&MCSPI_CHSTAT_RXS)
    {
        OMAP_SPI_RD32(MODU,OMAP_MCSPI_RX(CHAN), val);//It's must or the transmitting can't proceed
    }
#endif
    return 0;
}
EXPORT_SYMBOL(ar_spim_txw);

int ar_spim_rxw(u32 mode,u32 *data)

{
    u32 val = 0;
    u32 c = 0;
    u32 cfg;
#ifdef OMAP5912_SPI
    cfg=AD(EDEV1)|RD|WORD_LENGTH_CONF[mode]; //Enable device 1,Read/Write process activated
    OMAP_SPI_WR32(OMAP_SPI_CTRL_ADDR, cfg);
    OMAP_SPI_RD32(OMAP_SPI_ISR_ADDR, val);
    while (!(val & OMAP_SPI_ISR_RE)) 
    {
        if (c > 0x2000) 
        {
            DBG("Err: %s() No RD End Intr \n", __FUNCTION__);
            return -1;
        }
        c++;
        OMAP_SPI_RD32(OMAP_SPI_ISR_ADDR, val);
    }
    OMAP_SPI_WR32(OMAP_SPI_ISR_ADDR, OMAP_SPI_ISR_RE);
    for(c=0;c<100;c++);//some delay
    OMAP_SPI_RD32(OMAP_SPI_RX_ADDR, val);
#else
    OMAP_SPI_RD32(MODU,OMAP_MCSPI_CHCONF(CHAN),val);
    cfg=val&(~(MCSPI_CHCONF_WL(32)));
    cfg|=WORD_LENGTH_CONF[mode];
    if(val!=cfg)
    {
        OMAP_SPI_WR32(MODU,OMAP_MCSPI_CHCONF(CHAN),cfg);
    }

    OMAP_SPI_WR32(MODU,OMAP_MCSPI_TX(CHAN),0xffffffff);//generate the RX clock
    OMAP_SPI_RD32(MODU,OMAP_MCSPI_CHSTAT(CHAN), val);

    while (!(val & MCSPI_CHSTAT_RXS)) 
    {
        if (c > 0x2000) 
        {
            DBG("Err: %s() No RD End Intr \n", __FUNCTION__);
            return -1;
        }
        c++;
        OMAP_SPI_RD32(MODU,OMAP_MCSPI_CHSTAT(CHAN), val);
    }
    OMAP_SPI_RD32(MODU,OMAP_MCSPI_RX(CHAN),val);
#endif
    *data=val&DATA_SIZE_MASK[mode];
    return 0;
}

EXPORT_SYMBOL(ar_spim_rxw);

int ar_spim_WriteReg(u32 mode,u32 address,u32 data)
{
    int status;
    u32 cmd_addr;
    if((address==AR6000_SPI_STATUS_ADDRESS)||(address==AR6000_SPI_DMA_ADDR_ADDRESS)	\
		||(address==AR6000_SPI_DMA_CNT_ENA_ADDRESS)||(address==AR6000_SPI_CONFIG_ADDRESS))
    {
        cmd_addr=address|SPI_INTERNAL_ADDRESS;
	if(address==AR6000_SPI_CONFIG_ADDRESS)
		{
			bConfigRegFirstAccess=0;
		}
    }

    else
    {
        cmd_addr=address;
    }
    status=ar_spim_txw(PIO_16,cmd_addr);
    status+=ar_spim_txw(mode,data);
    return status;
}
int ar_spim_ReadReg(u32 mode,u32 address,u32* data)
{
    int status;
    u32 cmd_addr;
    
    if((address==AR6000_SPI_STATUS_ADDRESS)||(address==AR6000_SPI_DMA_ADDR_ADDRESS)	\
		||(address==AR6000_SPI_DMA_CNT_ENA_ADDRESS)||(address==AR6000_SPI_CONFIG_ADDRESS))
    {
        cmd_addr=address|SPI_INTERNAL_ADDRESS|SPI_READ_OPERATION;
        status=ar_spim_txw(PIO_16,cmd_addr);
        status+=ar_spim_rxw(mode,data);
        if((address==AR6000_SPI_CONFIG_ADDRESS)&&(bConfigRegFirstAccess==1))
        {
#ifdef OMAP5912_SPI
            status+=ar_spim_rxw(mode,data);//if the first accessing of 0x3000 is reading,it needs an additional phase
#endif
            bConfigRegFirstAccess=0;
        }
        return status;
    }
    else
    {
        if(address==AR6000_SPI_LOCAL_BUS_ADDRESS)
        {
            cmd_addr=address|SPI_READ_OPERATION;
            status=ar_spim_txw(PIO_16,cmd_addr);
            status+=ar_spim_rxw(PIO_16,data);
            return status;
        }
        else
        {
            cmd_addr=address|SPI_READ_OPERATION;
            status=ar_spim_txw(PIO_16,cmd_addr);
            status=ar_spim_txw(PIO_16,AR6000_SPI_STATUS_ADDRESS|SPI_READ_OPERATION|SPI_INTERNAL_ADDRESS);
            if(mode==PIO_32)
            {
                ar_spim_rxw(mode,data);//additional clock phase needed
            }
            status=ar_spim_rxw(mode,data);
            status+=ar_spim_rxw(mode,data);
            return status;
        }
    }

    }
int ar_spim_DS16_to_DS8(void)
{int status;
    ar_spim_WriteReg(PIO_16,0x3000,0x8080);
    ar_spim_ReadReg(PIO_8,0x3000,&status);
    mdelay(1);
    if((status&0xff)==0x80)
    {
        return 0;
    }
    else
    {
        printk("%s error:status:%08X\n",__FUNCTION__,status);
        printk("Please make sure the card will power into SDIO_OFF state(CLK_REQ=0) and re-insert the card,then try again\n");
        return -1;
    }
}
int ar_spim_DS8_to_DS16(void)
{int status;
    ar_spim_WriteReg(PIO_8,0x3000,0x8181);
    ar_spim_ReadReg(PIO_16,0x3000,&status);
    if((status&0xffff)==0x8181)
    {
        return 0;
    }
    else
    {
        printk("%s status:%08X\n",__FUNCTION__,status);
        return -1;
    }
}

void ar_spim_reset(void)
{
    //OMAP_SPI_WR32(OMAP_SPI_SCR_ADDR, SOFTRESET);
}
EXPORT_SYMBOL(ar_spim_reset);
volatile u32 *pPRCM;
void reg_status(void)
{
    volatile u32 *ptr;
    u32 i;
    ptr=(u32*)MCSPI1_VIRTUAL_BASE_ADDRESS;  
    for(i=0;i<0x20;i++)
    {
	if((i%4)==0)
	{
	    DBG("\n");
	}
	DBG("%08X\t",readl(ptr));
	ptr++;
    }
    DBG("\n");
}
int ar_spim_WriteRegDiag(u32 mode,u32 address,u32 data)
{
    int status=-1;
    if(mode==PIO_16)
    {
        status=ar_spim_WriteReg(PIO_16,WINDOW_DATA_ADDRESS,data&0xffff);
        status=ar_spim_WriteReg(PIO_16,WINDOW_DATA_ADDRESS+2,(data>>16));
        status+=ar_spim_WriteReg(PIO_16,WINDOW_WRITE_ADDR_ADDRESS+2,address>>16);
        status+=ar_spim_WriteReg(PIO_16,WINDOW_WRITE_ADDR_ADDRESS,address&0xffff);
    }
    else
    {
        if(mode==PIO_32)
        {
            /*PIO_32 mode not supported as the hardware mis-designed so that it triggers on the wrong 
             * end of the LittleEndian32-bit word
             *
             */
            //status=ar_spim_WriteReg(PIO_32,WINDOW_DATA_ADDRESS,data);
            //status+=ar_spim_WriteReg(PIO_32,WINDOW_WRITE_ADDR_ADDRESS,address);
            return status;
        }
        else
        {
            status =ar_spim_WriteReg(PIO_8,WINDOW_DATA_ADDRESS+3,data>>24);
            status+=ar_spim_WriteReg(PIO_8,WINDOW_DATA_ADDRESS+2,(data>>16)&0xff);
            status+=ar_spim_WriteReg(PIO_8,WINDOW_DATA_ADDRESS+1,(data>>8)&0xff);
            status+=ar_spim_WriteReg(PIO_8,WINDOW_DATA_ADDRESS,(data&0xff));
            
            status+=ar_spim_WriteReg(PIO_8,WINDOW_WRITE_ADDR_ADDRESS+3,address>>24);
            status+=ar_spim_WriteReg(PIO_8,WINDOW_WRITE_ADDR_ADDRESS+2,(address>>16)&0xff);
            status+=ar_spim_WriteReg(PIO_8,WINDOW_WRITE_ADDR_ADDRESS+1,(address>>8)&0xff);
            status+=ar_spim_WriteReg(PIO_8,WINDOW_WRITE_ADDR_ADDRESS,(address&0xff));
        }

    }
    return status;
}
u32 ar_spim_ReadRegDiag(u32 mode,u32 address,u32 *data)
{
    int status=-1;
    u32 tmp1,tmp2,tmp3,tmp4;
    if(mode==PIO_16)
    {
        status=ar_spim_WriteReg(PIO_16,WINDOW_READ_ADDR_ADDRESS+2,address>>16);
        status+=ar_spim_WriteReg(PIO_16,WINDOW_READ_ADDR_ADDRESS,address&0xffff);
        udelay(1); 
        status+=ar_spim_ReadReg(PIO_16,WINDOW_DATA_ADDRESS,&tmp1);
        status+=ar_spim_ReadReg(PIO_16,(WINDOW_DATA_ADDRESS+2),&tmp2);
        *data=tmp1+(tmp2<<16);
    }
    else
    {
        if(mode==PIO_32)
        {
            /*PIO_32 mode not supported as the hardware mis-designed so that it triggers on the wrong 
             * end of the LittleEndian32-bit word
             *
             */
            //status+=ar_spim_WriteReg(PIO_32,WINDOW_READ_ADDR_ADDRESS,address);
            //status+=ar_spim_ReadReg(PIO_32,WINDOW_DATA_ADDRESS,data);
            return status;
        }
        else
        {
            status+=ar_spim_WriteReg(PIO_8,WINDOW_READ_ADDR_ADDRESS+3,address>>24);
            status+=ar_spim_WriteReg(PIO_8,WINDOW_READ_ADDR_ADDRESS+2,(address>>16)&0xff);
            status+=ar_spim_WriteReg(PIO_8,WINDOW_READ_ADDR_ADDRESS+1,(address>>8)&0xff);
            status+=ar_spim_WriteReg(PIO_8,WINDOW_READ_ADDR_ADDRESS,(address&0xff));

            ar_spim_ReadReg(PIO_8,WINDOW_DATA_ADDRESS,&tmp1);
            ar_spim_ReadReg(PIO_8,WINDOW_DATA_ADDRESS+1,&tmp2);
            ar_spim_ReadReg(PIO_8,WINDOW_DATA_ADDRESS+2,&tmp3);
            ar_spim_ReadReg(PIO_8,WINDOW_DATA_ADDRESS+3,&tmp4);
            *data=tmp1+(tmp2<<8)+(tmp3<<16)+(tmp4<<24);
        }
    }
    return status;
}

void ar_spim_forceROM(void)
{int i;
    static struct
    {
        u32 addr;
        u32 data;
    }Force_ROM[]=
    {
        {0x00001ff0,0x175b0027},//jump instruction at 0xa0001ff0
        {0x00001ff4,0x00000000},//nop instruction at 0xa0001ff4
        {MC_REMAP_TARGET_ADDRESS,0x00001ff0},//remap to 0xa0001ff0
        {MC_REMAP_COMPARE_ADDRESS,0x01000040},//...from 0xbfc0040
        {MC_REMAP_SIZE_ADDRESS,0x00000000},//...1 cacheline
        {MC_REMAP_VALID_ADDRESS,0x00000001},//...remap is valid
        {LOCAL_COUNT_ADDRESS+0x10,0},//clear BMI credit counter
        {RESET_CONTROL_ADDRESS,RESET_CONTROL_WARM_RST_MASK},
    };
    DBG("Force Target to execute from ROM...\n");
    for(i=0;i<sizeof(Force_ROM)/sizeof(*Force_ROM);i++)
    {
       ar_spim_WriteRegDiag(PIO_8,Force_ROM[i].addr,Force_ROM[i].data);
    }
}

static int __init gspi_init(void)
{
#define  MAX_TRY 30
    u32 i=MAX_TRY;
    u32 rval=0;
    u32 status;
#ifdef OMAP2420_SPI
    MCSPI1_VIRTUAL_BASE_ADDRESS=(u32)ioremap(OMAP_MCSPI1_BASE_ADDRESS,0x100);
    OMAP_CONTROL_PADCONF_VIRTUAL_BASE_ADDRESS=(u32)ioremap(OMAP_CONTROL_PADCONF_BASE_ADDRESS,OMAP_CONTROL_PADCONF_SIZE); 
    pPRCM=ioremap(OMAP_PRCM_CORE_ENABLE_ADDRESS,0x20);
#endif    
    ar_spim_init();
    ar_spim_WriteReg(PIO_16,AR6000_SPI_LOCAL_BUS_ADDRESS,LOCAL_BUS_IO_ENABLE_16BIT_MASK|LOCAL_BUS_KEEP_AWAKE_16BIT_MASK);
    mdelay(1);
    
    while(i>0)	//loop till error cleared or maximum retry reached
    {
        ar_spim_ReadReg(PIO_16,AR6000_SPI_LOCAL_BUS_ADDRESS,&rval);
        ar_spim_WriteReg(PIO_16,AR6000_SPI_STATUS_ADDRESS,0xffff);//clear power on error
        ar_spim_ReadReg(PIO_16,AR6000_SPI_STATUS_ADDRESS,&status);
        if((status&(~STATUS_DMA_OVER_MASK_16BIT))==SPI_READY_MASK_16BIT)
        {
            break;
        }
        else
        {
            udelay(200);
        }
	i--;
    }
    ar_spim_ReadReg(PIO_16,AR6000_SPI_LOCAL_BUS_ADDRESS,&rval);
    i=MAX_TRY;
    while(i>0) //wait for chip to wake up
    {
        ar_spim_ReadReg(PIO_16,AR6000_SPI_LOCAL_BUS_ADDRESS,&rval);
        if((rval&0x3)==0x01)
        {
            break;
        }
        else
        {
            udelay(100);
        }
        i--;
    }
    printk("LocalBus:%08X\n",rval);

    ar_spim_ReadReg(PIO_16,AR6000_SPI_CONFIG_ADDRESS,&rval);
    DBG("CONFIG:%08X\n",rval);
    ar_spim_WriteReg(PIO_16,AR6000_HOST_INTERFACE_SCRATCH_ADDRESS,0x1234);	//write/read scrach register to test SPI 
    ar_spim_WriteReg(PIO_16,AR6000_HOST_INTERFACE_SCRATCH_ADDRESS+2,0x5678);
    
    ar_spim_ReadReg(PIO_16,AR6000_HOST_INTERFACE_SCRATCH_ADDRESS,&rval);
    DBG("Write:0x1234,Read:%04X\n",rval);
    ar_spim_ReadReg(PIO_16,AR6000_HOST_INTERFACE_SCRATCH_ADDRESS+2,&rval);
    DBG("Write:0x5678,Read:%04X\n",rval);
    ar_spim_ReadReg(PIO_16,AR6000_SPI_LOCAL_BUS_ADDRESS,&rval);

    ar_spim_DS16_to_DS8();//Diagnostic Window Register needs 8bit access mode
    mdelay(2);
    ar_spim_WriteRegDiag(PIO_8,LOCAL_SCRATCH_ADDRESS,0x8);
    ar_spim_ReadRegDiag(PIO_8,SYSTEM_SLEEP_ADDRESS,&rval);	//disable system sleep
    DBG("System Sleep:%08X\t\t",rval);
    ar_spim_WriteRegDiag(PIO_8,SYSTEM_SLEEP_ADDRESS,0x1);
    ar_spim_ReadRegDiag(PIO_8,SYSTEM_SLEEP_ADDRESS,&rval);
    DBG("Disable Sleep:%08X\n",rval);
    ar_spim_ReadRegDiag(PIO_8,CORE_PAD_ENABLE_ADDRESS,&rval); //Core pad enable when on SOC_ON/ON state
    DBG("CORE PAD:%08X\t\t",rval);
    ar_spim_WriteRegDiag(PIO_8,CORE_PAD_ENABLE_ADDRESS,CORE_PAD_ENABLE_ON_MASK|CORE_PAD_ENABLE_SOC_ON_MASK);
    ar_spim_ReadRegDiag(PIO_8,CORE_PAD_ENABLE_ADDRESS,&rval);
    DBG("CORE PAD:%08X\n",rval);
    ar_spim_WriteRegDiag(PIO_8,GPIO_OUT_W1TS_ADDRESS,0x100);//GPIO8 to reflect the wake/sleep status
    ar_spim_WriteRegDiag(PIO_8,GPIO_ENABLE_W1TS_ADDRESS,0x100);
    ar_spim_WriteRegDiag(PIO_8,KEEP_AWAKE_ADDRESS,0x40);//30.5*64 =1952us=2ms
    
    ar_spim_forceROM();	//force the Target to warm reset and boot from ROM
    mdelay(1000);
    ar_spim_DS8_to_DS16();//Diagnostic Window Register needs 8bit access mode
    return 0;
}

static void __exit gspi_exit (void)
{
//   ar_spim_reset();
}
module_init(gspi_init);
module_exit(gspi_exit);
