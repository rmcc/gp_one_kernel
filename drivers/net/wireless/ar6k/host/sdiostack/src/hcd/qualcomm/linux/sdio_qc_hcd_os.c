// The software source and binaries included in this development package are
// licensed, not sold. You, or your company, received the package under one
// or more license agreements. The rights granted to you are specifically
// listed in these license agreement(s). All other rights remain with Atheros
// Communications, Inc., its subsidiaries, or the respective owner including
// those listed on the included copyright notices. Distribution of any
// portion of this package must be in strict compliance with the license
// agreement(s) terms. 


/******************************************************************************
 *
 * @File:       sdio_qualcomm_hcd_os.c
 *
 * @Abstract:   Qualcomm SDIO Host Controller Driver
 *
 * @Notice:     Copyright(c), 2008 Atheros Communications, Inc.
 *
 * $ATH_LICENSE_SDIOSTACK0$
 *
 *****************************************************************************/

/* head files */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <asm/dma-mapping.h>
#include <asm/irq.h>
#include <asm/scatterlist.h>
#if defined(QST1105_BSP2120) || defined(ND1)
#include <asm/arch/pmic.h>
#endif
///FIH+++
//#include <asm/arch/rpc_pm.h>
#ifdef SLEEP_REG_SUPPORT
#include <asm/arch/sleep.h>
#endif
///FIH+++
//#if defined(MSM7201A_BSP6310) || defined(MSM7201A_BSP6320) || defined(QSD8K_BSP2045) || defined(QST1105_BSP2510)
//#include <asm/arch/vreg.h>
//#endif
// FIH---
#if defined(ND1)
#include <asm/arch/gpio.h>
#endif
#include "../sdio_qc_hcd.h"
#include "sdio_std_hcd_linux.h"

///FIH+++
#include <mach/gpio.h>
//FIH---
/* global variables */
static int ForceSDMA = 0;
#ifdef SLEEP_REG_SUPPORT
sleep_okts_handle sd_hcd_sleep_handle;
#endif

#ifdef CONFIG_PM
static PSDHCD_INSTANCE cur_pHcInstance=NULL;
#endif

/*
 * MapAddress - sets up the address for a given device
 */
static int MapAddress(struct platform_device *pdev, char *pName, PSDHCD_MEMORY pAddress)
{
    if (!request_mem_region(pAddress->Raw, pAddress->Length, pName)) {
        printk( KERN_ERR " SDIO Qualcomm HCD: MapAddress - "
               "memory in use: 0x%X(0x%X)\n",
               (UINT)pAddress->Raw,
               (UINT)pAddress->Length);
        return -EBUSY;
    }

    pAddress->pMapped = ioremap_nocache(pAddress->Raw, pAddress->Length);

    if (pAddress->pMapped == NULL) {
        printk( KERN_ERR " SDIO Qualcomm HCD: MapAddress - "
               "unable to map memory\n");
        /* cleanup region */
        release_mem_region(pAddress->Raw, pAddress->Length);
        return -EFAULT;
    }

    return 0;
}


/*
 * UnmapAddress - unmaps the address 
 */
static void UnmapAddress(PSDHCD_MEMORY pAddress)
{
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
#ifdef CONFIG_PM
    cur_pHcInstance = NULL;
#endif
}

static void sdio_hcd_clk_en(
        struct platform_device   *pdev,
        PSDHCD_INSTANCE          hcd_ins)
{
    SYSTEM_STATUS err=0;

    /* enable bus clock */
    hcd_ins->pclk = clk_get(&pdev->dev, MSM_SDC_PCLK);
    if (IS_ERR(hcd_ins->pclk)) {
        err = PTR_ERR(hcd_ins->pclk);
        printk( KERN_ERR " SDIO Qualcomm HCD: error obtaining bus clk, err=[%d]\n", err);
        return;
    }
    
    err = clk_enable(hcd_ins->pclk);
#if defined(MSM7201A_BSP6310) || defined(MSM7201A_BSP6320) || defined(QSD8K_BSP2045) || defined(QST1105_BSP2510)
    if (err) {
        printk( KERN_ERR " SDIO Qualcomm HCD: bus clk enable failed, err=[%d]\n", err);
        if( err > 0 ) err = -err;
        return;
    }
#endif

    /* enable slot clock */
    hcd_ins->mclk = clk_get(&pdev->dev, MSM_SDC_MCLK);
    if (IS_ERR(hcd_ins->mclk)) {
        err = PTR_ERR(hcd_ins->mclk);
        printk( KERN_ERR " SDIO Qualcomm HCD: error obtaining slot clk,err=[%d]\n", err);
        return;
    }

    err = clk_enable(hcd_ins->mclk);
#if defined(MSM7201A_BSP6310) || defined(MSM7201A_BSP6320) || defined(QSD8K_BSP2045) || defined(QST1105_BSP2510)
    if (err) {
        printk( KERN_ERR " SDIO Qualcomm HCD: slot clk enable failed, err=[%d]\n", err);
        if( err > 0 ) err = -err;
        return;
    }
#endif
}

