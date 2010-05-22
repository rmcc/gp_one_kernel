/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@file: sdio_hcd_os.c

@abstract: S3C6400 SDIO Host Controller Driver

#notes: includes module load and unload functions
 
@notice: Copyright (c), 2008 Atheros Communications, Inc.


//
// The software source and binaries included in this development package are
// licensed, not sold. You, or your company, received the package under one
// or more license agreements. The rights granted to you are specifically
// listed in these license agreement(s). All other rights remain with Atheros
// Communications, Inc., its subsidiaries, or the respective owner including
// those listed on the included copyright notices.  Distribution of any
// portion of this package must be in strict compliance with the license
// agreement(s) terms.
//
//
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/highmem.h>
#include <linux/dma-mapping.h>
#include <linux/mmc/host.h>
#include <linux/mmc/protocol.h>
#include <linux/mmc/card.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/proc_fs.h>

#include <asm/dma.h>
#include <asm/dma-mapping.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/scatterlist.h>
#include <asm/sizes.h>
#include <asm/mach/mmc.h>

#include <asm/arch/regs-hsmmc.h>
#include <asm/plat-s3c24xx/clock.h>
#include <asm/arch/dma.h>

#include <asm/arch/hsmmc.h>

#include "../../../include/ctsystem.h"
#include "../../../include/sdio_busdriver.h"
#include "../../stdhost/linux/sdio_std_hcd_linux.h"
#include "../sdio_s3c6400_hcd.h"
#include "../../stdhost/linux/sdio_std_hcd_linux_lib.h"

#define DESCRIPTION "SDIO S3C6400 HCD"
#define AUTHOR "Atheros Communications, Inc."

static INT ForceSDMA = 0;
module_param(ForceSDMA, int, 0444);
MODULE_PARM_DESC(ForceSDMA, "Force Host controller to use simple DMA if available");

#define SDIO_HCD_MAPPED            0x01

#define S3C6400_SDIO_IRQ_SET 0x01

static int s3c6400_probe(struct platform_device *pdev);
static int s3c6400_remove(struct platform_device *pdev);

static irqreturn_t hcd_sdio_irq(int irq, void *context
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
, struct pt_regs * r);
#else
);
#ifndef SA_SHIRQ
#define SA_SHIRQ           IRQF_SHARED
#endif

#endif /* LINUX_VERSION_CODE */

static struct s3c_hsmmc_cfg s3c6400_sdio_platform = {
        .hwport = 0,
        .enabled = 0,
        .host_caps = (MMC_CAP_4_BIT_DATA | MMC_CAP_MULTIWRITE),
        .base = NULL,
        .ctrl3[0] = 0x80800000,
        .ctrl3[1] = 0x80800000,
        .ctrl3[2] = 0x00000080,
};

struct clk              *selected_clk;

struct s3c_hsmmc_cfg    *plat_data;

/* For now - vluban */
#define s3c6400_suspend NULL
#define s3c6400_resume NULL

static struct platform_driver s3c6400_sdio_driver =
{
        .probe          = s3c6400_probe,
        .remove         = s3c6400_remove,
        .suspend        = s3c6400_suspend,
        .resume         = s3c6400_resume,
        .driver         = {
                .name   = "s3c-hsmmc",
                .owner  = THIS_MODULE,
        },
};

    /* Advanced DMA description */
SDDMA_DESCRIPTION HcdADMADefaults = {
    .Flags = SDDMA_DESCRIPTION_FLAG_SGDMA,
    .MaxDescriptors = SDHCD_MAX_ADMA_DESCRIPTOR,
    .MaxBytesPerDescriptor = SDHCD_MAX_ADMA_LENGTH,
    .Mask = SDHCD_ADMA_ADDRESS_MASK, 
    .AddressAlignment = SDHCD_ADMA_ALIGNMENT,
    .LengthAlignment = SDHCD_ADMA_LENGTH_ALIGNMENT,
};

    /* simple DMA descriptions */
