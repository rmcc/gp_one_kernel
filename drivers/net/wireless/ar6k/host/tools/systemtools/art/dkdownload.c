#include <stdio.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <signal.h>
/*
#ifdef _IQV
#include <windows.h>
#endif 
*/
#include "wlantype.h"
#include "dk_common.h"
#include "dk_cmds.h"
#include "test.h"

typedef struct _socRegData{
    A_UINT32 address;
    A_UINT32 value;
}SOC_REG_DATA;

typedef struct _socRegDataInfo{
    A_UINT32 numofpair;
    SOC_REG_DATA SOC_REG_DATA_ARRAY[100];
}SOC_REG_DATA_Info;

typedef struct _socMemData{
    A_UINT32 baseaddress;
    A_UINT8 byte[256];
    A_UINT32 blockLength;
}SOC_MEM_DATA;

typedef struct _socMemDataInfo{
    A_UINT32 numofMemBlock;
    SOC_MEM_DATA SOC_MEM_DATA_ARRAY[4];
}SOC_MEM_DATA_Info;

#define DK_DOWNLOAD_FILE1_0 "..\\..\\client\\dkdownload.sh"
#define DK_DOWNLOAD_FILE2_0 "..\\..\\client\\dkdownload2.sh"
#define DK_DOWNLOAD_FILE1_0twl "..\\..\\client\\dkdownload_twl.sh"
#define DK_DOWNLOAD_FILE2_0twl "..\\..\\client\\dkdownload2_twl.sh"

#define WAR1_SH_FILE "..\\..\\client\\AR6002.war1.sh"

#define BIN_PATH "..\\..\\client\\"

#define AR6002_VERSION_REV2 0x20000188

static char delimiters[]   = " \t";

SOC_REG_DATA_Info REG_DATA_Info;
SOC_MEM_DATA_Info MEM_DATA_Info;
static unsigned char device_bin[100*1024];
static unsigned int device_bin_len;

extern PIPE_CMD GlobalCmd;
extern CMD_REPLY cmdReply;
extern A_BOOL sdio_client;
extern A_BOOL com_client;


//function delcaration
static A_BOOL getDeviceBin(unsigned char *pfile);
static A_UINT32 art_bmiSocRegWrite(A_UINT8 devNum,HANDLE handle,A_UINT32 address, A_UINT32 value);
static A_UINT32 art_bmiSocMemWrite(A_UINT8 devNum,HANDLE handle,A_UINT32  address,
                                                        A_UCHAR *buffer,A_UINT32 length);
static A_UINT32 art_bmiSocRegRead(A_UINT8 devNum,HANDLE handle,A_UINT32 address,A_UINT32 *param);
A_UINT32 BmiOpeation(HANDLE sd_handle, A_UINT32 nTargetID);

/*
#ifdef	_IQV
HANDLE get_ene_handle(A_UINT32 device_fn);
A_BOOL close_ene_handle(void);
DWORD ene_DRG_Write(HANDLE COM_Write,  PUCHAR buf, ULONG length);
DWORD ene_DRG_Read(HANDLE pContext,  PUCHAR buf, ULONG length,  PULONG pBytesRead);

typedef A_BOOL	(CALLBACK* PROC_LOAD)(A_BOOL,A_UINT32);
typedef A_BOOL	(CALLBACK* PROC_INIT)(HANDLE*,A_UINT32*);
typedef int		(CALLBACK* PROC_BMIReadSOCRegister)(HANDLE, A_UINT32, A_UINT32*);
typedef int		(CALLBACK* PROC_BMIWriteSOCRegister)(HANDLE, A_UINT32, A_UINT32);
typedef int		(CALLBACK* PROC_BMIWriteMemory)(HANDLE, A_UINT32, A_UCHAR*, A_UINT32);
typedef int		(CALLBACK* PROC_BMIDone)(HANDLE);
typedef HANDLE	(CALLBACK* PROC_open_device_ene)(A_UINT32, A_UINT32, char*);
typedef A_BOOL	(CALLBACK* PROC_close_ene_handle)(void);
typedef DWORD	(CALLBACK* PROC_DRG_Write)(HANDLE, PUCHAR, ULONG);
typedef DWORD	(CALLBACK* PROC_DRG_Read)(HANDLE, PUCHAR, ULONG, PULONG);

PROC_LOAD					call_load;
PROC_INIT					call_init;
PROC_BMIReadSOCRegister		call_BMIReadSOCRegister;
PROC_BMIWriteSOCRegister	call_BMIWriteSOCRegister;
PROC_BMIWriteMemory			call_BMIWriteMemory;
PROC_BMIDone				call_BMIDone;
PROC_open_device_ene		call_open_device_ene;
PROC_close_ene_handle		call_close_ene_handle;
PROC_DRG_Write				call_DRG_Write;
PROC_DRG_Read				call_DRG_Read;

A_BOOL	devdrvRtn;
HINSTANCE h_devdrv;
#endif	// _IQV
*/
static A_BOOL getRefClock_bin(unsigned char *pFile,SOC_MEM_DATA *pdata)
{
    FILE *fStream;
    unsigned char temp;
    int i;

    if( (fStream = fopen(pFile, "rb")) == NULL ) {
        uiPrintf("Failed to open file: %s \n", pFile);
        return FALSE;
    }

    i=0;
    while(fread(&temp,1,1,fStream)!=0)
    {
        pdata->byte[i]=temp;
        //uiPrintf("%x\t",pdata->byte[i]);
        i++;
    }

    pdata->blockLength = i;
    uiPrintf("\npdata->blockLength = %d\n",pdata->blockLength);
    
    fclose(fStream);
    
    return TRUE;   
}

