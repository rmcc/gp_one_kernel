// Copyright (c) 2004-2006 Atheros Communications Inc.
// 
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

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@file: slot_tranceiver.c

@abstract: Linux I2C interface for TWL transceiver chip

#notes: Only required for OMAP 2.4 kernel.
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#include <ctsystem.h>
#include <linux/i2c.h>

static int DriverAttachAdapter(struct i2c_adapter *pAdapter);
static int DriverDetachClient(struct i2c_client *pClient);

static struct i2c_driver g_Driver = {
    .name       = "TWL Tranceiver",
    .flags      = I2C_DF_NOTIFY,
    .detach_client  = DriverDetachClient,
    .attach_adapter = DriverAttachAdapter,
};

typedef struct _TRANCEIVER_CONTROL {
    struct i2c_client I2CClient;
    BOOL   Initialized;
}TRANCEIVER_CONTROL, *PTRANCEIVER_CONTROL;

static TRANCEIVER_CONTROL g_Tranceiver;
static unsigned short normal_i2c[] = { 0x72, I2C_CLIENT_END };
static unsigned short normal_i2c_range[] = { I2C_CLIENT_END };
I2C_CLIENT_INSMOD;

BOOL WriteTranceiver(UINT8 Offset, UINT8 Value)
{
    s32 value;
    
	value = i2c_smbus_write_byte_data(&g_Tranceiver.I2CClient, Offset, Value);

    if (value == -1) {
        DBG_ASSERT(FALSE);
        return FALSE;    
    }
    
    return TRUE;
}

BOOL ReadTranceiver(UINT8 Offset, PUINT8 pValue)
{
    s32 value;
	value = i2c_smbus_read_byte_data(&g_Tranceiver.I2CClient, Offset);
    
    if (value == -1) {
        DBG_ASSERT(FALSE);
        return FALSE;    
    }
    
    *pValue = (UINT8)value;
    return TRUE; 
}
	
static int DriverDetachClient(struct i2c_client *pClient)
{
    DBG_PRINT(SDDBG_TRACE, 
                ("SDIO OMAP HCD: - DriverDetachClient \n"));
    g_Tranceiver.Initialized = FALSE;              
	i2c_detach_client(pClient);
	return 0;
}

static int DriverAttachAdapter(struct i2c_adapter *pAdapter)
{
	int err = 0;
    
    DBG_PRINT(SDDBG_TRACE, 
                ("SDIO OMAP HCD: - DriverAttachAdapter \n"));
                            
    do {
        if (g_Tranceiver.Initialized) {
                /* already in use */
            err = -ENODEV;
            break; 
        }
        
        if (!i2c_check_functionality(pAdapter, 
                                     I2C_FUNC_SMBUS_WORD_DATA | 
                                     I2C_FUNC_SMBUS_WRITE_BYTE)) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO OMAP HCD: - DriverAttachAdapter Failed\n"));
            err = -ENODEV;
            break;
        }
        
        memset(&g_Tranceiver,0,sizeof(g_Tranceiver));
        strcpy(g_Tranceiver.I2CClient.name, "TWLXcvr");
        g_Tranceiver.I2CClient.flags = 0;
        g_Tranceiver.I2CClient.addr = addr_data.normal_i2c[0];
        g_Tranceiver.I2CClient.driver = &g_Driver;
        g_Tranceiver.I2CClient.adapter = pAdapter;
        g_Tranceiver.I2CClient.id = 1;
        
        err = i2c_attach_client(&g_Tranceiver.I2CClient);               
    
        if (err != 0) {
            DBG_PRINT(SDDBG_ERROR, 
                ("SDIO OMAP HCD: - DriverAttachAdapter Failed (%d) \n",err)); 
            break;   
        }
        
        g_Tranceiver.Initialized = TRUE;
    } while (FALSE);

    return err;
}

BOOL SetupTranceiver(void)
{
	int err;
    
    DBG_PRINT(SDDBG_TRACE, ("SDIO OMAP HCD: - Setup TWL Transceiver \n"));
     
	err = i2c_add_driver(&g_Driver);
       
    if (err != 0) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO OMAP HCD: - Setup TWL Transceiver Failed: %d\n",
            err));
        return FALSE;    
    }
    
	return TRUE;
}

void CleanupTranceiver(void)
{
	int err;
	
    err = i2c_del_driver(&g_Driver);
	
    if (err != 0) {
        DBG_ASSERT(FALSE);     
    }
}

SDIO_STATUS SlotEnableControl(BOOL Enable)
{
    UINT8 value;
    SDIO_STATUS status = SDIO_STATUS_ERROR;
    
    DBG_PRINT(SDDBG_TRACE, ("SDIO OMAP HCD: - SlotEnableControl %s \n",
       Enable ? "Enable":"Disable"));
    
    do {
        
        if (!ReadTranceiver(0x10, &value)) {
            break;    
        }
        
        if (Enable) {
            value |= 0x03;   
        } else {
            value &= ~0x03;     
        }
        
        if (!WriteTranceiver(0x10, value)) {
            break;
        }
        
        if (!ReadTranceiver(0x38, &value)) {
            break;    
        }
        
        if (Enable) {
            value |= 0x01;   
        } else {
            value &= ~0x01;     
        }
        if (!WriteTranceiver(0x38, value)) {
            break;
        }
        
        status = SDIO_STATUS_SUCCESS;
    
    } while (FALSE);
     
    return status;
}


