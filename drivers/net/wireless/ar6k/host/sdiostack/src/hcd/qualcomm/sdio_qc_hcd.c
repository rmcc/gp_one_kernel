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
 * @File:       sdio_qualcomm_hcd.c
 *
 * @Abstract:   Qualcomm SDIO Host Controller Driver
 *
 * @Notice:     Copyright(c), 2008 Atheros Communications, Inc.
 *
 * $ATH_LICENSE_SDIOSTACK0$
 *
 *****************************************************************************/

#include "sdio_qc_hcd.h"
#if defined(QST1105_BSP2120) || defined(ND1)
#include <asm/arch/pmic.h>
#endif

/* clock divisor table */
SD_CLOCK_TBL_ENTRY SDClockDivisorTable[SD_CLOCK_MAX_ENTRIES] =
{   /* clock rate divisor, divisor setting */
    {1,   0x0000},
    {2,   0x0100},
    {4,   0x0200},
    {8,   0x0400},
    {16,  0x0800},
    {32,  0x1000},
    {64,  0x2000},
    {128, 0x4000},
    {256, 0x8000}, 
};

/*
 * ClockStartStop - SD clock control
 * Input:  pHcInstance - device object
 *         On - turn on or off (TRUE/FALSE)
 * Output: 
 * Return: 
 * Notes: 
 */
void ClockStartStop(PSDHCD_INSTANCE pHcInstance, BOOL On) 
{
    UINT32 ints;

    DBG_PRINT(SDDBG_TRACE, ("+SDIO Qualcomm HCD: ClockStartStop - action=[%d]\n", (UINT)On));

    ints = READ_MMC_REG(pHcInstance, MCI_CLK_REG);
    ints &= ~( (UINT32)(MCI_CLK_ENABLE_MASK)  |
               (UINT32)(MCI_CLK_PWRSAVE_MASK) |
               (UINT32)(MCI_CLK_FLOWCTRL_MASK) );

    if (On) {
    	ints |= MCI_CLK_ON;
    	ints |= MCI_CLK_PWRSAVE_ON;
    } else {
    	ints |= MCI_CLK_OFF;
    	ints |= MCI_CLK_PWRSAVE_OFF;
    }

    ints |= MCI_CLK_FLOWCTRL_ON;
    WRITE_MMC_REG(pHcInstance, MCI_CLK_REG, ints);
    DBG_PRINT(SDDBG_TRACE, ("-SDIO Qualcomm HCD: ClockStartStop\n"));
}

int SetClockRate(PSDHCD_INSTANCE pHcInstance, PSDCONFIG_BUS_MODE_DATA pMode) 
{
    DBG_PRINT(SDDBG_TRACE, (" SDIO Qualcomm HCD: SetClockRate - clock=[%d]\n", pMode->ClockRate));
    return 0;
}

/*
 * SetBusMode - Set Bus mode
 * Input:  pHcInstance - device object
 *         pMode - mode
 * Output: 
 * Return: 
 * Notes: 
 */
void SetBusMode(PSDHCD_INSTANCE pHcInstance, PSDCONFIG_BUS_MODE_DATA pMode) 
{
    UINT32  control;    
    
    DBG_PRINT(SDDBG_TRACE, ("+SDIO Qualcomm HCD: SetMode\n"));

    control = 0;

    control = READ_MMC_REG(pHcInstance, MCI_CLK_REG);
    control &= ~((UINT32)(MCI_CLK_WIDEBUS_MASK));

    switch (SDCONFIG_GET_BUSWIDTH(pMode->BusModeFlags)) {
        case SDCONFIG_BUS_WIDTH_1_BIT:
             printk( KERN_ERR " SDIO Qualcomm HCD: SetMode - bus set to 1 bit mode\n");
	     control |= MCI_CLK_WIDEBUS_1BIT;
            break;        
        case SDCONFIG_BUS_WIDTH_4_BIT:
            printk( KERN_ERR " SDIO Qualcomm HCD: SetMode - bus set to 4 bit mode\n");
            control |= MCI_CLK_WIDEBUS_4BIT;
            break;
        case SDCONFIG_BUS_WIDTH_MMC8_BIT:
            DBG_PRINT(SDDBG_TRACE,
                      (" SDIO Qualcomm HCD: SetMode - "
                       "8 bit bus, NOT SUPPORT, width requested 0x%X\n",
                      pMode->BusModeFlags));
            break;
        default:
            DBG_PRINT(SDDBG_TRACE,
                      ("SDIO Qualcomm HCD: SetMode - "
                       "unknown bus width requested [0x%X]\n",
                      pMode->BusModeFlags));
            break;
    }

    WRITE_MMC_REG(pHcInstance, MCI_CLK_REG, control);
    DBG_PRINT(SDDBG_TRACE, ("-SDIO Qualcomm HCD: SetMode\n"));
}