static A_UINT32 getLengthOfBin(unsigned char *pFile)
{
    FILE *fStream;
    unsigned char temp;
    int i;

    if( (fStream = fopen(pFile, "rb")) == NULL ) {
        uiPrintf("Failed to open file: %s \n", pFile);
		return 0;
    }

    i=0;
    while(fread(&temp,1,1,fStream)!=0)
    {
        //uiPrintf("%x\t",temp);
        i++;
    }
    //uiPrintf("\npdata->blockLength = %d\n",i);
    fclose(fStream);
    return i;   
}


static A_UINT32 getContentOfBin(unsigned char* pFile, unsigned char* buf)
{
    FILE *fStream;
    unsigned char temp;
    int i;

    if( (fStream = fopen(pFile, "rb")) == NULL ) {
        uiPrintf("Failed to open file: %s \n", pFile);
		return FALSE;
    }
    i=0;
    while(fread(&temp,1,1,fStream)!=0)
    {
        buf[i] = temp;
        i++;
    }

    fclose(fStream);
    return TRUE;
}

static A_UINT32 parseExecWarSh(A_UINT8 devNum, HANDLE handle)
{

    FILE *fwarsh;
    char lineBuf[222], *pLine;
    A_UINT32 testVal;
    A_UINT32 address;
    A_UINT32 length;
    unsigned char *pbuf;
    
    int i;
    unsigned char index=0;
    char fileBuf[222];
    char pfile[222];

    if( (fwarsh = fopen(WAR1_SH_FILE, "r")) == NULL ) {
        uiPrintf("Failed to open file: %s \n", WAR1_SH_FILE);
		return FALSE;
    }

    while(fgets(lineBuf, 120, fwarsh) != NULL) 
    {
        pLine = lineBuf;
        while(isspace(*pLine)) pLine++;
        if(*pLine == '#') {
            continue;
        }        
        if(strnicmp("sleep", pLine, strlen("sleep")) == 0) 
        {
            pLine +=strlen("sleep");
            pLine = strtok( pLine, delimiters ); //get past any white space etc
            if (!sscanf(pLine, "%d", &(testVal))) {
				uiPrintf("sleep Error[1]: %s\n",pLine);
				return FALSE;
			}else{      
                    uiPrintf("---> sleep %d s\n",testVal);
                    Sleep(testVal*1000);
            }
        }
        if(strnicmp("$IMAGEPATH/bmiloader -i $NETIF --set", pLine, strlen("$IMAGEPATH/bmiloader -i $NETIF --set")) == 0) 
        {
            pLine += strlen("$IMAGEPATH/bmiloader -i $NETIF --set");
            pLine = strtok( pLine, delimiters ); //get past any white space etc

            //SOC reg write
            if(strnicmp("--address=", pLine, strlen("--address=")) == 0) {
                pLine += strlen("--address=");
                if (!sscanf(pLine, "%x", &(address))) {
                    uiPrintf("Coul Error[1]: %s\n",pLine);
                    return FALSE;
                }else{      
                    //uiPrintf("SOC ----> 0x%X   ",address);
                }
                pLine = strtok( NULL, delimiters ); //get past any white space etc
                if(strnicmp("--param=", pLine, strlen("--param=")) == 0){
                    pLine += strlen("--param=");
                    if (!sscanf(pLine, "%x", &(testVal))) {
                        uiPrintf("Coul Error[2]: %s\n",pLine);
                        return FALSE;
                    }else{      
                     //   uiPrintf("--->val:  0x%X   \n",testVal);
                    }
                    if (!art_bmiSocRegWrite(devNum,handle,address,testVal))
						return FALSE;
                }
            }           
        }           
    }
    fclose(fwarsh);
	return TRUE;
}

