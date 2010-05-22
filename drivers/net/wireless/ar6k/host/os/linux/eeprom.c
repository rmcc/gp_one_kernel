/*
 * Copyright (c) 2006 Atheros Communications, Inc.
 * All rights reserved.
 * 
 *
 * 
// The software source and binaries included in this development package are
// licensed, not sold. You, or your company, received the package under one
// or more license agreements. The rights granted to you are specifically
// listed in these license agreement(s). All other rights remain with Atheros
// Communications, Inc., its subsidiaries, or the respective owner including
// those listed on the included copyright notices.  Distribution of any
// portion of this package must be in strict compliance with the license
// agreement(s) terms.
// </copyright>
// 
// <summary>
// 	Wifi driver for AR6002
// </summary>
//
 * 
 */

#include "ar6000_drv.h"
#include "htc.h"

#include "AR6002/hw/apb_map.h"
#include "AR6002/hw/gpio_reg.h"
#include "AR6002/hw/rtc_reg.h"
#include "AR6002/hw/si_reg.h"

//
// defines
//

#define MAX_FILENAME 1023
#define EEPROM_WAIT_LIMIT 16 

#define HOST_INTEREST_ITEM_ADDRESS(item)          \
        (AR6002_HOST_INTEREST_ITEM_ADDRESS(item))

#define EEPROM_SZ 768

//
// static variables
//

static A_UCHAR eeprom_data[EEPROM_SZ];
static A_UINT32 sys_sleep_reg;
static HIF_DEVICE *p_bmi_device;

//
// Functions
//

/* Read a Target register and return its value. */
inline void
BMI_read_reg(A_UINT32 address, A_UINT32 *pvalue)
{
    BMIReadSOCRegister(p_bmi_device, address, pvalue);
}

/* Write a value to a Target register. */
inline void
BMI_write_reg(A_UINT32 address, A_UINT32 value)
{
    BMIWriteSOCRegister(p_bmi_device, address, value);
}

/* Read Target memory word and return its value. */
inline void
BMI_read_mem(A_UINT32 address, A_UINT32 *pvalue)
{
    BMIReadMemory(p_bmi_device, address, (A_UCHAR*)(pvalue), 4);
}

/* Write a word to a Target memory. */
inline void
BMI_write_mem(A_UINT32 address, A_UINT8 *p_data, A_UINT32 sz)
{
    BMIWriteMemory(p_bmi_device, address, (A_UCHAR*)(p_data), sz); 
}

/*
 * Enable and configure the Target's Serial Interface
 * so we can access the EEPROM.
 */
static void
enable_SI(HIF_DEVICE *p_device)
{
    A_UINT32 regval;

    printk("%s\n", __FUNCTION__);

    p_bmi_device = p_device;

    BMI_read_reg(RTC_BASE_ADDRESS+SYSTEM_SLEEP_OFFSET, &sys_sleep_reg);
    BMI_write_reg(RTC_BASE_ADDRESS+SYSTEM_SLEEP_OFFSET, SYSTEM_SLEEP_DISABLE_SET(1)); //disable system sleep temporarily

    BMI_read_reg(RTC_BASE_ADDRESS+CLOCK_CONTROL_OFFSET, &regval);
    regval &= ~CLOCK_CONTROL_SI0_CLK_MASK;
    BMI_write_reg(RTC_BASE_ADDRESS+CLOCK_CONTROL_OFFSET, regval);

    BMI_read_reg(RTC_BASE_ADDRESS+RESET_CONTROL_OFFSET, &regval);
    regval &= ~RESET_CONTROL_SI0_RST_MASK;
    BMI_write_reg(RTC_BASE_ADDRESS+RESET_CONTROL_OFFSET, regval);


    BMI_read_reg(GPIO_BASE_ADDRESS+GPIO_PIN0_OFFSET, &regval);
    regval &= ~GPIO_PIN0_CONFIG_MASK;
    BMI_write_reg(GPIO_BASE_ADDRESS+GPIO_PIN0_OFFSET, regval);

    BMI_read_reg(GPIO_BASE_ADDRESS+GPIO_PIN1_OFFSET, &regval);
    regval &= ~GPIO_PIN1_CONFIG_MASK;
    BMI_write_reg(GPIO_BASE_ADDRESS+GPIO_PIN1_OFFSET, regval);

    /* SI_CONFIG = 0x500a6; */
    regval =    SI_CONFIG_BIDIR_OD_DATA_SET(1)  |
                SI_CONFIG_I2C_SET(1)            |
                SI_CONFIG_POS_SAMPLE_SET(1)     |
                SI_CONFIG_INACTIVE_CLK_SET(1)   |
                SI_CONFIG_INACTIVE_DATA_SET(1)   |
                SI_CONFIG_DIVIDER_SET(6);
    BMI_write_reg(SI_BASE_ADDRESS+SI_CONFIG_OFFSET, regval);
    
}