static void sdio_hcd_clk_rate(
        struct platform_device   *pdev,
        PSDHCD_INSTANCE          hcd_ins)
{
#if defined(MSM7201A_BSP6310) || defined(MSM7201A_BSP6320) || defined(QSD8K_BSP2045) || defined(QST1105_BSP2510)

    SYSTEM_STATUS err = 0;

    DBG_PRINT(SDDBG_TRACE, ("+SDIO Qualcomm HCD: configure SD clock rate\n"));

    err = clk_set_rate(hcd_ins->mclk, WLAN_CLK_RATE);
    if (err) {
        printk( KERN_ERR " SDIO Qualcomm HCD: Clock rate set failed (%d)\n", err);
    }

#elif defined(QST1105_BSP2120) || defined(ND1)

    SDHCD_MEMORY SDC1 = {0};

    DBG_PRINT(SDDBG_TRACE, ("+SDIO Qualcomm HCD: configure SD clock rate\n"));
    msleep(1000);

    /* map clock registers */
    SDC1.Raw = 0xa8600000;
    SDC1.Length = 0x1000;
    MapAddress(pdev, "SDIO72001", &SDC1);

    /* turn off by SDC_H_CLK_ENA */
    DBG_PRINT(SDDBG_TRACE, (" SDIO Qualcomm HCD: turn off clock rate\n"));
    _WRITE_DWORD_REG(
            SDC1.pMapped + 0x0,
            _READ_DWORD_REG(SDC1.pMapped + 0x0) & ~(1 << SDC_H_CLK_ENA));
    msleep(200);

    /*
     * QSD8K	
     *
     * 15MHz:
     * 0x0005FFBF  0xFFC40B59
     * 20MHz:
     * 0x0005FFCF  0xFFD40B59
     * 20M by K
     * 0x0005FFEF  0xFFF40B59
     * 16M by K
     * 0x0004FFEF  0xFFF30B59
     */
    
    /*
     * QST1105
     *
     * 16M
     * 0x0019ff9f  0xffb80b5c
     * 20M
     * 0x0021ff9f  0xffC00b5c
     * 25M
     * 0x0029ff9f  0xffC80b5c
     * 30M
     * 0x0031ff9f  0xffd00b5c
     */

    /* configure SDC_MD_REG and SDC_NS_REG registers */
    _WRITE_DWORD_REG(SDC1.pMapped + SDC_MD_REG, 0x0029FF9F);
    _WRITE_DWORD_REG(SDC1.pMapped + SDC_NS_REG, 0xFFC80B5C);
    msleep(200);

    /* turn on by SDC_H_CLK_ENA */
    DBG_PRINT(SDDBG_TRACE, (" SDIO Qualcomm HCD: turn on clock rate\n"));
    _WRITE_DWORD_REG(
            SDC1.pMapped + 0x0,
    	    _READ_DWORD_REG(SDC1.pMapped + 0x0) | (1 << SDC_H_CLK_ENA));
    msleep(200);
    
    printk( KERN_ERR " SDIO Qualcomm HCD: CLK REG = [%x, %x, %x, %x]\n",
           _READ_DWORD_REG(SDC1.pMapped + 0x300),
           _READ_DWORD_REG(SDC1.pMapped + 0x320),
           _READ_DWORD_REG(SDC1.pMapped + SDC_MD_REG),
           _READ_DWORD_REG(SDC1.pMapped + SDC_NS_REG));

    /* unmap memory */
    msleep(1000);
    UnmapAddress(&SDC1);
#endif

    DBG_PRINT(SDDBG_TRACE, ("-SDIO Qualcomm HCD: configure SD clock rate\n"));
}