static A_UINT32 execWar1Sh(A_UINT8 devNum, HANDLE handle)
{
    A_UINT32 address;
    A_UINT32 save_sleep,save_options;
    
    uiPrintf("Install AR6002 LF Clock Calibration WAR \n");
    /*
    # Step 1: Use remap entry 31 to nullify firmware's initialization
    # of LPO_CAL_TIME at 0x8e1027
    # Step 1a: Load modified text at end of memory
    */
    if (!art_bmiSocRegWrite(devNum,handle,0x52dfe0,0x00239100))
		return FALSE;
    if (!art_bmiSocRegWrite(devNum,handle,0x52dfe4,0xf0102282))
		return FALSE;
    if (!art_bmiSocRegWrite(devNum,handle,0x52dfe8,0x23a20020))
		return FALSE;
    if (!art_bmiSocRegWrite(devNum,handle,0x52dfec,0x0008e09e))
		return FALSE;
    if (!art_bmiSocRegWrite(devNum,handle,0x52dff0,0xc0002441))
		return FALSE;
    if (!art_bmiSocRegWrite(devNum,handle,0x52dff4,0x24a20020))
		return FALSE;
    if (!art_bmiSocRegWrite(devNum,handle,0x52dff8,0x046a67b0))
		return FALSE;
    if (!art_bmiSocRegWrite(devNum,handle,0x52dffc,0x63b20b0c))
		return FALSE;
    //# Step 1b: Write remap size
    if (!art_bmiSocRegWrite(devNum,handle,0x80fc,0x0))
		return FALSE;
    //# Step 1c: Write compare address
    if (!art_bmiSocRegWrite(devNum,handle,0x817c,0x4e1020))
		return FALSE;
    //# Step 1d: Write target address
    if (!art_bmiSocRegWrite(devNum,handle,0x81fc,0x12dfe0))
		return FALSE;
    //# Step 1e: Write valid bit
    if (!art_bmiSocRegWrite(devNum,handle,0x807c,0x1))
		return FALSE;
    /*
    # Step 2: Use remap entry 30 to nullify firmware's initialization
    # of LPO_CAL_TIME at 0x8e12f7
    # Step 2a: Load modified text at end of memory
    */
    if (!art_bmiSocRegWrite(devNum,handle,0x52dfc0,0x00203130))
		return FALSE;
    if (!art_bmiSocRegWrite(devNum,handle,0x52dfc4,0x220020c0))
		return FALSE;
    if (!art_bmiSocRegWrite(devNum,handle,0x52dfc8,0x63f2b76e))
		return FALSE;
    if (!art_bmiSocRegWrite(devNum,handle,0x52dfcc,0x0c0a0c9e))
		return FALSE;
    if (!art_bmiSocRegWrite(devNum,handle,0x52dfd0,0x21a4d20b))
		return FALSE;
    if (!art_bmiSocRegWrite(devNum,handle,0x52dfd4,0xf00020c0))
		return FALSE;
    if (!art_bmiSocRegWrite(devNum,handle,0x52dfd8,0x34c10020))
		return FALSE;
    if (!art_bmiSocRegWrite(devNum,handle,0x52dfdc,0x00359100))
		return FALSE;
    //# Step 2b: Write remap size
    if (!art_bmiSocRegWrite(devNum,handle,0x80f8,0x0))
		return FALSE;
    //# Step 2c: Write compare address
    if (!art_bmiSocRegWrite(devNum,handle,0x8178,0x4e12e0))
		return FALSE;
    //# Step 2d: Write target address
    if (!art_bmiSocRegWrite(devNum,handle,0x81f8,0x12dfc0))
		return FALSE;
    //# Step 2e: Write valid bit
    if (!art_bmiSocRegWrite(devNum,handle,0x8078,0x1))
		return FALSE;
    /*
    # Step 3: Use remap entry 29 to nullify firmware's initialization
    # of host_interest at 0x8e1384
    # Step 3a: Load modified text at end of memory
    */
    if (!art_bmiSocRegWrite(devNum,handle,0x52dfa0,0x04a87685))
		return FALSE;
    if (!art_bmiSocRegWrite(devNum,handle,0x52dfa4,0x1b0020f0))
		return FALSE;
    if (!art_bmiSocRegWrite(devNum,handle,0x52dfa8,0x00029199))
		return FALSE;
    if (!art_bmiSocRegWrite(devNum,handle,0x52dfac,0x3dc0dd90))
		return FALSE;
    if (!art_bmiSocRegWrite(devNum,handle,0x52dfb0,0x04ad76f0))
		return FALSE;
    if (!art_bmiSocRegWrite(devNum,handle,0x52dfb4,0x1b0049a2))
		return FALSE;
    if (!art_bmiSocRegWrite(devNum,handle,0x52dfb8,0xb19a0c99))
		return FALSE;
    if (!art_bmiSocRegWrite(devNum,handle,0x52dfdc,0x8ce5003c))
		return FALSE;
    //# Step 3b: Write remap size
    if (!art_bmiSocRegWrite(devNum,handle,0x80f4,0x0))
		return FALSE;
    //# Step 3c: Write compare address
    if (!art_bmiSocRegWrite(devNum,handle,0x8174,0x4e1380))
		return FALSE;
    //# Step 3d: Write target address
    if (!art_bmiSocRegWrite(devNum,handle,0x81f4,0x12dfa0))
		return FALSE;
    //# Step 3e: Write valid bit
    if (!art_bmiSocRegWrite(devNum,handle,0x8074,0x1))
		return FALSE;
    #if 1
    /*
    # Step 4: Reset.
    # Step 4a: Save state that may have been changed since reset.
    */
    if (!art_bmiSocRegRead(devNum,handle,0x40c4,&save_sleep))
		return FALSE;
    if (!art_bmiSocRegRead(devNum,handle,0x180c0,&save_options))
		return FALSE;
    //# Step 4b: Issue reset.
    if (!art_bmiSocRegWrite(devNum,handle,0x4000,0x100))
		return FALSE;

    //# Step 4c: Sleep while reset occurs - TBDXXX sleep 1
    Sleep(1000);
    //# Step 4d: Restore state that may have been changed since reset.
    if (!art_bmiSocRegWrite(devNum,handle,0x40c4,save_sleep))
		return FALSE;
    if (!art_bmiSocRegWrite(devNum,handle,0x180c0,save_options))
		return FALSE;
    #endif
    /*
    # Step 5: Reclaim remap entries 29 and 30, since they are no longer needed.
    # (Could reclaim remap entry 31 later, if needed.)
    */
    if (!art_bmiSocRegWrite(devNum,handle,0x8074,0x0))
		return FALSE;
    if (!art_bmiSocRegWrite(devNum,handle,0x8078,0x0))
		return FALSE;

    /*
    # Step 6: Write LPO_INIT_DIVIDEND_INT. 
    # TBDXXX: This value is hardcoded for a 26MHz reference clock
    */
    if (!art_bmiSocRegWrite(devNum,handle,0x40d8,0x2e7ddb))
		return FALSE;
    /*
    # Step 7: Write LPO_INIT_DIVIDEND_FRACTION.
    # TBDXXX: This value is hardcoded for a 26MHz reference clock
    */
    if (!art_bmiSocRegWrite(devNum,handle,0x40dc,0x0))
		return FALSE;
    uiPrintf("Done installing AR6002 LF Clock calibration WAR \n");
    return TRUE;
}

