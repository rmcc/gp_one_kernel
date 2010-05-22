/******************************************************************************
 *
 * @File:       sdio_qualcomm_hcd.h
 *
 * @Abstract:   Qualcomm SDIO Host Controller Driver Header File
 *
 * @Notice:     Copyright(c), 2008 Atheros Communications, Inc.
 *
 * $ATH_LICENSE_SDIOSTACK0$
 *
 *****************************************************************************/

#ifndef __SDIO_QUALCOMM_HCD_H___
#define __SDIO_QUALCOMM_HCD_H___

/* os-dependency */
#include "../../include/ctsystem.h"
#include "../../include/sdio_busdriver.h"
#include "../../include/sdio_lib.h"

#ifdef VXWORKS
#include "vxworks/sdio_hcd_vxworks.h"
#endif /* VXWORKS */

#ifdef QNX
#include "nto/sdio_hcd_nto.h"
#endif /* QNX */

#if defined(LINUX) || defined(__linux__)
#include "linux/sdio_qc_hcd_linux.h"
#endif /* LINUX */

#ifdef UNDER_CE
#include "wince/sdio_std_hcd_wince.h"
#endif /* UNDER_CE */

/* general macro */
#define CLOCK_ON                                 TRUE
#define CLOCK_OFF                                FALSE

/* register-related */
#define READ_MMC_REG(pC, OFFSET)                 \
        _READ_DWORD_REG((volatile UINT32*)(((UINT32)((pC)->pRegs) + (OFFSET))))

#define WRITE_MMC_REG(pC, OFFSET, VALUE)         \
        _WRITE_DWORD_REG((volatile UINT32*)((UINT32)((pC)->pRegs) + (OFFSET)), (VALUE))

/* for debug */
enum STD_HOST_TRACE_ENUM {
    STD_HOST_TRACE_CARD_INSERT = (SDDBG_TRACE + 1),
    STD_HOST_TRACE_DATA        = (SDDBG_TRACE + 2),       
    STD_HOST_TRACE_REQUESTS,  
    STD_HOST_TRACE_DATA_DUMP,  
    STD_HOST_TRACE_CONFIG,     
    STD_HOST_TRACE_INT,    
    STD_HOST_TRACE_CLOCK,
    STD_HOST_TRACE_SDIO_INT,
    STD_HOST_TRACE_LAST
};

/* irq register */
#define MSS_SDCC_INT_CLEAR_REG                   0x0
#define MSS_SDCC_INT_EN_REG                      0x4
#define MSS_SDCC_INT_STATUS_REG                  0x8
#define MSS_SDCC_INT_DETECT_CTL_REG              0xc
#define MSS_SDCC_INT_SDC1_IRQ0                   (1 << 0)
#define MSS_SDCC_INT_SDC1_IRQ1                   (1 << 1)

/* base address of irq */
#define MSS_BASE_PHYS                            0xB8000000
#define MSS_SEC_INTCTL_REG_OFFSET                0x0500
#define MSS_SEC_INTCTRL_REG_LENGTH               0x1000

/* MCI_POWER */
#define MCI_POWER_REG                            0x000
#define	MCI_POWER_CTRL_MASK                      0x003
#define MCI_POWER_CTRL_OFF                       0x0
#define	MCI_POWER_CTRL_UP                        0x2
#define	MCI_POWER_CTRL_ON                        0x3

/* MCI_CLK */
#define	MCI_CLK_REG                              0x004
#define	MCI_CLK_ENABLE_MASK                      0x0100
#define MCI_CLK_OFF                              0x0
#define	MCI_CLK_ON                               0x100
#define	MCI_CLK_PWRSAVE_MASK                     0x0200
#define MCI_CLK_PWRSAVE_OFF                      0x0
#define	MCI_CLK_PWRSAVE_ON                       0x200
#define	MCI_CLK_WIDEBUS_MASK                     0x0C00
#define MCI_CLK_WIDEBUS_1BIT                     0x0
#define	MCI_CLK_WIDEBUS_4BIT                     0x0800
#define	MCI_CLK_FLOWCTRL_MASK                    0x1000
#define MCI_CLK_FLOWCTRL_OFF                     0x0
#define	MCI_CLK_FLOWCTRL_ON                      0x1000