void SlotPowerOnOff(PSDHCD_INSTANCE pHcInstance, BOOL On)
{ 
    UINT32 ints;
    
    DBG_PRINT(SDDBG_TRACE, (" SDIO Qualcomm HCD: turn power %s\n", (On)? "ON": "OFF"));

    ints = READ_MMC_REG(pHcInstance, MCI_POWER_REG);
    ints &= ~((UINT32)(MCI_POWER_CTRL_MASK));

    if (On) {
    	ints |= MCI_POWER_CTRL_ON;
    } else {
    	ints |= MCI_POWER_CTRL_OFF;
    }

    WRITE_MMC_REG(pHcInstance, MCI_POWER_REG, ints);
}


//FIH+++

int wlan_pwr_on(void)
{
    return 0;
}

int wlan_enable(void)
{
    return 0;
}

#if 0

int wlan_pwr_on(void)
{
#if defined(MSM7201A_BSP6310) || defined(MSM7201A_BSP6320) || defined(QSD8K_BSP2045) || defined(QST1105_BSP2510)
    struct vreg *vreg;
#endif
    int ret = 0;

    printk( KERN_ERR " SDIO Qualcomm HCD: power up WLAN\n");

#if defined(MSM7201A_BSP6310) || defined(MSM7201A_BSP6320) || defined(QSD8K_BSP2045) || defined(QST1105_BSP2510)

    /* for 3.0V*/
    vreg= vreg_get(0, WLAN_3_0V_ID);

    ret = vreg_enable(vreg);
    if (ret)
        printk( KERN_ERR " SDIO Qualcomm HCD: voltage enable failed\n");

    ret = vreg_set_level(vreg, 3050);
    if (ret)
        printk( KERN_ERR " SDIO Qualcomm HCD: voltage level setup failed\n");

#elif defined(ND1)

    /* for 2.6V*/
    ret = pm_vreg_set_level(WLAN_2_6V_ID, 2600);
    if (ret) {
        printk( KERN_ERR " SDIO Qualcomm HCD: 2.6V set level failed\n");
    }

    ret = pm_vote_vreg_switch_pc(WLAN_2_6V_ID, 0);
    if (ret) {
        printk( KERN_ERR " SDIO Qualcomm HCD: 2.6V switch pc failed\n");
    }

    /* for 1.8V*/
    ret = pm_vreg_set_level(WLAN_1_8V_ID, 1800);
    if (ret) {
        printk( KERN_ERR " SDIO Qualcomm HCD: 1.8V set level failed\n");
    }

    ret = pm_vote_vreg_switch_pc(WLAN_1_8V_ID, 0);
    if (ret) {
        printk( KERN_ERR " SDIO Qualcomm HCD: 1.8V switch pc failed\n");
    }

    /* for 1.2V*/
    gpio_direction_output(WLAN_1_2V_ID, 1);
#endif

    msleep(1);
    return ret;
}

int wlan_enable(void)
{
    printk( KERN_ERR " SDIO Qualcomm HCD: Enable WLAN\n");

#if defined(ND1)
    /* back to off state */
    gpio_direction_output(WLAN_CHIP_PWD_L_ID, 0);
    mdelay(1);
    gpio_direction_output(WLAN_CHIP_PWD_L_ID, 1);
#endif

    mdelay(40);
    return 0;
}


#endif
///FIH---