static void
disable_SI(void)
{
    A_UINT32 regval;
    
    printk("%s\n", __FUNCTION__);

    BMI_write_reg(RTC_BASE_ADDRESS+RESET_CONTROL_OFFSET, RESET_CONTROL_SI0_RST_MASK);
    BMI_read_reg(RTC_BASE_ADDRESS+CLOCK_CONTROL_OFFSET, &regval);
    regval |= CLOCK_CONTROL_SI0_CLK_MASK;
    BMI_write_reg(RTC_BASE_ADDRESS+CLOCK_CONTROL_OFFSET, regval);//Gate SI0 clock
    BMI_write_reg(RTC_BASE_ADDRESS+SYSTEM_SLEEP_OFFSET, sys_sleep_reg); //restore system sleep setting
}

/*
 * Tell the Target to start an 8-byte read from EEPROM,
 * putting the results in Target RX_DATA registers.
 */
static void
request_8byte_read(int offset)
{
    A_UINT32 regval;

//    printk("%s: request_8byte_read from offset 0x%x\n", __FUNCTION__, offset);

    
    /* SI_TX_DATA0 = read from offset */
        regval =(0xa1<<16)|
                ((offset & 0xff)<<8)    |
                (0xa0 | ((offset & 0xff00)>>7));
    
        BMI_write_reg(SI_BASE_ADDRESS+SI_TX_DATA0_OFFSET, regval);

        regval = SI_CS_START_SET(1)      |
                SI_CS_RX_CNT_SET(8)     |
                SI_CS_TX_CNT_SET(3);
        BMI_write_reg(SI_BASE_ADDRESS+SI_CS_OFFSET, regval);
}

/*
 * Tell the Target to start a 4-byte write to EEPROM,
 * writing values from Target TX_DATA registers.
 */
static void
request_4byte_write(int offset, A_UINT32 data)
{
    A_UINT32 regval;

    printk("%s: request_4byte_write (0x%x) to offset 0x%x\n", __FUNCTION__, data, offset);

        /* SI_TX_DATA0 = write data to offset */
        regval =    ((data & 0xffff) <<16)    |
                ((offset & 0xff)<<8)    |
                (0xa0 | ((offset & 0xff00)>>7));
        BMI_write_reg(SI_BASE_ADDRESS+SI_TX_DATA0_OFFSET, regval);

        regval =    data >> 16;
        BMI_write_reg(SI_BASE_ADDRESS+SI_TX_DATA1_OFFSET, regval);

        regval =    SI_CS_START_SET(1)      |
                SI_CS_RX_CNT_SET(0)     |
                SI_CS_TX_CNT_SET(6);
        BMI_write_reg(SI_BASE_ADDRESS+SI_CS_OFFSET, regval);
}

/*
 * Check whether or not an EEPROM request that was started
 * earlier has completed yet.
 */
static A_BOOL
request_in_progress(void)
{
    A_UINT32 regval;

    /* Wait for DONE_INT in SI_CS */
    BMI_read_reg(SI_BASE_ADDRESS+SI_CS_OFFSET, &regval);

//    printk("%s: request in progress SI_CS=0x%x\n", __FUNCTION__, regval);
    if (regval & SI_CS_DONE_ERR_MASK) {
        printk("%s: EEPROM signaled ERROR (0x%x)\n", __FUNCTION__, regval);
    }

    return (!(regval & SI_CS_DONE_INT_MASK));
}