/* MCI_ARGUMENT */
#define	MCI_ARG_REG                              0x008

/* MCI_CMD */
#define	MCI_CMD_REG                              0x00C
#define HWIO_MCI_CMD_CCS_DISABLE_SHFT            0xf
#define HWIO_MCI_CMD_CCS_ENABLE_SHFT             0xe
#define HWIO_MCI_CMD_MCIABORT_SHFT               0xd
#define HWIO_MCI_CMD_DAT_CMD_SHFT                0xc
#define HWIO_MCI_CMD_PROG_ENA_SHFT               0xb
#define HWIO_MCI_CMD_ENABLE_SHFT                 0xa
#define HWIO_MCI_CMD_PENDING_SHFT                0x9
#define HWIO_MCI_CMD_INTERRUPT_SHFT              0x8
#define HWIO_MCI_CMD_LONGRSP_SHFT                0x7
#define HWIO_MCI_CMD_RESPONSE_SHFT               0x6
#define HWIO_MCI_CMD_CMD_INDEX_SHFT              0x0
#define MCI_CMD_MCIABORT                         (1UL << HWIO_MCI_CMD_MCIABORT_SHFT)
#define MCI_CMD_DAT_CMD                          (1UL << HWIO_MCI_CMD_DAT_CMD_SHFT)
#define MCI_CMD_RESPONSE                         (1UL << HWIO_MCI_CMD_RESPONSE_SHFT)
#define MCI_CMD_LONG_RESP                        (1UL << HWIO_MCI_CMD_LONGRSP_SHFT)
#define MCI_CMD_ENABLED                          (1UL << HWIO_MCI_CMD_ENABLE_SHFT)
#define MCI_CMD_PROG_ENABLED                     (1UL << HWIO_MCI_CMD_PROG_ENA_SHFT)

/* MCI_RESP_CMD */
#define	MCI_RESP_CMD_REG                         0x010
#define	MCI_RESP_REG                             0x014

/* MCI_DATA_TIMER */
#define	MCI_DATA_TIMER_REG                       0x024

/* MCI_DATA_LENGTH */
#define	MCI_DATA_LENGTH_REG                      0x028
#define	MCI_DATA_CTL_REG                         0x02C

/* MCI_DATA_CTL */
#define MCI_DATA_CTL_ENABLE                      0X1
#define MCI_DATA_CTL_RD_DIR                      (1 << 1)
#define MCI_DATA_CTL_WR_DIR                      0
#define MCI_DATA_CTL_BYTE_MODE                   (1 << 2)
#define MCI_DATA_CTL_DMA_EN                      (1 << 3)
#define MCI_DATA_CTL_BLK_SIZE_SHIFT              4

/* MCI_DATA_COUNT */
#define	MCI_DATA_COUNT_REG                       0x030

#define DATA_READY_MASK                          0x100

/* MCI_STATUS */
#define	MCI_STATUS_REG                           0x034
#define MCI_STATUS_CCSTimeOut                    (1 << 26)
#define MCI_STATUS_SDIOIntOper                   (1 << 25)
#define MCI_STATUS_AtaCmdCompl                   (1 << 24)
#define MCI_STATUS_ProgDone                      (1 << 23)
#define MCI_STATUS_SDIOInt      	  	 (1 << 22)
#define MCI_STATUS_RxDataAvlbl                   (1 << 21)
#define MCI_STATUS_TxDataAvlbl                   (1 << 20)
#define MCI_STATUS_RxFifoEmpty                   (1 << 19)
#define MCI_STATUS_TxFifoEmpty                   (1 << 18)
#define MCI_STATUS_RxFifoFull                    (1 << 17)
#define MCI_STATUS_TxFifoFull                    (1 << 16)
#define MCI_STATUS_RxFifoHalfFull                (1 << 15)
#define MCI_STATUS_TxFifoHalfFull                (1 << 14)
#define MCI_STATUS_RxActive                      (1 << 13)
#define MCI_STATUS_TxActive                      (1 << 12)
#define MCI_STATUS_CmdActive                     (1 << 11)
#define MCI_STATUS_DataBlockEnd                  (1 << 10)
#define MCI_STATUS_StartBitErr                   (1 << 9)
#define MCI_STATUS_DataEnd                       (1 << 8)
#define MCI_STATUS_CmdSent                       (1 << 7)
#define MCI_STATUS_CmdRespEnd                    (1 << 6)
#define MCI_STATUS_RxOverrun                     (1 << 5)
#define MCI_STATUS_TxOverrun                     (1 << 4)
#define MCI_STATUS_DataTimeOut                   (1 << 3)
#define MCI_STATUS_CmdTimeOut                    (1 << 2)
#define MCI_STATUS_DataCmdCrcFail                (1 << 1)
#define MCI_STATUS_CmdCrcFail                    (1 << 0)