static A_UINT32 parseExecDkdownload(A_UINT8 devNum, HANDLE handle, A_UINT32 nTargetID)
{

    FILE *fdkdownload;
    char lineBuf[222], *pLine;
    A_UINT32 testVal;
    A_UINT32 address;
    A_UINT32 length;
    unsigned char *pbuf;
    
    int i;
    unsigned char index=0;
    char fileBuf[222];
    char pfile[222];

    REG_DATA_Info.numofpair=0;
    MEM_DATA_Info.numofMemBlock = 0;

	if (AR6002_VERSION_REV2 == nTargetID ) {
	    if(com_client){
	        if( (fdkdownload = fopen(DK_DOWNLOAD_FILE2_0twl, "r")) == NULL ) {
	            uiPrintf("Failed to open file: %s \n", DK_DOWNLOAD_FILE2_0);
	            return FALSE;
			}
	    }else{
		if( (fdkdownload = fopen(DK_DOWNLOAD_FILE2_0, "r")) == NULL ) {
	            uiPrintf("Failed to open file: %s \n", DK_DOWNLOAD_FILE2_0);
	            return FALSE;
			}
         }
	}

    while(fgets(lineBuf, 120, fdkdownload) != NULL) 
    {
        pLine = lineBuf;
        while(isspace(*pLine)) pLine++;
        if(*pLine == '#') {
            continue;
        }        

        if(strnicmp("$BMILOADER", pLine, strlen("$BMILOADER")) == 0) 
        {
            pLine += strlen("$BMILOADER");
            pLine = strtok( pLine, delimiters ); //get past any white space etc

            //SOC reg write
            if(strnicmp("-s", pLine, strlen("-s")) == 0) {
                pLine = strtok( NULL, delimiters ); //get past any white space etc
                if(strnicmp("-a", pLine, strlen("-a")) == 0){
                    pLine = strtok( NULL, delimiters );
                    if (!sscanf(pLine, "%x", &(address))) {
                        uiPrintf("Coul Error[1]: %s\n",pLine);
			            return FALSE;
				     }else{      
                      // uiPrintf("SOC ----> 0x%X   ",address);
                    }

                    pLine = strtok( NULL, delimiters ); //get past any white space etc
                    if(strnicmp("-p", pLine, strlen("-p")) == 0){
                        pLine = strtok( NULL, delimiters );
                        if (!sscanf(pLine, "%x", &(testVal))) {
                            uiPrintf("Coul Error[2]: %s\n",pLine);
				            return FALSE;
                        }else{      
                            //uiPrintf("----> 0x%X   \n",testVal);
                        }
                        if (!art_bmiSocRegWrite(devNum,handle,address,testVal))
							return FALSE;
                    }
                }     
            }           

            //SOC Mem write
            if(strnicmp("-w", pLine, strlen("-w")) == 0) {
                pLine = strtok( NULL, delimiters ); //get past any white space etc
                if(strnicmp("-a", pLine, strlen("-a")) == 0){
                    pLine = strtok( NULL, delimiters );
                    if (!sscanf(pLine, "%x", &(address))) {
                        uiPrintf("Coul Error[3]: %s\n",pLine);
                        return FALSE;
					}else{      
                        //uiPrintf("Mem ----> 0x%X   ",address);
                    }           

                    pLine = strtok( NULL, delimiters ); //get past any white space etc
                    if(strnicmp("-f", pLine, strlen("-f")) == 0){
                        pLine = strtok( NULL, delimiters );
                        if (!sscanf(pLine, "%s",pfile)) {
                            uiPrintf("Coul Error[4]: %s\n",pLine);
                        }else
                        {      
                            //uiPrintf("----> %s   \n",pfile);                       
                        }
                        sscanf(BIN_PATH, "%s", fileBuf);
                        strncat(fileBuf,pfile,strlen(pfile));    
                        length = getLengthOfBin(fileBuf);
						if (length == 0)
							return FALSE;
                        pbuf = (unsigned char *)malloc(length);
                        if(pbuf == NULL)
                        {
                            uiPrintf("malloc %d failed\n",length);
                            return FALSE;
                        }
                        if (!getContentOfBin(fileBuf,pbuf))
							return FALSE;
                        if (!art_bmiSocMemWrite(devNum,handle,address,pbuf,length))
							return FALSE;
                        free(pbuf);
                    }                   
                }                
            }             
        }           
    }
    
    fclose(fdkdownload);
	return TRUE;
}