SDDMA_DESCRIPTION HcdSDMADefaults = {
    .Flags = SDDMA_DESCRIPTION_FLAG_DMA,
    .MaxDescriptors = 1,
    .MaxBytesPerDescriptor = SDHCD_MAX_SDMA_LENGTH,
    .Mask = SDHCD_SDMA_ADDRESS_MASK, 
    .AddressAlignment = SDHCD_SDMA_ALIGNMENT,
    .LengthAlignment = SDHCD_SDMA_LENGTH_ALIGNMENT,
};

/* s3c_hsmmc_get_platdata
 *
 * get the platform data associated with the given device, or return
 * the default if there is none
 */

static struct s3c_hsmmc_cfg *s3c6400_sdio_get_platdata (struct device *dev)
{
        if (dev->platform_data != NULL)
                return (struct  s3c_hsmmc_cfg *)dev->platform_data;
 
        return &s3c6400_sdio_platform;
}

#if 0
/* s3c_clockstartstop
 * 
 * Calculate divisor and enable output clock
 * or turn the clock off
 */
static void s3c_clockstartstop(PSDHCD_INSTANCE pHcInstance, BOOL On)
{
    UINT8  ctrl;
    UINT32 j, tmpclk, timeout;
    
    printk( KERN_ERR "SDIO S3C6400 HCD: + ClockStartStop %d\n", On);

    WRITE_HOST_REG16(pHcInstance, HOST_REG_CLOCK_CONTROL, 0);
    WRITE_HOST_REG8(pHcInstance, HOST_REG_TIMEOUT_CONTROL, S3C_HSMMC_TIMEOUT_MAX);
    mmiowb();
    
    ctrl = READ_HOST_REG8(pHcInstance, HOST_REG_CONTROL);
    ctrl &= ~HOST_REG_CONTROL_HI_SPEED;
    WRITE_HOST_REG8(pHcInstance, HOST_REG_CONTROL, ctrl);
    mmiowb();
    
    if( !On ) {
      printk( KERN_ERR "SDIO S3C6400 HCD: - ClockStartStop returning after turning the clock off\n");
      return;
    }
    
    for( j=1; j<0x100; j<<=1 ) {
      tmpclk = clk_get_rate( selected_clk ) / j;
      if( tmpclk <= pHcInstance->presetclock )
          break;
    }
    tmpclk = clk_get_rate( selected_clk ) / j;
    printk( KERN_ERR "SDIO S3C6400 HCD: Set clock from source %s, divisor 0x%x, rate %d, clksrc<<4 = %x\n", 
                      plat_data->clk_name[CLKSRC],
                      j<<7, tmpclk, CLKSRC<<4 );
    
    WRITE_HOST_REG32(pHcInstance, 0x80, (0x40000100 | (CLKSRC << 4)) ); 
    
    if (pHcInstance->presetclock > 25000000) {
      WRITE_HOST_REG32(pHcInstance, S3C_HSMMC_CONTROL3, plat_data->ctrl3[1]);
    } else {
      WRITE_HOST_REG32(pHcInstance, S3C_HSMMC_CONTROL3, plat_data->ctrl3[0]);
    }
    mmiowb();
    
    WRITE_HOST_REG16(pHcInstance, HOST_REG_CLOCK_CONTROL, ( ((j<<7) & 0xFF00) | HOST_REG_CLOCK_CONTROL_CLOCK_ENABLE));
    mmiowb();
    
    timeout = 10; /* Wait for internal clock to stabilize */
    while(!((tmpclk = READ_HOST_REG16(pHcInstance, HOST_REG_CLOCK_CONTROL) ) &
                    (UINT16)HOST_REG_CLOCK_CONTROL_CLOCK_STABLE)) { 
        timeout--;
        if( !timeout ) {
          printk( KERN_ERR "SDIO S3C6400 HCD: Timeout waiting for internal clock 2 stabilize\n");
          break;
        }
        mdelay(1);
    }
    
    WRITE_HOST_REG16(pHcInstance, HOST_REG_CLOCK_CONTROL, tmpclk | HOST_REG_CLOCK_CONTROL_SD_ENABLE );
    mmiowb();

    timeout = 10; /* Wait for external clock to stabilize */
    while(!((tmpclk = READ_HOST_REG16(pHcInstance, HOST_REG_CLOCK_CONTROL) ) &
                    (UINT16)HOST_REG_CLOCK_CONTROL_CLOCK_STABLE)) { 
        timeout--;
        if( !timeout ) {
          printk( KERN_ERR "SDIO S3C6400 HCD: Timeout waiting for external clock 2 stabilize\n");
          break;
        }
        mdelay(1);
    }
    printk( KERN_ERR "SDIO S3C6400 HCD: - ClockStartStop\n");
} 