#define MMC_RESP_ERRORS                          (MCI_STATUS_CmdCrcFail | MCI_STATUS_CmdTimeOut)
#define MMC_MAX_DATAEND_WAIT_CNT                 10000000

/* MCI_CLEAR */
#define	MCI_CLEAR_REG                            0x038

/* MCI_INT_MASK */
#define	MCI_INT_MASK_REG                         0x03C
#define MMC_MASK_CCSTimeOut                      (1 << 26)
#define MMC_MASK_SDIOIntOper                     (1 << 25)
#define MMC_MASK_AtaCmdCompl                     (1 << 24)
#define MMC_MASK_ProgDone                        (1 << 23)
#define MMC_MASK_SDIOInt                         (1 << 22)
#define MMC_MASK_RxDataAvlbl                     (1 << 21)
#define MMC_MASK_TxDataAvlbl                     (1 << 20)
#define MMC_MASK_RxFifoEmpty                     (1 << 19)
#define MMC_MASK_TxFifoEmpty                     (1 << 18)
#define MMC_MASK_RxFifoFull                      (1 << 17)
#define MMC_MASK_TxFifoFull                      (1 << 16)
#define MMC_MASK_RxFifoHalfFull                  (1 << 15)
#define MMC_MASK_TxFifoHalfFull                  (1 << 14)
#define MMC_MASK_RxActive                        (1 << 13)
#define MMC_MASK_TxActive                        (1 << 12)
#define MMC_MASK_CmdActive                       (1 << 11)
#define MMC_MASK_DataBlockEnd                    (1 << 10)
#define MMC_MASK_StartBitErr                     (1 << 9)
#define MMC_MASK_DataEnd                         (1 << 8)
#define MMC_MASK_CmdSent                         (1 << 7)
#define MMC_MASK_CmdRespEnd                      (1 << 6)
#define MMC_MASK_RxOverrun                       (1 << 5)
#define MMC_MASK_TxOverrun                       (1 << 4)
#define MMC_MASK_DataTimeOut                     (1 << 3)
#define MMC_MASK_CmdTimeOut                      (1 << 2)
#define MMC_MASK_DataCmdCrcFail                  (1 << 1)
#define MCI_MASK_CmdCrcFail                      (1 << 0)
#define MMC_MASK_ALL_INTS                        0x005807FF

#define MCI_STATUS_STATIC_MASK                   0x01C007FF

/* MCI_FIFO_COUNT */
#define	MCI_FIFO_COUNT_REG                       0x044

/* MCI_CCS_TIMER */
#define	MCI_CCS_TIMER_REG                        0x058

/* MCI_FIFO */
#define MCI_FIFO_SIZE                            (16 * 4)
#define	MCI_FIFO_REG                             0x080

/* MCI_PERPH_ID0 */
#define	MCI_PERPH_ID0_REG                        0x0E0

/* MCI_PERPH_ID1 */
#define	MCI_PERPH_ID1_REG                        0x0E4

/* MCI_PERPH_ID2 */
#define	MCI_PERPH_ID2_REG                        0x0E8

/* MCI_PERPH_ID3 */
#define	MCI_PERPH_ID3_REG                        0x0EC

/* MCI_PCELL_ID0 */
#define	MCI_PCELL_ID0_REG                        0x0F0