/*
 * try to detect the type of EEPROM,16bit address or 8bit address
 */

static void eeprom_type_detect(void)
{
    A_UINT32 regval;
    A_UINT8 i = 0;

    request_8byte_read(0x100);
   /* Wait for DONE_INT in SI_CS */
    do{
        BMI_read_reg(SI_BASE_ADDRESS+SI_CS_OFFSET, &regval);
        if (regval & SI_CS_DONE_ERR_MASK) {
            printk("%s: ERROR : address type was wrongly set\n", __FUNCTION__);     
            break;
        }
        if (i++ == EEPROM_WAIT_LIMIT) {
            printk("%s: EEPROM not responding\n", __FUNCTION__);
        }
    } while(!(regval & SI_CS_DONE_INT_MASK));
}

/*
 * Extract the results of a completed EEPROM Read request
 * and return them to the caller.
 */
inline void
read_8byte_results(A_UINT32 *data)
{
    /* Read SI_RX_DATA0 and SI_RX_DATA1 */
    BMI_read_reg(SI_BASE_ADDRESS+SI_RX_DATA0_OFFSET, &data[0]);
    BMI_read_reg(SI_BASE_ADDRESS+SI_RX_DATA1_OFFSET, &data[1]);
}


/*
 * Wait for a previously started command to complete.
 * Timeout if the command is takes "too long".
 */
static void
wait_for_eeprom_completion(void)
{
    int i=0;

    while (request_in_progress()) {
        if (i++ == EEPROM_WAIT_LIMIT) {
            printk("%s: EEPROM not responding\n", __FUNCTION__);
        }
    }
}

/*
 * High-level function which starts an 8-byte read,
 * waits for it to complete, and returns the result.
 */
static void
fetch_8bytes(int offset, A_UINT32 *data)
{
    request_8byte_read(offset);
    wait_for_eeprom_completion();
    read_8byte_results(data);

    /* Clear any pending intr */
    BMI_write_reg(SI_BASE_ADDRESS+SI_CS_OFFSET, SI_CS_DONE_INT_MASK);
}

/*
 * High-level function which starts a 4-byte write,
 * and waits for it to complete.
 */
inline void
commit_4bytes(int offset, A_UINT32 data)
{
    request_4byte_write(offset, data);
    wait_for_eeprom_completion();
}



void eeprom_ar6000_transfer(HIF_DEVICE *device)
{
    A_UINT32 first_word;
    A_UINT32 board_data_addr;
    int i;

    printk("%s: Enter\n", __FUNCTION__);

    enable_SI(device);
    eeprom_type_detect();

    /*
     * Read from EEPROM to file OR transfer from EEPROM to Target RAM.
     * Fetch EEPROM_SZ Bytes of Board Data, 8 bytes at a time.
     */

    fetch_8bytes(0, (A_UINT32 *)(&eeprom_data[0]));

    /* Check the first word of EEPROM for validity */
    first_word = *((A_UINT32 *)eeprom_data);

    if ((first_word == 0) || (first_word == 0xffffffff)) {
    	printk("Did not find EEPROM with valid Board Data.\n");
    }

    for (i=8; i<EEPROM_SZ; i+=8) {
        fetch_8bytes(i, (A_UINT32 *)(&eeprom_data[i]));
    }


    /* Determine where in Target RAM to write Board Data */
    BMI_read_mem( HOST_INTEREST_ITEM_ADDRESS(hi_board_data), &board_data_addr);
    if (board_data_addr == 0) {
        printk("hi_board_data is zero\n");
    }

    /* Write EEPROM data to Target RAM */
    BMI_write_mem(board_data_addr, ((A_UINT8 *)eeprom_data), EEPROM_SZ);

    /* Record the fact that Board Data IS initialized */
    {
       A_UINT32 one = 1;
       BMI_write_mem(HOST_INTEREST_ITEM_ADDRESS(hi_board_data_initialized),
                     (A_UINT8 *)&one, sizeof(A_UINT32));
    }

    disable_SI();
}