/* s3c_set_bus_width
 *
 * This is no different from standard SDIO controller with the exception
 * that for s3c6400 bit HOST_REG_CONTROL_HI_SPEED never gets set.
 */
static int s3c_set_bus_width (PSDHCD_INSTANCE pHcInstance, PSDCONFIG_BUS_MODE_DATA pMode)
{
    UINT8 control;

    printk( KERN_ERR "SDIO S3C6400 HCD: + s3c_set_bus_width to ");
    control = READ_HOST_REG8(pHcInstance, HOST_REG_CONTROL);
    control &= ~HOST_REG_CONTROL_BUSWIDTH_BITS;
    switch (SDCONFIG_GET_BUSWIDTH(pMode->BusModeFlags)) {
        case SDCONFIG_BUS_WIDTH_1_BIT:
            control |= HOST_REG_CONTROL_1BIT_WIDTH;
            printk(" 1 bit\n");
            break;        
        case SDCONFIG_BUS_WIDTH_4_BIT:
            control |=  HOST_REG_CONTROL_4BIT_WIDTH;
            printk(" 4 bit\n");
            break;
        case SDCONFIG_BUS_WIDTH_MMC8_BIT:
            control |=  HOST_REG_CONTROL_EXTENDED_DATA;
            break;    
        default:      
            DBG_PRINT(SDDBG_TRACE , ("SDIO STD HOST - SetMode, unknown bus width requested 0x%X\n", pMode->BusModeFlags));
            break;
    }
    WRITE_HOST_REG8(pHcInstance, HOST_REG_CONTROL, control);
    mmiowb();
    printk( KERN_ERR "SDIO S3C6400 HCD: - s3c_set_bus_width");\
    return 0;
}

/* s3c_SetBusMode
 * 
 * Sets clock in hcd instance, but defers controller setup until ClockON
 * sets bus width with GPIO setup 4 s3c6400
 */