A_UINT32 BmiOpDone(A_UINT8 devNum,HANDLE handle)

{

        int status;
        A_UINT32 nTargetID;
        A_UINT32 param;
        
#if 1        
        // the common configurations for sdio_client and com_client
#if 0
       param = 1;
        if (!art_bmiSocMemWrite(devNum,handle,
                           AR6002_HOST_SETTING_ITEM_ADDRESS(hi_serial_enable),
                           (A_UCHAR *)&param,
                           4))
			   return FALSE;
        uiPrintf("Serial console prints enabled\n");
#endif

        param = 1;
        if (!art_bmiSocMemWrite(0,handle,
                           AR6002_HOST_SETTING_ITEM_ADDRESS(hi_board_data_initialized),
                           (A_UCHAR *)&param,
                           4))
			   return FALSE;
        uiPrintf("hi_board_data_initialized set\n");

        if(sdio_client)
        {
        param = 1;
        if (!art_bmiSocMemWrite(0,handle,
                   AR6002_HOST_SETTING_ITEM_ADDRESS(hi_app_host_interest),
                   (A_UCHAR *)&param,
                   4))
		   return FALSE;
        uiPrintf("HTC1 (hi_app_host_interest) enable\n");
        }       
#endif    

    if(sdio_client)
    {
        //BMISetAppStart(handle, 0x80002000);

     //  BMISetAppStart(handle, 0x915000);      
 //      uiPrintf("press key to go!\n");getchar();
//#ifndef _IQV
        status = BMIDone(handle);
//#else
//		status = call_BMIDone(handle);
//#endif	// _IQV
        if(status != A_OK)
        {
            uiPrintf("Failed BMIDone\n");
            return FALSE;
        }
        else
        {
            uiPrintf("BMI Done success\n");
        } 
        return TRUE;
    }

    if(com_client)
    {
        GlobalCmd.cmdID = ART_BMI_OP_DONE_ID;
        GlobalCmd.devNum = devNum;
        if(!artSendCmd(&GlobalCmd,
                                    sizeof(GlobalCmd.cmdID),
                                    NULL)) 
        {
            uiPrintf("Error: Unable to successfully send BmiOpDone command\n");
            return FALSE;
        }
    }
    return TRUE;
}



void loadTarget_ENE(void)
{
    HANDLE sd_handle;
	A_UINT32 nTargetID;

	if(!InitAR6000_ene(&sd_handle, &nTargetID)) {
        uiPrintf("Error: Failed InitAR6000_ene\n");
        return FALSE;
	}
    //printf("---->sd_handle = 0x%x\n",sd_handle);
    if (!BmiOpeation(sd_handle, nTargetID))
		return FALSE;
	return TRUE;
/*
#else
    HANDLE sd_handle;
	A_UINT32 nTargetID;

	if (h_devdrv != NULL) 
	{ 
		devdrvRtn = call_init(&sd_handle, &nTargetID);
		if (!devdrvRtn) {
			printf("Error calling InitAR6000_ene\n");
			return FALSE;
		}
		//printf("---->sd_handle = 0x%x\n",sd_handle);
		if (!BmiOpeation(sd_handle, nTargetID))
			return FALSE;
	}
#endif	// _IQV
*/
}