SDIO_STATUS SetUpOneSlotController(
                PSDHCD_CORE_CONTEXT       pStdCore,
                struct  platform_device   *pdev,
                UINT                      SlotNumber,
                BOOL                      AllowDMA)
{
    SDIO_STATUS              status = SDIO_STATUS_ERROR;
    TEXT                     nameBuffer[SDHCD_MAX_DEVICE_NAME];
    PSDHCD_INSTANCE          pHcInstance = NULL;
    UINT                     startFlags = 0;

    do {   
        /* setup the name */
        snprintf(
            nameBuffer,
            SDHCD_MAX_DEVICE_NAME,
            "qualcomm_sdio:%i",
            SlotNumber);

        /* create the instance */        
        pHcInstance = CreateStdHcdInstance(
                              &pdev->dev, 
                              SlotNumber, 
                              nameBuffer);
      
        if (NULL == pHcInstance) {
            status = SDIO_STATUS_NO_RESOURCES;
            break;     
        }

        pHcInstance->OsSpecific.Address.Raw = MSM_SDC_BASE_ADDR; 
        pHcInstance->OsSpecific.Address.Length = MSM_SDC_ADDR_LEN; 

        /* enable bus and slot clock */
	sdio_hcd_clk_en(pdev, pHcInstance);

        /* mapping sdcc register address */
        status = MapAddress(pdev, 
                            pHcInstance->Hcd.pName, 
                            &pHcInstance->OsSpecific.Address);
                            
        /* check if we are able to read SDCC registers correctly now */
        printk( KERN_ERR " SDIO Qualcomm HCD: [%x, %x, %x, %x]\n", 
               _READ_DWORD_REG(pHcInstance->OsSpecific.Address.pMapped + 0xE0),
               _READ_DWORD_REG(pHcInstance->OsSpecific.Address.pMapped + 0xE8),
               _READ_DWORD_REG(pHcInstance->OsSpecific.Address.pMapped + 0xF4),
               _READ_DWORD_REG(pHcInstance->OsSpecific.Address.pMapped + 0xF8));

        if (!SDIO_SUCCESS(status)) {

            printk( KERN_ERR " SDIO Qualcomm HCD: failed to "
                    "map device memory address %s 0x%X, status %d\n",
                    pHcInstance->Hcd.pName,
                    (unsigned int)pHcInstance->OsSpecific.Address.Raw,
                    status); 
            break;                  
        }

        printk( KERN_ERR " SDIO Qualcomm HCD: map address [0x%x] success\n",
                (unsigned int)pHcInstance->OsSpecific.Address.Raw);

        pHcInstance->OsSpecific.InitMask |= SDIO_HCD_MAPPED;
        pHcInstance->pRegs = pHcInstance->OsSpecific.Address.pMapped;       

        if (!AllowDMA) {
            startFlags |= START_HCD_FLAGS_FORCE_NO_DMA;
        }
        
        /* power on WLAN */
        wlan_pwr_on();

        /* enable WLAN */
        wlan_enable();

        /* power on SD command */
	SlotPowerOnOff(pHcInstance, TRUE);

	/* set clock rate */
	sdio_hcd_clk_rate(pdev, pHcInstance);
        
        /* startup this instance */
        status = AddStdHcdInstance(
                     pStdCore,
                     pHcInstance,
                     startFlags,
                     NULL);

#ifdef CONFIG_PM
	cur_pHcInstance = pHcInstance;
#endif
        
    } while (FALSE);     
    
    if (!SDIO_SUCCESS(status)) {
        if (pHcInstance != NULL) {
            cleanup_resources(pdev,pHcInstance);
            DeleteStdHcdInstance(pHcInstance);
        }    
    }
    
    return status;
}

static void CleanUpHcdCore(struct platform_device *pdev, PSDHCD_CORE_CONTEXT pStdCore)
{   
    PSDHCD_INSTANCE pHcInstance;
    unsigned int    irq;

    DBG_PRINT(SDDBG_TRACE, ("+SDIO Qualcomm HCD: CleanUpHcdCore\n"));

    /* make sure interrupts are disabled */
    if (pStdCore->CoreReserved1 & qualcomm_SDIO_IRQ_SET) {
        pStdCore->CoreReserved1 &= ~qualcomm_SDIO_IRQ_SET;
#if 0 /* the current kernel may not assign this resource. */
        irq = platform_get_irq(pdev, 0);
#else
        irq = MSM_SDC_INT;
#endif
        free_irq(irq, pStdCore);
    }

    /* remove all hcd instances associated with this device  */
    while (1) {

        pHcInstance = RemoveStdHcdInstance(pStdCore);
        if (NULL == pHcInstance) {
            /* no more instances */
            break;    
        }

        printk( KERN_ERR " SDIO Qualcomm HCD: CleanUpHcdCore - "
                "removed HC Instance:[0x%X], HCD:[0x%X]\n",
                (UINT)pHcInstance,
                (UINT)&pHcInstance->Hcd);

        /* hcd is now removed, we can clean it up */            
        cleanup_resources(pdev, pHcInstance); 
        DeleteStdHcdInstance(pHcInstance);    
    }

    DeleteStdHostCore(pStdCore);
    DBG_PRINT(SDDBG_TRACE, ("-SDIO Qualcomm HCD: CleanUpHcdCore\n"));
}