void s3c_SetBusMode(PSDHCD_INSTANCE pHcInstance, PSDCONFIG_BUS_MODE_DATA pMode)
{
    printk( KERN_ERR "SDIO S3C6400 HCD: + s3c_SetBusMode, width=%d\n", plat_data->bus_width);
    
    pHcInstance->presetclock = pMode->ClockRate;
    
//    hsmmc_set_gpio(plat_data->hwport, plat_data->bus_width);
    s3c_set_bus_width(pHcInstance, pMode);

    printk( KERN_ERR "SDIO S3C6400 HCD: - s3c_SetBusMode\n");
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  s3c-SetPowerLevel - Set power level of board
  Input:  pHcInstance - device context
          On - if true turns power on, else off
          Level - SLOT_VOLTAGE_MASK level
  Output: 
  Return: 
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
SDIO_STATUS s3c_SetPowerLevel(PSDHCD_INSTANCE pHcInstance, BOOL On, SLOT_VOLTAGE_MASK Level)
{
    UINT8 out;
    UINT32 capCurrent;
    
    printk( KERN_ERR "SDIO S3C6400 HCD: + s3c_SetPowerLevel, Power %s\n", On?"on":"off");
    capCurrent = READ_HOST_REG32(pHcInstance, HOST_REG_MAX_CURRENT_CAPABILITIES);
    
    switch (Level) {
      case SLOT_POWER_3_3V:
        out = HOST_REG_POWER_CONTROL_VOLT_3_3;
            /* extract */
        capCurrent = (capCurrent & HOST_REG_MAX_CURRENT_CAPABILITIES_3_3_MASK) >>
                        HOST_REG_MAX_CURRENT_CAPABILITIES_3_3_SHIFT;
        break;
      case SLOT_POWER_3_0V:
        out = HOST_REG_POWER_CONTROL_VOLT_3_0;
            /* extract */
        capCurrent = (capCurrent & HOST_REG_MAX_CURRENT_CAPABILITIES_3_0_MASK) >>
                        HOST_REG_MAX_CURRENT_CAPABILITIES_3_0_SHIFT;
        break;
      case SLOT_POWER_1_8V:
        out = HOST_REG_POWER_CONTROL_VOLT_1_8;
            /* extract */
        capCurrent = (capCurrent & HOST_REG_MAX_CURRENT_CAPABILITIES_1_8_MASK) >>
                        HOST_REG_MAX_CURRENT_CAPABILITIES_1_8_SHIFT;       
        break;
      default:
        DBG_PRINT(SDDBG_ERROR, ("SDIO S3C6400 HCD: SetPowerLevel - illegal power level %d\n",
                                (UINT)Level));
        return SDIO_STATUS_INVALID_PARAMETER; 
    }
     
    if (capCurrent != 0) {
            /* convert to mA and set max current */
        pHcInstance->Hcd.MaxSlotCurrent = capCurrent * HOST_REG_MAX_CURRENT_CAPABILITIES_SCALER;
    } else {
        DBG_PRINT(SDDBG_WARN, ("SDIO S3C6400 HCD: No Current Caps value for VMask:0x%X, using 200mA \n",
                  Level));  
            /* set a value */
        pHcInstance->Hcd.MaxSlotCurrent = 200;
    }
     
    if (On) {
        out |= HOST_REG_POWER_CONTROL_ON;
        if( out != pHcInstance->presetlevel ) {
          WRITE_HOST_REG8(pHcInstance, HOST_REG_POWER_CONTROL, 0);
          mmiowb();
        }
    } else {
        /* Disable interrupts */
        WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE, 0);
        mmiowb();
        /* Reset controller */
        WRITE_HOST_REG8(pHcInstance, HOST_REG_SW_RESET, HOST_REG_SW_RESET_ALL );
        printk(KERN_ERR "SDIO S3C6400 HCD:  Reset HCD\n");
        while(  READ_HOST_REG8(pHcInstance, HOST_REG_SW_RESET) & HOST_REG_SW_RESET_ALL )
          mdelay(1);
                
        capCurrent = S3C_HSMMC_INT_BUS_POWER | S3C_HSMMC_INT_DATA_END_BIT |
                     S3C_HSMMC_INT_DATA_CRC | S3C_HSMMC_INT_DATA_TIMEOUT | S3C_HSMMC_INT_INDEX |
                     S3C_HSMMC_INT_END_BIT | S3C_HSMMC_INT_CRC | S3C_HSMMC_INT_TIMEOUT |
                     S3C_HSMMC_INT_CARD_REMOVE | S3C_HSMMC_INT_CARD_INSERT |
                     S3C_HSMMC_INT_DATA_AVAIL | S3C_HSMMC_INT_SPACE_AVAIL | 
                     S3C_HSMMC_INT_DATA_END | S3C_HSMMC_INT_RESPONSE;
         
        WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE, capCurrent);
        WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE, capCurrent);
        mmiowb();
    }
    
     
    WRITE_HOST_REG8(pHcInstance, HOST_REG_POWER_CONTROL, out);
    WRITE_HOST_REG8(pHcInstance, HOST_REG_POWER_CONTROL, out);
    mmiowb();
    pHcInstance->presetlevel = out;
    capCurrent = READ_HOST_REG32(pHcInstance, HOST_REG_MAX_CURRENT_CAPABILITIES);
    udelay(1000);
    printk( KERN_ERR "SDIO S3C6400 HCD: - s3c-SetPowerLevel\n");
    return SDIO_STATUS_SUCCESS;
}
#endif /* 0 */