A_UINT32 BmiOpeation(HANDLE sd_handle, A_UINT32 nTargetID)
{

    int i;
    A_UINT32 status;
    A_UINT32 sleep,old_sleep;
    A_UINT32 address;
    A_UINT32 save_options,save_address;
    
    //sd_handle no used for USB_NITRO ART

#if 1
    address = 0x40c4; 
    if (!art_bmiSocRegRead(0,sd_handle,
                   address,
                   &old_sleep))
				   return FALSE;
    
#ifdef _DEBUG    
    uiPrintf("old_sleep =0x%x\n",old_sleep);
#endif

    if (!art_bmiSocRegWrite(0,sd_handle,
                    address,
                    old_sleep|1))   //to make it awake
		return FALSE;
    if (!art_bmiSocRegRead(0,sd_handle,
                   RTC_BASE_ADDRESS+RESET_CONTROL_OFFSET,
                   &sleep))
		return FALSE;
#ifdef _DEBUG    
    uiPrintf("-----> RESET =0x%x\n",sleep);
#endif

    /* 
     * make sure all module wakeup, 
     * different reset register init vale
     * between Dragon and Mercury.
     * We only disable SI here     
     */
    if (!art_bmiSocRegWrite(0,sd_handle,
                   RTC_BASE_ADDRESS+RESET_CONTROL_OFFSET,
                   RESET_CONTROL_SI0_RST_MASK))
		return FALSE;
    uiPrintf("Reset control register set to 0x%x\n", RESET_CONTROL_SI0_RST_MASK);
#endif    

    if (!parseExecDkdownload(0,sd_handle, nTargetID))  //by now, we only parse the func of "-s -a -p " and "-w -a -f" in dkdownload.sh
		return FALSE;
    //if (!downloadTarget(0,sd_handle)) return FALSE;

    if (com_client)
    {
	GlobalCmd.cmdID = ART_IMAGE_DOWNLOAD_ID;
	GlobalCmd.devNum = 0;
	if(!artSendCmd(&GlobalCmd,0, NULL))
	{
	    uiPrintf("Error: Unable to successfully send  GlobalCmd.cmdID->%d\n",GlobalCmd.cmdID);
	    return FALSE;
	}
    }
    if (!art_bmiSocRegWrite(0,sd_handle,
                address,
                old_sleep))
		return FALSE;
    if (!BmiOpDone(0,sd_handle))
		return FALSE;

  // DisableDragonSleep(sd_handle);
	return TRUE;
}

static A_UINT32   
art_bmiSocRegRead(A_UINT8 devNum,HANDLE handle,A_UINT32 address,A_UINT32 *param)
{
    A_UINT32 *pretbuf = NULL;
    
    if(sdio_client)
    {
//#ifndef _IQV
        if(BMIReadSOCRegister(handle,address,param) != 0)
//#else
//		if ( call_BMIReadSOCRegister(handle,address,param) != 0)
//#endif	// _IQV
        {
            uiPrintf("Error when  art_bmiSocRegRead: address=0x%X\n",address);
            return FALSE;
        }
        return TRUE;
    }
    
    if(com_client)
    {
        GlobalCmd.cmdID = ART_BMI_READ_SOC_REGISTER_ID;
        GlobalCmd.devNum = devNum;
        GlobalCmd.CMD_U.REG_READ_CMD.readAddr = address;
        if(!artSendCmd(&GlobalCmd,
                                    sizeof(GlobalCmd.CMD_U.REG_READ_CMD)+sizeof(GlobalCmd.cmdID),
                                                       (void **)&pretbuf))       
        {
            uiPrintf("Error when  art_bmiSocRegRead: address=0x%X\n",address);
            return FALSE;
        }
        
        if(pretbuf != NULL)
            *param = *pretbuf;
        return TRUE;
    }
    
}

static A_UINT32 art_bmiSocRegWrite(A_UINT8 devNum,HANDLE handle,A_UINT32 address, A_UINT32 value)
{
    int status;

#if 1
    uiPrintf("BMI Write Register (address: 0x%x, param: 0x%x)\n",address,value);
#endif

    if(sdio_client)
    {
//#ifndef _IQV
		status=BMIWriteSOCRegister(handle,address,value);
//#else
//		status=call_BMIWriteSOCRegister(handle,address,value);
//#endif	// _IQV
        if(status != A_OK)
        {
		uiPrintf("Failed BMIWriteSOCRegister ad=0x%X, value=0x%X\n",address,value);
		return FALSE;
        }
        return TRUE;
    }

    if (com_client)
    {
        GlobalCmd.cmdID = ART_BMI_WRITE_SOC_REGISTER_ID;
        GlobalCmd.devNum = devNum;
        GlobalCmd.CMD_U.ART_BMI_WRITE_SOC_REGISTER_CMD.address = address;
        GlobalCmd.CMD_U.ART_BMI_WRITE_SOC_REGISTER_CMD.value = value;

        if(!artSendCmd(&GlobalCmd,
                                    sizeof(GlobalCmd.CMD_U.ART_BMI_WRITE_SOC_REGISTER_CMD)+sizeof(GlobalCmd.cmdID),
                                    NULL)) 
        {
            uiPrintf("Error: Unable to successfully send art_bmiSocRegWrite command\n");
            return FALSE;
        }
    }
	return TRUE;
}