/*
 * Probe - probe to setup our device, if present
 */
static int qualcomm_probe(struct platform_device *pdev)
{
    int                    status = SDIO_STATUS_SUCCESS;
    int                    irq;
    int                    controllers = 0;
    BOOL                   dmaSupported = FALSE;
    PSDHCD_CORE_CONTEXT    pStdCore = NULL;
    int                    err = 0;

    printk( KERN_ERR "+SDIO Qualcomm HCD: Probe\n");

#if defined(MSM7201A_BSP6310) || defined(MSM7201A_BSP6320) || defined(QSD8K_BSP2045) || defined(QST1105_BSP2510)
    if (pdev->id != SDIO_WLAN_SLOT_ID) {
        printk(" SDIO Qualcomm HCD: not WLAN slot\n");
        return -EINVAL;
    }
#endif

    do {
        
        pStdCore = CreateStdHostCore(pdev);
        
        if (NULL == pStdCore) {
            err = -ENOMEM; 
            break;  
        }        

        /* Qualcomm DMA is not implemented yet */
        dmaSupported = FALSE;
        
        /* setup an hcd instance  */
        status = SetUpOneSlotController(
                     pStdCore,
                     pdev,          /* device instance */
                     0,             /* std host slot number */
                     dmaSupported   /* enabled DMA */
                     );

        if (SDIO_SUCCESS(status)) {
            controllers++;    
        }
        
        if (0 == controllers) {
            /* if none were created, error */
            err = -ENODEV;    
            break;
        }

	irq = MSM_SDC_INT;
	printk( KERN_ERR " SDIO Qualcomm HCD: Probe - register irq [%d]\n", irq);
            
        /*
	 * enable the single controller interrupt 
         * Interrupts can be called from this point on
	 */
        err = request_irq(
                  irq,
                  sdio_hcd_irq,
                  0,
                  "Qualcomm SDIO",
                  pStdCore);
                          
        if (err < 0) {
            printk( KERN_ERR " SDIO Qualcomm HCD: Probe - "
                    "unable to map interrupt, err=[%d]\n", err);
            break;
        } 
        
        pStdCore->CoreReserved1 |= qualcomm_SDIO_IRQ_SET; 

#ifdef SLEEP_REG_SUPPORT
        sd_hcd_sleep_handle = sleep_register("sdcc2", 1);
        if (sd_hcd_sleep_handle == 0) {
	    printk( KERN_ERR "SDIO Qualcomm HCD: Probe - sleep register failed\n");
        }
#endif

        /* startup the hosts..., this will enable interrupts for card detect */
        status = StartStdHostCore(pStdCore);

        if (!SDIO_SUCCESS(status)) {
            printk( KERN_ERR " SDIO Qualcomm HCD: Probe - unable to start core\n");
            err = -ENODEV;  
            break;
        }
    } while (FALSE);

    if (err < 0) {
        if (pStdCore != NULL) {
            CleanUpHcdCore(pdev, pStdCore);    
        }
    }

    printk( KERN_ERR "-SDIO Qualcomm HCD: Probe\n");
    return err;  
}

/*
 * Remove - remove  device
 * perform the undo of the Probe
 */
static int qualcomm_remove(struct platform_device *pdev) 
{
    PSDHCD_CORE_CONTEXT  pStdCore;
    
    printk( KERN_ERR "+SDIO Qualcomm HCD: Remove\n");

    pStdCore = GetStdHostCore(pdev);
    
    if (NULL == pStdCore) {
        DBG_ASSERT(FALSE);
        return -1;    
    }

    CleanUpHcdCore(pdev, pStdCore);
#ifdef SLEEP_REG_SUPPORT
    sleep_unregister(sd_hcd_sleep_handle);
#endif
    printk( KERN_ERR "-SDIO Qualcomm HCD: Remove\n");
    return 0;
}