/*
 * MapAddress - sets up the address for a given device
*/
static int MapAddress(struct platform_device *pdev, char *pName, PSDHCD_MEMORY pAddress)
{
    struct resource         *mem;
    
    mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    
    if (!mem ) {
        printk(KERN_ERR "SDIO S3C6400 HCD: MapAddress, Failed to get io memory region resouce.\n");
        return -ENOENT;
    } 
    pAddress->Raw = mem->start;
    pAddress->Length = mem->end - mem->start + 1;
    
    if (!request_mem_region (pAddress->Raw, pAddress->Length, pName)) {
        printk(KERN_ERR "SDIO S3C6400 HCD: MapAddress - memory in use: 0x%X(0x%X)\n",
                               (UINT)pAddress->Raw, (UINT)pAddress->Length);
        return -EBUSY;
    }
    pAddress->pMapped = ioremap_nocache(pAddress->Raw, pAddress->Length);
    if (pAddress->pMapped == NULL) {
        printk(KERN_ERR "SDIO S3C6400 HCD: MapAddress - unable to map memory\n");
        /* cleanup region */
        release_mem_region (pAddress->Raw, pAddress->Length);
        return -EFAULT;
    }
    printk(KERN_ERR "SDIO S3C6400 HCD: MapAddress - Channel %d, bus width %d mapped memory: 0x%X(0x%X) to 0x%X\n", 
                            plat_data->hwport, plat_data->bus_width,
                            (UINT)pAddress->Raw, (UINT)pAddress->Length, (UINT)pAddress->pMapped);

    _WRITE_DWORD_REG(pAddress->pMapped + 0x84, plat_data->ctrl3[0]); /* Up 2 25MHz only! */
    hsmmc_set_gpio(plat_data->hwport, 4 /*plat_data->bus_width*/);
    return 0;
}


/*
 * UnmapAddress - unmaps the address 
*/
static void UnmapAddress(PSDHCD_MEMORY pAddress) {
    iounmap(pAddress->pMapped);
    release_mem_region(pAddress->Raw, pAddress->Length);
    pAddress->pMapped = NULL;
}
   
/*
 * cleanup_resources - cleanup resources
*/
static void cleanup_resources(struct platform_device *pdev, 
                                PSDHCD_INSTANCE pHcInstance)
{    
    if (pHcInstance->OsSpecific.InitMask & SDIO_HCD_MAPPED) {
        UnmapAddress(&pHcInstance->OsSpecific.Address);
        pHcInstance->OsSpecific.InitMask &= ~SDIO_HCD_MAPPED;
    }
}