/*
 * HcdConfig - HCD configuration handler
 * Input:  pHcd - HCD object
 *         pConfig - configuration setting
 * Output: 
 * Return: 
 * Notes: 
 */
SDIO_STATUS HcdConfig(PSDHCD pHcd, PSDCONFIG pConfig) 
{
    PSDHCD_INSTANCE pHcInstance = (PSDHCD_INSTANCE)pHcd->pContext; 
    SDIO_STATUS     status = SDIO_STATUS_SUCCESS;
    UINT16          command;
    
    if(pHcInstance->ShuttingDown) {
        DBG_PRINT(SDDBG_TRACE, ("SDIO Qualcomm HCD: HcdConfig canceled\n"));
        return SDIO_STATUS_CANCELED;
    }

    command = GET_SDCONFIG_CMD(pConfig);
        
    switch (command) {
        case SDCONFIG_GET_WP:
            *((SDCONFIG_WP_VALUE *)pConfig->pData) = 0;  
            break;
        case SDCONFIG_SEND_INIT_CLOCKS:
            DBG_PRINT(SDDBG_TRACE, (" SDIO Qualcomm HCD: HcdConfig - SDCONFIG_SEND_INIT_CLOCKS\n"));
            ClockStartStop(pHcInstance, CLOCK_ON);
            /* should be at least 80 clocks at our lowest clock setting */
            status = OSSleep(100);
            break;
        case SDCONFIG_SDIO_INT_CTRL:
            if (GET_SDCONFIG_CMD_DATA(PSDCONFIG_SDIO_INT_CTRL_DATA,pConfig)->SlotIRQEnable) {

                SDIO_IRQ_MODE_FLAGS irqModeFlags;
                
                irqModeFlags = GET_SDCONFIG_CMD_DATA(PSDCONFIG_SDIO_INT_CTRL_DATA,pConfig)->IRQDetectMode;

                if (irqModeFlags & IRQ_DETECT_4_BIT) {

                    DBG_PRINT(SDDBG_TRACE, (" SDIO Qualcomm HCD: 4 Bit IRQ mode\r\n")); 
                    /* in 4 bit mode, the clock needs to be left on */
                    pHcInstance->KeepClockOn = TRUE;

                    if (irqModeFlags & IRQ_DETECT_MULTI_BLK) {
                        DBG_PRINT(
                            SDDBG_TRACE,
                            ("SDIO Qualcomm HCD: 4 Bit Multi-block IRQ detection enabled\r\n"));
                    }
                } else {
                    /* in 1 bit mode, the clock can be left off */
                    pHcInstance->KeepClockOn = FALSE;   
                }                   

                /* enable detection */
                EnableDisableSDIOIRQ(pHcInstance,TRUE,FALSE); 
            } else {
                pHcInstance->KeepClockOn = FALSE; 
                EnableDisableSDIOIRQ(pHcInstance,FALSE,FALSE);
            }
            break;
        case SDCONFIG_SDIO_REARM_INT:
            /* re-enable IRQ detection */
            EnableDisableSDIOIRQ(pHcInstance,TRUE,FALSE);
            break;
        case SDCONFIG_BUS_MODE_CTRL:
            SetBusMode(pHcInstance, (PSDCONFIG_BUS_MODE_DATA)(pConfig->pData));
            break;
        case SDCONFIG_POWER_CTRL:
            DBG_PRINT(SDDBG_TRACE, (" SDIO Qualcomm HCD: POWER_CTRL En:[%d], VCC:[0x%X]\n",
                      GET_SDCONFIG_CMD_DATA(PSDCONFIG_POWER_CTRL_DATA,pConfig)->SlotPowerEnable,
                      GET_SDCONFIG_CMD_DATA(PSDCONFIG_POWER_CTRL_DATA,pConfig)->SlotPowerVoltageMask));
            status = SetPowerLevel(pHcInstance, 
                     GET_SDCONFIG_CMD_DATA(PSDCONFIG_POWER_CTRL_DATA,pConfig)->SlotPowerEnable,
                     GET_SDCONFIG_CMD_DATA(PSDCONFIG_POWER_CTRL_DATA,pConfig)->SlotPowerVoltageMask);
            break;
        case SDCONFIG_GET_HCD_DEBUG:
            *((CT_DEBUG_LEVEL *)pConfig->pData) = DBG_GET_DEBUG_LEVEL();
            break;
        case SDCONFIG_SET_HCD_DEBUG:
            DBG_SET_DEBUG_LEVEL(*((CT_DEBUG_LEVEL *)pConfig->pData));
            break;
        case SDCONFIG_ENABLE:
            DBG_PRINT(SDDBG_TRACE, (" SDIO Qualcomm HCD: HcdConfig - SDCONFIG_ENABLE\n"));
            hcd_data_trans_en(pHcd);
            break;
        default:
            /* invalid request */
            DBG_PRINT(SDDBG_ERROR,
                      (" SDIO Qualcomm HCD: HcdConfig - bad command:[0x%X]\n",
                      command));
            status = SDIO_STATUS_INVALID_PARAMETER;
    }
    
    return status;
} 