/* SDIO interrupt request */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static irqreturn_t sdio_hcd_irq(int irq, void *context, struct pt_regs *r)
#else
static irqreturn_t sdio_hcd_irq(int irq, void *context)
#endif
{
    PSDHCD_CORE_CONTEXT pStdCore = (PSDHCD_CORE_CONTEXT)context;
    PSDLIST             pListItem;
    PSDHCD_INSTANCE	pHcInstance;

    do {
        if (SDLIST_IS_EMPTY(&pStdCore->SlotList)) {
	    break;
        }

    	pListItem = SDLIST_GET_ITEM_AT_HEAD(&pStdCore->SlotList);
    	pHcInstance = CONTAINING_STRUCT(pListItem, SDHCD_INSTANCE, List);
        WRITE_MMC_REG(pHcInstance, MCI_INT_MASK_REG, 0); 
        QueueEventResponse(pHcInstance, WORK_ITEM_SDIO_IRQ);
    } while (0);

    return IRQ_HANDLED;
}

#ifdef CONFIG_PM
static int qualcomm_suspend(struct platform_device *pdev, pm_message_t state)
{
    printk("%s: Enter, event=%d\n", __FUNCTION__, state.event);

    switch (state.event) {
	case PM_EVENT_SUSPEND:
		printk("%s: EVENT Suspend\n", __FUNCTION__);
		break;
	case PM_EVENT_FREEZE:
		printk("%s: EVENT Freeze\n", __FUNCTION__);
		break;
	case PM_EVENT_PRETHAW:
		printk("%s: EVENT Prethaw\n", __FUNCTION__);
		break;
	default:
		printk("%s: Unknown event: %d\n", __FUNCTION__, state.event);
		break;
    }

    if ( cur_pHcInstance != NULL ) {
	printk("%s: Sent HcdEvent: DETACH\n", __FUNCTION__);
    	SDIO_HandleHcdEvent(&(cur_pHcInstance->Hcd) ,EVENT_HCD_DETACH);
    
	HcdDeinitialize(cur_pHcInstance);

    	// cut off the power
	SlotPowerOnOff(cur_pHcInstance, FALSE);
    }



    printk("%s: Leave\n", __FUNCTION__);
    return 0; 
}

static int qualcomm_resume(struct platform_device *pdev)
{
    printk("%s: Enter\n", __FUNCTION__);

    if ( cur_pHcInstance != NULL ) {
	SlotPowerOnOff(cur_pHcInstance, TRUE);

       HcdInitialize(cur_pHcInstance);

	printk("%s: call ProcessDeferredCardDetect\n", __FUNCTION__);
    	SDIO_HandleHcdEvent(&(cur_pHcInstance->Hcd),EVENT_HCD_ATTACH);
	//ProcessDeferredCardDetect(cur_pHcInstance);
    }

    printk("%s: Leave\n", __FUNCTION__);
    return 0;
}
#endif

static struct platform_driver qualcomm_sdio_driver =
{
    .probe          = qualcomm_probe,
    .remove         = qualcomm_remove,
#if 0
#ifdef CONFIG_PM
    .suspend        = qualcomm_suspend,
    .resume         = qualcomm_resume,
#endif
#else
    .suspend        = NULL,
    .resume         = NULL,
#endif
    .driver         = {
                          .name   = SDIO_HCD_PD_NAME,
                          .owner  = THIS_MODULE,
                      },
};

/*
 * module init
 */
static int __init sdio_qualcomm_hcd_init(void)
{
    int err;
    
    printk( KERN_ERR "+SDIO Qualcomm HCD: Load\n");
    InitStdHostLib();
   
    /* register platform driver */
    err = platform_driver_register(&qualcomm_sdio_driver);

    if (err < 0) {
        printk( KERN_ERR " SDIO Qualcomm HCD: Load - "
                "failed to register platform driver, err=[%d]\n",
	        err);
    }

    printk( KERN_ERR "-SDIO Qualcomm HCD: Load\n");
    return err;
}

/*
 * module cleanup
 */
static void __exit sdio_qualcomm_hcd_cleanup(void)
{
    printk( KERN_ERR "+SDIO Qualcomm HCD: Unload\n");
    platform_driver_unregister(&qualcomm_sdio_driver);
    DeinitStdHostLib();
    printk( KERN_ERR "-SDIO Qualcomm HCD: Unload\n");
}

/* module entry functions */
module_init(sdio_qualcomm_hcd_init);
module_exit(sdio_qualcomm_hcd_cleanup);

/* module parameters */
module_param(ForceSDMA, int, 0444);
MODULE_PARM_DESC(ForceSDMA, "Force Host controller to use simple DMA if available");

/* module characters */
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_AUTHOR(AUTHOR);