SDIO_STATUS SetUpOneSlotController(PSDHCD_CORE_CONTEXT pStdCore,
                                   struct  platform_device *pdev,
                                   UINT       SlotNumber,
                                   BOOL       AllowDMA)
{
    SDIO_STATUS              status = SDIO_STATUS_ERROR;
    TEXT                     nameBuffer[SDHCD_MAX_DEVICE_NAME];
    PSDHCD_INSTANCE          pHcInstance = NULL;
    UINT                     startFlags = 0;
    
    do {   
            /* setup the name */
        snprintf(nameBuffer, SDHCD_MAX_DEVICE_NAME, "s3c6400_sdio:%i",
                 SlotNumber);
         
            /* create the instance */        
        pHcInstance = CreateStdHcdInstance(&pdev->dev, 
                                           SlotNumber, 
                                           nameBuffer);
      
        if (NULL == pHcInstance) {
            status = SDIO_STATUS_NO_RESOURCES;
            break;     
        }

        status = MapAddress(pdev, 
                            pHcInstance->Hcd.pName, 
                            &pHcInstance->OsSpecific.Address);
                            
        if (!SDIO_SUCCESS(status)) {
            printk(KERN_ERR 
               "SDIO S3C6400 HCD: Probe - failed to map device memory address %s 0x%X, status %d\n",
                pHcInstance->Hcd.pName, (unsigned int)pHcInstance->OsSpecific.Address.Raw, status); 
            break;                  
        }
        pHcInstance->OsSpecific.InitMask |= SDIO_HCD_MAPPED;
#if 0
        pHcInstance->ClockStartStop = s3c_clockstartstop;
        pHcInstance->SetPowerLevel  = s3c_SetPowerLevel;
        pHcInstance->SetBusMode     = s3c_SetBusMode;
#endif    
        pHcInstance->pRegs = pHcInstance->OsSpecific.Address.pMapped;       
    
        if (!AllowDMA) {
            startFlags |= START_HCD_FLAGS_FORCE_NO_DMA;
        }
        
        if (ForceSDMA) {
            startFlags |= START_HCD_FLAGS_FORCE_SDMA;
        }

        /* reset controller */
        WRITE_HOST_REG8(pHcInstance, HOST_REG_SW_RESET, HOST_REG_SW_RESET_ALL );
        mmiowb();
        printk(KERN_ERR "SDIO S3C6400 HCD:  Reset HCD\n");
        while(  READ_HOST_REG8(pHcInstance, HOST_REG_SW_RESET) & HOST_REG_SW_RESET_ALL )
          mdelay(1);
                
        WRITE_HOST_REG32(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE, 0x0);
        WRITE_HOST_REG8(pHcInstance, HOST_REG_TIMEOUT_CONTROL, 0x0E);

        
        /* Enable clock source #2, it's supposed to be 48 MHz */
        clk_enable(clk_get(&pdev->dev, "sclk_48m"));
        pHcInstance->BaseClock = 133*1000*1000;
        clk_enable(selected_clk = clk_get(&pdev->dev, plat_data->clk_name[CLKSRC]));
          
        WRITE_HOST_REG32(pHcInstance, 0x80, 0x40000100 | (CLKSRC << 4) );
        WRITE_HOST_REG32(pHcInstance, S3C_HSMMC_CONTROL3, plat_data->ctrl3[0]);                            
        
        {
            unsigned int i;
            
            i = READ_HOST_REG32(pHcInstance, HOST_REG_PRESENT_STATE);
            
            printk( KERN_ERR "State is %x, Card is %s present\n", i,
                    ( i & 0x40000 ) ? "" : "NOT" );
        }
          
        
            /* startup this instance */
        status = AddStdHcdInstance(pStdCore,
                                   pHcInstance,
                                   startFlags,
                                   NULL,
                                   &HcdSDMADefaults,
                                   &HcdADMADefaults); 
        
    } while (FALSE);     
    
    if (!SDIO_SUCCESS(status)) {
        if (pHcInstance != NULL) {
            cleanup_resources(pdev,pHcInstance);
            DeleteStdHcdInstance(pHcInstance);
        }    
    } else {
        WRITE_HOST_REG32(pHcInstance, 0x80, 0x40000100 | (CLKSRC << 4) );
        WRITE_HOST_REG32(pHcInstance, S3C_HSMMC_CONTROL3, plat_data->ctrl3[0]);                            
        printk(KERN_ERR "SDIO S3C6400 Probe - HCD:0x%x @ 0x%x ready! \n",(UINT)pHcInstance, (UINT)pHcInstance->pRegs);
        pHcInstance->IntSignalEn = 0x1F7;
        pHcInstance->ErrIntSignalEn = HOST_REG_ERROR_INT_STATUS_ALL_ERR;
    }  
    
    return status;
}


static void CleanUpHcdCore(struct platform_device *pdev, PSDHCD_CORE_CONTEXT pStdCore)
{   
    PSDHCD_INSTANCE pHcInstance;
    int             irq;
    
    printk(KERN_ERR "+ SDIO S3C6400 HCD: CleanUpHcdCore\n");
        /* make sure interrupts are disabled */
    if (pStdCore->CoreReserved1 & S3C6400_SDIO_IRQ_SET) {
        pStdCore->CoreReserved1 &= ~S3C6400_SDIO_IRQ_SET; 
        irq = platform_get_irq(pdev, 0);
        free_irq(irq, pStdCore);
    }
    
        /* remove all hcd instances associated with this device  */
    while (1) {
        pHcInstance = RemoveStdHcdInstance(pStdCore);
        if (NULL == pHcInstance) {
                /* no more instances */
            break;    
        }
        printk(KERN_ERR " SDIO S3C6400 HCD: Remove - removed HC Instance:0x%X, HCD:0x%X\n",
            (UINT)pHcInstance, (UINT)&pHcInstance->Hcd);
            /* hcd is now removed, we can clean it up */            
        cleanup_resources(pdev,pHcInstance); 
        DeleteStdHcdInstance(pHcInstance);    
    }
    
    DeleteStdHostCore(pStdCore);     
    printk(KERN_ERR "- SDIO S3C6400 HCD: CleanUpHcdCore\n");
}