static A_UINT32 art_bmiSocMemWrite(A_UINT8 devNum,HANDLE handle,A_UINT32  address,
                                                        A_UCHAR *buffer,A_UINT32 length)
{
    int i;
    A_UINT32 *pRet;
    int status;
    A_UINT32 timeSt,timeEnd;
    
#if 1	
    uiPrintf("BMI Write Memory: Enter (address: 0x%x, length: %d)\n",address, length);
#endif

    if(sdio_client)
    {
//#ifndef _IQV
        status = BMIWriteMemory(handle,address,buffer,length);
//#else
//		status=call_BMIWriteMemory(handle,address,buffer,length);
//#endif	// _IQV
        if(status != A_OK)
        {
			uiPrintf("Failed BMIWriteMemory ad=0x%X\n",address);
			return FALSE;
        }
        return TRUE;
    }      

    if(com_client) //if the length is bigger than 2048 byte, then split 
    {
        timeSt=milliTime();
        while(length)
        {
            //uiPrintf("sending device_bin remain len = %d cnt = %d\n",length,kk_cnt++);
            if(length >  2048){
                GlobalCmd.cmdID = ART_BMI_WRITE_SOC_MEMORY_ID;
                GlobalCmd.devNum = devNum;
                GlobalCmd.CMD_U.ART_BMI_WRITE_SOC_MEMORY_CMD.baseaddress=address;
                GlobalCmd.CMD_U.ART_BMI_WRITE_SOC_MEMORY_CMD.blockLength=2048;

                memcpy(GlobalCmd.CMD_U.ART_BMI_WRITE_SOC_MEMORY_CMD.byte,buffer, 2048);
                if(!artSendCmd(&GlobalCmd,2048
                                        +sizeof(GlobalCmd.CMD_U.ART_BMI_WRITE_SOC_MEMORY_CMD.baseaddress)
                                        +sizeof(GlobalCmd.CMD_U.ART_BMI_WRITE_SOC_MEMORY_CMD.blockLength)
                                        +sizeof(GlobalCmd.cmdID), (void **)&pRet)) 
                {
					uiPrintf("Error: Unable to successfully send  GlobalCmd.cmdID->%d\n",GlobalCmd.cmdID);
					return FALSE;
                }
                //judgeDownloadTarget((A_UINT8)*pRet,length);
                length -= 2048;
                buffer += 2048;
                address += 2048;
                uiPrintf(". ");
                continue;
            }
            else{
                GlobalCmd.cmdID = ART_BMI_WRITE_SOC_MEMORY_ID;
                GlobalCmd.devNum = devNum;
                GlobalCmd.CMD_U.ART_BMI_WRITE_SOC_MEMORY_CMD.baseaddress=address;
                GlobalCmd.CMD_U.ART_BMI_WRITE_SOC_MEMORY_CMD.blockLength=length;

                memcpy(GlobalCmd.CMD_U.ART_BMI_WRITE_SOC_MEMORY_CMD.byte,buffer,length);

                if(!artSendCmd(&GlobalCmd,length
                                        +sizeof(GlobalCmd.CMD_U.ART_BMI_WRITE_SOC_MEMORY_CMD.baseaddress)
                                        +sizeof(GlobalCmd.CMD_U.ART_BMI_WRITE_SOC_MEMORY_CMD.blockLength)
                                        +sizeof(GlobalCmd.cmdID), (void **)&pRet)) 
                {
					uiPrintf("Error: Unable to successfully send  GlobalCmd.cmdID->%d\n",GlobalCmd.cmdID);
					return FALSE;
                }
                //judgeDownloadTarget((A_UINT8)*pRet,length);
                break;
            }
	}	
	uiPrintf("OK!!\n");
	timeEnd=milliTime();
	uiPrintf("Time Taken=%d ms\n",timeEnd-timeSt);
    }
    return TRUE;
}

static A_BOOL getDeviceBin(unsigned char *pfile)
{
    FILE *fStream;
    int i;
    unsigned int length;
    unsigned char temp;
    uiPrintf("getDeviceBin:: open file name = %s\n",pfile);
    if( (fStream = fopen(pfile, "rb")) == NULL ) {
        uiPrintf("Failed to open file: %s \n", pfile);
        return FALSE;
    }
    i=0;
    while(fread(&temp,1,1,fStream)!=0)
    {
        device_bin[i]=temp;
        //uiPrintf("%x\t",device_bin[i]);
        i++;
    }
    device_bin_len = i;
    //uiPrintf("\ndevice_bin_len = %d\n",device_bin_len);

    fclose(fStream);
    return TRUE;
}