/* MCI_PCELL_ID1 */
#define	MCI_PCELL_ID1_REG                        0x0F4

/* MCI_PCELL_ID2 */
#define MCI_PCELL_ID2_REG                        0x0F8

/* MCI_PCELL_ID3 */
#define	MCI_PCELL_ID3_REG                        0x0FC

#define SD_DEFAULT_RESPONSE_BYTES                6
#define SD_R2_RESPONSE_BYTES                     16
#define SDIO_SD_MAX_BLOCKS                       ((UINT)0xFFFF)
#define SD_CLOCK_MAX_ENTRIES                     9

/* structure define */
typedef struct _SD_CLOCK_TBL_ENTRY {
    INT       ClockRateDivisor;                  /* divisor */
    UINT16    RegisterValue;                     /* register value for clock divisor */  
}SD_CLOCK_TBL_ENTRY;

/* standard host controller instance */
typedef struct _SDHCD_INSTANCE {
    SDLIST            List;                      /* list */
    SDHCD             Hcd;                       /* HCD structure for registration */
    SDDMA_DESCRIPTION DmaDescription;            /* dma description for this HCD if used*/
    UINT32            Caps;                      /* host controller capabilities */
#define SDHC_HW_INIT    0x01
#define SDHC_REGISTERED 0x02
    UINT8             InitStateMask;             /* init state for hardware independent layer */
    BOOL              CardInserted;              /* card inserted flag */
    BOOL              Cancel;                    /* cancel flag */
    BOOL              ShuttingDown;              /* indicates shut down of HCD */
    BOOL              StartUpCardCheckDone;
    BOOL              KeepClockOn;
    UINT32            BufferReadyWaitLimit;
    UINT32            TransferCompleteWaitLimit;
    UINT32            PresentStateWaitLimit;
    UINT32            ResetWaitLimit;
    BOOL              RequestCompleteQueued;
    PVOID             pRegs;                     /* a more direct pointer to the registers */
    UINT16            ClockConfigIdle;           /* clock configuration for idle in 4-bit mode*/
    UINT16            ClockConfigNormal;         /* clock configuration for normal operation */
    /* when the bus is idle, switch to 1 bit mode for IRQ detection */
    BOOL              Idle1BitIRQ;
    UINT16            ClockControlState;         /* copies of registers */
    UINT16            ErrIntSignalEn;
    UINT16            ErrIntStatusEn;
    UINT16            IntStatusEn;
    UINT16            IntSignalEn;
    SDHCD_OS_SPECIFIC OsSpecific;      
    struct clk        *pclk;
    struct clk        *mclk;
    PSDREQUEST        pReq;
} SDHCD_INSTANCE, *PSDHCD_INSTANCE;

/* prototypes */
SDIO_STATUS HcdRequest(PSDHCD pHcd);
SDIO_STATUS HcdConfig(PSDHCD pHcd, PSDCONFIG pReq);
SDIO_STATUS HcdInitialize(PSDHCD_INSTANCE pHcInstance);
void HcdDeinitialize(PSDHCD_INSTANCE pHcInstance);
SDIO_STATUS QueueEventResponse(PSDHCD_INSTANCE pHcInstance, INT WorkItemID);
void EnableDisableSDIOIRQ(PSDHCD_INSTANCE pHcInstance, BOOL Enable, BOOL FromIsr);
void HcdTransferDataDMAEnd(PSDHCD_INSTANCE pHcInstance, PSDREQUEST pReq);
SDIO_STATUS SetPowerLevel(PSDHCD_INSTANCE pHcInstance, BOOL On, SLOT_VOLTAGE_MASK Level); 
SDIO_STATUS ProcessCommandDone(PSDHCD_INSTANCE pHcInstance, PSDREQUEST pReq, BOOL FromIsr);
SDIO_STATUS sdcc_cmd_send_verify(PSDHCD_INSTANCE pHcInstance);
SDIO_STATUS sdcc_poll_cmd_response(PSDHCD_INSTANCE pHcInstance, UINT32 cmd);
void hcd_data_trans_en(PSDHCD);

#endif /* __SDIO_QUALCOMM_HCD_H___ */