/*
 * Probe - probe to setup our device, if present
*/
static int s3c6400_probe(struct platform_device *pdev)
{
    int status = SDIO_STATUS_SUCCESS;
    int ii = 0;
//    int count;
    int irq;
    int controllers = 0;
//    UINT8 config;
    BOOL dmaSupported = FALSE;
    PSDHCD_CORE_CONTEXT pStdCore = NULL;
    SYSTEM_STATUS err = 0;
    struct s3c_hsmmc_cfg    *lplat_data;
    
    lplat_data = s3c6400_sdio_get_platdata(&pdev->dev);
    
    if( lplat_data->hwport )
      return -ENODEV;         /* Support only the first SDIO controller */
    
    plat_data = lplat_data;
    
    printk(KERN_ERR "SDIO S3C6400 HCD: Probe - probing for new device\n");
    
    do {
        
        pStdCore = CreateStdHostCore(pdev);
        
        if (NULL == pStdCore) {
            err = -ENOMEM; 
            break;  
        }        
    
        dmaSupported = TRUE;
        
        /* setup an hcd instance  */
//        for(ii = 0; ii < count; ii++, firstBar++) {
        status = SetUpOneSlotController(pStdCore,
                                            pdev,          /* device instance */
                                            ii,            /* std host slot number */
                                            dmaSupported  /* enabled DMA */
                                            );       
        if (SDIO_SUCCESS(status)) {
                controllers++;    
        }
        
        if (0 == controllers) {
                /* if none were created, error */
            err = -ENODEV;    
            break;
        }
        
        irq = platform_get_irq(pdev, 0);
                
            /* enable the single controller interrupt 
               Interrupts can be called from this point on */
        err = request_irq(irq, hcd_sdio_irq, SA_SHIRQ,
                          "s3c6400hcd", pStdCore);
                          
        if (err < 0) {
            printk(KERN_ERR "SDIO S3C6400 - probe, unable to map interrupt \n");
            break;
        } 
        
        pStdCore->CoreReserved1 |= S3C6400_SDIO_IRQ_SET; 
        
            /* startup the hosts..., this will enable interrupts for card detect */
        status = StartStdHostCore(pStdCore);
        
        if (!SDIO_SUCCESS(status)) {
            printk( KERN_ERR "SDIO S3C6400 HCD: Unable to start core\n");
            err = -ENODEV;  
            break;
        }
               
    } while (FALSE);
    
    if (err < 0) {
        if (pStdCore != NULL) {
            CleanUpHcdCore(pdev,pStdCore);    
        }
    }
    printk(KERN_ERR "SDIO S3C6400 HCD: Exit probe\n");
    return err;  
}

/* Remove - remove  device
 * perform the undo of the Probe
*/
static int s3c6400_remove(struct platform_device *pdev) 
{
    PSDHCD_CORE_CONTEXT  pStdCore;
    
    printk(KERN_ERR "+SDIO S3C6400 HCD: Remove - removing device\n");

    pStdCore = GetStdHostCore(pdev);
    
    if (NULL == pStdCore) {
        DBG_ASSERT(FALSE);
        return -1;    
    }

    CleanUpHcdCore(pdev, pStdCore);
    
    printk(KERN_ERR "-SDIO S3C6400 HCD: Remove\n");
    return 0;
}