A_UINT32 judgeDownloadTarget(A_UINT8 Ret,unsigned int len)
{
		//		eepromValue = *pRegValue; // for ref
	switch(Ret){
		case 0x00:{
			//uiPrintf("Client downlowd image OK ... !!!...\n");
			uiPrintf(". ");
			break;
			}
		case 0x01:{
			uiPrintf("\nClient downlowd image failed ... when donwload length=%d, try again!!\n",len);
			return FALSE;
			}
		case 0x02:{
			uiPrintf("\nClient sdio driver open failed ... try again!!!...\n");
			return FALSE;
			}
		default: {
			uiPrintf("\nunknow error when Client download target image...errNo=0x%x\n",Ret);
			return FALSE;
			}
        }
	return TRUE;
}


A_UINT32 downloadTarget(A_UINT8 devNum,HANDLE handle)
{
        A_UINT32 *pRet;
        unsigned int  length;
        unsigned char *pDevice;
        unsigned int byte_index=0;
        unsigned int kk_cnt=0;       
        int i;
        A_UINT32 timeSt,timeEnd;
        int status;

	length = device_bin_len;
	pDevice = device_bin;

    if(sdio_client)
    {
//#ifndef _IQV
        status = BMIWriteMemory(handle,
             // 0x80002000,
                0x502400,
               device_bin,
               device_bin_len);
/*#else
		status=call_BMIWriteMemory(handle,
             // 0x80002000,
                0x502400,
               device_bin,
               device_bin_len);
#endif	// _IQV */
        if(status != A_OK)
        {
			uiPrintf("\nFailed BMIWriteMemory ad=0x%X\n",0x502000);
			return FALSE;
        }        
        return TRUE;
    }

    if(com_client)
    {
        uiPrintf("total device_bin length = %d bytes\n",length);
	uiPrintf("Start to download the device.bin to target, plz wait !\n");
	uiPrintf("downloading . ");
	timeSt=milliTime();
	while(length)
	{
	//uiPrintf("sending device_bin remain len = %d cnt = %d\n",length,kk_cnt++);
         if(length >  2048){
            GlobalCmd.cmdID = ART_IMAGE_DOWNLOAD_ID;
            GlobalCmd.devNum = devNum;
		GlobalCmd.CMD_U.ART_IMAGE_DOWNLOAD_CMD.ifEnd=0;
		GlobalCmd.CMD_U.ART_IMAGE_DOWNLOAD_CMD.block_length=2048;
		for(i=0;i<2048;i++){
			GlobalCmd.CMD_U.ART_IMAGE_DOWNLOAD_CMD.bytes[i]=*(pDevice++);
			//uiPrintf("%x\t",GlobalCmd.CMD_U.ART_IMAGE_DOWNLOAD_CMD.bytes[i]);
              }
		//uiPrintf("\n");
		if(!artSendCmd(&GlobalCmd,2048+4+sizeof(GlobalCmd.cmdID), (void **)&pRet)) 
		{
			uiPrintf("Error: Unable to successfully send  GlobalCmd.cmdID->%d\n",GlobalCmd.cmdID);
			return FALSE;
		}
		if (!judgeDownloadTarget((A_UINT8)*pRet,length))
			return FALSE;
		length -= 2048;
		continue;
		}
	else{
		GlobalCmd.cmdID = ART_IMAGE_DOWNLOAD_ID;
      		GlobalCmd.devNum = devNum;
		GlobalCmd.CMD_U.ART_IMAGE_DOWNLOAD_CMD.ifEnd=1;
		GlobalCmd.CMD_U.ART_IMAGE_DOWNLOAD_CMD.block_length=length;
		for(i=0;i<length;i++){
			GlobalCmd.CMD_U.ART_IMAGE_DOWNLOAD_CMD.bytes[i]=*(pDevice++);
			//uiPrintf("%x\t",GlobalCmd.CMD_U.ART_IMAGE_DOWNLOAD_CMD.bytes[i]);
  		}
		//uiPrintf("\n");
		if(!artSendCmd(&GlobalCmd,length+4+sizeof(GlobalCmd.cmdID), (void **)&pRet)) 
		{
			uiPrintf("Error: Unable to successfully send  GlobalCmd.cmdID->%d\n",GlobalCmd.cmdID);
			return FALSE;
		}
		if (!judgeDownloadTarget((A_UINT8)*pRet,length))
			return FALSE;
		break;
		}
	}	
	uiPrintf("OK!!\n");
	timeEnd=milliTime();
	uiPrintf("Time Taken=%d ms\n",timeEnd-timeSt);
    }
	return TRUE;
}