/*
 * SetPowerLevel - Set power level of board
 * Input:  pHcInstance - device context
 *         On - if true turns power on, else off
 *         Level - SLOT_VOLTAGE_MASK level
 * Output: 
 * Return: 
 * Notes: 
 */
SDIO_STATUS SetPowerLevel(PSDHCD_INSTANCE pHcInstance, BOOL On, SLOT_VOLTAGE_MASK Level) 
{
    pHcInstance->Hcd.MaxSlotCurrent = 400;  
    return SDIO_STATUS_SUCCESS;
}

/*
 * HcdInitialize - Initialize MMC controller
 * Input:  pHcInstance - device context
 * Output: 
 * Return: 
 * Notes: I/O resources must be mapped before calling this function
 */
SDIO_STATUS HcdInitialize(PSDHCD_INSTANCE pHcInstance) 
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
 
    DBG_PRINT(SDDBG_TRACE, ("+SDIO Qualcomm HCD: HcdInitialize\n"));
    
    if (0 == pHcInstance->BufferReadyWaitLimit) {
            /* initialize all these to defaults */
	    pHcInstance->BufferReadyWaitLimit = 50000;
	    pHcInstance->TransferCompleteWaitLimit = 100000;
	    pHcInstance->PresentStateWaitLimit = 30000;
	    pHcInstance->ResetWaitLimit = 30000;
    }

    pHcInstance->Hcd.MaxBytesPerBlock = 1024;
    pHcInstance->Hcd.MaxClockRate =  19500000;
    pHcInstance->Hcd.SlotVoltageCaps |= SLOT_POWER_3_3V;
    pHcInstance->Hcd.SlotVoltagePreferred = SLOT_POWER_3_3V;
    /* check host capabilities and back off some features */
    pHcInstance->Hcd.Attributes &= ~SDHCD_ATTRIB_SD_HIGH_SPEED;    

    DBG_PRINT(SDDBG_TRACE,
              (" SDIO Qualcomm HCD: HcdInitialize - SlotVoltageCaps:[0x%X]\n",
              (UINT)pHcInstance->Hcd.SlotVoltageCaps));

    DBG_PRINT(SDDBG_TRACE, ("-SDIO Qualcomm HCD: HcdInitialize\n"));
    return status;
}

/*
 * HcdDeinitialize - deactivate controller
 * Input:  pHcInstance - context
 * Output: 
 * Return: 
 * Notes:
 */
void HcdDeinitialize(PSDHCD_INSTANCE pHcInstance)
{
    DBG_PRINT(SDDBG_TRACE, ("+SDIO Qualcomm HCD: HcdDeinitialize\n"));
    pHcInstance->KeepClockOn = FALSE;
    pHcInstance->ShuttingDown = TRUE;
    DBG_PRINT(SDDBG_TRACE, ("-SDIO Qualcomm HCD: HcdDeinitialize\n"));
}