/* SDIO interrupt request */
static irqreturn_t hcd_sdio_irq(int irq, void *context
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
, struct pt_regs * r
#endif
)
{
    irqreturn_t retStat;
     
        /* call shared handling ISR in case this is a mult-slot controller using 1 IRQ.
         * if this was not a mult-slot controller or each controller has it's own system
         * interrupt, we could call HcdSDInterrupt((PSDHCD_INSTANCE)context)) instead */
    /* if (HcdSDInterrupt((PSDHCD_CORE_CONTEXT)context)) { */
    if (HandleSharedStdHostInterrupt((PSDHCD_CORE_CONTEXT)context)) {
        retStat = IRQ_HANDLED;
    } else {
        retStat = IRQ_NONE;
    }    
    
    return retStat;
}
#if 0
void s3c_prepare_data(PSDHCD pHcd)
{
    SDIO_STATUS     status = SDIO_STATUS_SUCCESS;
    PSDHCD_INSTANCE pHcInstance = (PSDHCD_INSTANCE)pHcd->pContext;
    UINT16          temp;
    PSDREQUEST      pReq;
    UINT32          mask;
 
    pReq = GET_CURRENT_REQUEST(pHcd);
    DBG_ASSERT(pReq != NULL);

}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  s3c_HcdRequest - SD request handler   
  Input:  pHcd - HCD object
  Output:  
  Return: 
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
SDIO_STATUS s3c_HcdRequest(PSDHCD pHcd) 
{
    SDIO_STATUS     status = SDIO_STATUS_SUCCESS;
    PSDHCD_INSTANCE pHcInstance = (PSDHCD_INSTANCE)pHcd->pContext;
    UINT16          temp;
    PSDREQUEST      pReq;
    UINT32          mask;
    
    pReq = GET_CURRENT_REQUEST(pHcd);
    DBG_ASSERT(pReq != NULL);
                
    /* make sure clock is off */
    s3c_clockstartstop(pHcInstance, CLOCK_OFF);
    /* mask the remove while we are spinning on the CMD ready bits */
    MaskIrq(pHcInstance, HOST_REG_INT_STATUS_ALLOW_INSERT_REMOVE_ONLY,FALSE);
                 
    mask = HOST_REG_PRESENT_STATE_BUFFER_COMMAND_INHIBIT_CMD;
    if ( (pReq->Flags & SDREQ_FLAGS_DATA_TRANS) || (GET_SDREQ_RESP_TYPE(pReq->Flags)) )
        mask |= HOST_REG_PRESENT_STATE_BUFFER_COMMAND_INHIBIT_DAT;
    
    /* temp is used as timeout counter for controller wakeup here */
    temp = 10;                               /* 10 mS max         */
    while( READ_HOST_REG32(pHcInstance, HOST_REG_PRESENT_STATE) & mask ) {
        if ( ! temp ) {
             printk(KERN_ERR "S3C6400 SDIO HCD: Controller never released\n");
             UnmaskIrq(pHcInstance, HOST_REG_INT_STATUS_ALLOW_INSERT_REMOVE_ONLY,FALSE);
             return SDIO_STATUS_DEVICE_ERROR;
        }
        tmp--;
        mdelay(1);
    }
    
    if ( !(pReq->Flags & SDREQ_FLAGS_DATA_TRANS) ) {
        mask = READ_HOST_REG32(pHcInstance, HOST_REG_INT_STATUS_ENABLE) | HOST_REG_INT_STATUS_CMD_COMPLETE_ENABLE ;
        WRITE_HOST_REG32(pHcInstance, HOST_REG_INT_STATUS_ENABLE, mask);
    } else {
        mask = READ_HOST_REG32(pHcInstance, HOST_REG_INT_STATUS_ENABLE) & ~HOST_REG_INT_STATUS_CMD_COMPLETE_ENABLE ;
        WRITE_HOST_REG32(pHcInstance, HOST_REG_INT_STATUS_ENABLE, mask);
        s3c_prepare_data(pHcd);
    }
    
}
#endif

/*
 * module init
*/
static int __init sdio_s3c6400_hcd_init(void) {
    SYSTEM_STATUS err;
    
    printk(KERN_ERR "+SDIO S3C6400 HCD: loading....\n");
    InitStdHostLib();
    
    /* register platform driver */
    err = platform_driver_register(&s3c6400_sdio_driver);
    if (err < 0) {
        printk(KERN_ERR "SDIO S3C6400 HCD: failed to register platform driver, %d\n",
                                err);
    }
    printk(KERN_ERR "-SDIO S3C6400 HCD \n");
    return err;
}

/*
 * module cleanup
*/
static void __exit sdio_s3c6400_hcd_cleanup(void) {
    printk(KERN_ERR "+SDIO S3C6400 HCD: unload\n");
    platform_driver_unregister(&s3c6400_sdio_driver);
    DeinitStdHostLib();
    printk(KERN_ERR "-SDIO S3C6400 HCD: leave sdio_s3c6400_hcd_cleanup\n");
}



MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_AUTHOR(AUTHOR);

module_init(sdio_s3c6400_hcd_init);
module_exit(sdio_s3c6400_hcd_cleanup);
