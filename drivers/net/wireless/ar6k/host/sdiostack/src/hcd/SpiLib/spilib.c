// Copyright (c) 2005, 2006 Atheros Communications Inc.
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
@file: spilib.c

@abstract: SPI library functions
 
note: only single block read/writes currently supported.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#define MODULE_NAME SDSPILIB
#define DBG_DECLARE 3

#include "spilib.h"
#include "../../include/_sdio_defs.h"

static BOOL TestCRC16(PUCHAR pBuffer, UINT Length, UINT16 Crc);
static UINT8 ComputeCRC7(PUCHAR pBuffer, UINT Length);
static UINT16 ComputeCRC16(PUCHAR pBuffer, UINT Length);
static UINT ComputeErrorShift(UINT8 Response);
static UINT ComputeDataResponseErrorShift(PUINT8 pInBuffer, UINT Length);
static UINT ComputeDataReadStartResponseErrorShift(PUINT8 pInBuffer, UINT Length);

/* size of response by response type:  none, R1, R1B, R2, R3, MMCR4, MCR5, R6, SDIO_R4, SDIO_R5 */
static const UINT8 ResponseSize[] = {     0,  1,   1,  2,  5,     0,    0,  0,       5,       2};


/**++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  @function: prepares a command to be sent to the SPI device

  @category: HD_Reference
  @input: pRequest - the request to prepare
  @input: Length - length of pDataBuffer. See pDataBuffer for minimum size
  
  @output: pDataBuffer - data buffer to send out SPI port, length must be large enough for 6-byte command
                         plus response bytes plus SPILIB_LEADING_READ_BYTES
  @output: pRemainingBytes - number of bytes to read to get response
  
  @return: SDIO Status
  
  @notes:  This function creates the output to deliver across the SPI bus and sets the internal
           state of the library to expect the appropriate response buffer.
           pRequest must stay valid through the data reception phase.
  
  @see also: 
  
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
SDIO_STATUS SDSpi_PrepCommand(PSDSPI_CONFIG pConfig, PSDREQUEST pRequest, UINT Length, PUINT8 pDataBuffer, PUINT pRemainingBytes)
{
    UINT    responseType = GET_SDREQ_RESP_TYPE(pRequest->Flags);
    UINT ii;
    
    DBG_ASSERT_WITH_MSG(responseType != SDREQ_FLAGS_RESP_MMC_R4, "SDIO SPIlib - SDREQ_FLAGS_RESP_MMC_R4 not supported\n");
    DBG_ASSERT_WITH_MSG(responseType != SDREQ_FLAGS_RESP_MMC_R5, "SDIO SPIlib - SDREQ_FLAGS_RESP_MMC_R5 not supported\n");
    pConfig->State = CMD_RESPONSE_START;
    pConfig->DataSize = ResponseSize[responseType];
    if (Length < (pConfig->DataSize + SPILIB_COMMAND_BUFFER_SIZE + SPILIB_LEADING_READ_BYTES)) {
        return SDIO_STATUS_INVALID_PARAMETER;
    }
    pConfig->ExpectBusy  = (responseType == SDREQ_FLAGS_RESP_R1B);
    pConfig->TempCount = 0;
    if (pConfig->MaxReadTimeoutBytes == 0) {
        pConfig->MaxReadTimeoutBytes = SPILIB_READ_LEADING_FF;
    }
    pConfig->pRequest = pRequest;
    /* set the data into the 6-byte buffer 
     Bit position 47 46             [45:40]        [39:8] [7:1]     0
     Width (bits) 1   1                  6           32     7       1
     Value       ‘0’ ‘1’                 x            x     x      ‘1’
          start bit transmission bit command index argument CRC7  end bit
     */
    pDataBuffer[0] = 0x40 | (pRequest->Command & 0x3F);
    pDataBuffer[1] = (pRequest->Argument & 0xFF000000) >> 24;
    pDataBuffer[2] = (pRequest->Argument & 0x00FF0000) >> 16;
    pDataBuffer[3] = (pRequest->Argument & 0x0000FF00) >> 8;
    pDataBuffer[4] = (pRequest->Argument & 0x000000FF);
    /* create the crc for the command */
    pDataBuffer[5] = ComputeCRC7(pDataBuffer, 5) | 0x1;
    ii = 6;
    if (pRequest->Flags & SDREQ_FLAGS_DATA_TRANS) {
        /* this is data transfer */
        pConfig->BlockSize = pRequest->BlockLen;
        pConfig->BlockCount = pRequest->BlockCount;
        if (IS_SDREQ_WRITE_DATA(pRequest->Flags)) {
            /* set direction of transfer */
            pConfig->State = DATA_RESPONSE_WRITE_STATUS;
            pConfig->pRequest->pHcdContext = pConfig->pRequest->pDataBuffer;
            pRequest->DataRemaining = pConfig->BlockSize * pConfig->BlockCount;
        } else {
            pConfig->State = DATA_RESPONSE_READ_STATUS;
            pConfig->TempCount = 0;
            pConfig->pRequest->pHcdContext = pConfig->pRequest->pDataBuffer;
            pRequest->DataRemaining = pConfig->BlockSize * pConfig->BlockCount;
        }
    } else {
        pConfig->BlockSize = 0;
        pConfig->BlockCount = 0;
    }
    DBG_PRINT(SDDBG_TRACE, ("SDIO SDSpi_PrepCommand: BlockSize/Count: %d/%d cmd: 0x%X\n",
              pConfig->BlockSize, pConfig->BlockCount, pRequest->Command));
    if (pConfig->DataSize > 0) {
        *pRemainingBytes = pConfig->DataSize + SPILIB_LEADING_READ_BYTES;
        if ((pRequest->Flags & SDREQ_FLAGS_DATA_TRANS) && !IS_SDREQ_WRITE_DATA(pRequest->Flags)) {
            /* some card sneed an extra 8 clocks after the data read */
            *pRemainingBytes += 1;
        }
        
        /* make even value */
        *pRemainingBytes = (*pRemainingBytes + 1) & 0xFFFFFFFE;
        /* place FFs in remaining buffer to allow SPI to clock in response */
        memset(&pDataBuffer[ii], 0xFF, Length - pConfig->DataSize);
    } else {
        *pRemainingBytes = 0;
    }
    return SDIO_STATUS_SUCCESS;
}

/**++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  @function: processes data returned from the device

  @category: HD_Reference
  @input: pDataBuffer - the data from the SPI bus
  @input: DataLength - number of data bytes in pDataBuffer
  
  @output: pRemainingBytes - the number of bytes left in the response

  @return: SDIO Status - SDIO_STATUS_PENDING - more data needed to complete response
                         SDIO_STATUS_SUCCESS - data complete
  
  @notes:  This function processes the incoming response data, validating the CRC, and
           placing the results in the pResonse->Response buffer.
  
  @see also: 
  
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
SDIO_STATUS SDSpi_ProcessResponse(PSDSPI_CONFIG pConfig, PUCHAR pDataBuffer, UINT DataLength, PUINT pRemainingBytes)
{
    int ii;

    DBG_PRINT(SDDBG_TRACE, ("SDIO  SDSpi_ProcessResponse: state: %d, DataLength: %d\n", 
              pConfig->State, DataLength));

    switch(pConfig->State) {
      case CMD_RESPONSE_START: {
        /* waiting for the leading high bytes */
        /* we expect 1 to 8 high bytes */
        for (ii = 0; (ii < (SPILIB_LEADING_FF - pConfig->TempCount)) && (ii < DataLength); ii++) {
            if (pDataBuffer[ii] != 0xFF) {
                pConfig->State = CMD_RESPONSE;
                pConfig->TempCount = pConfig->DataSize;
                break;
            }
        }
        if (pConfig->State == CMD_RESPONSE_START) {
            if (ii == SPILIB_LEADING_FF) {
                pConfig->State = CMD_RESPONSE;
                pConfig->TempCount = pConfig->DataSize;
            } else {
                pConfig->TempCount += ii;
                if (pConfig->TempCount >= SPILIB_LEADING_FF) {
                    return SDIO_STATUS_BUS_RESP_TIMEOUT;
                }
            }
        }
        if (ii < DataLength) {
            /* we have more data to process, call recursively */
            pConfig->TempCount = ii;
            return SDSpi_ProcessResponse(pConfig, &pDataBuffer[ii], DataLength - ii, pRemainingBytes);
        } else {
            if (pConfig->State == CMD_RESPONSE) {
                *pRemainingBytes = pConfig->TempCount;
            } else {
                *pRemainingBytes = SPILIB_LEADING_FF - pConfig->TempCount + pConfig->DataSize;
            }
            pConfig->TempCount = ii;
            return SDIO_STATUS_PENDING;
        }
      }

      case CMD_RESPONSE: {
        /* waiting for the command response */
        UINT remaining = pConfig->DataSize;
        if (remaining == 0) {
            /* nothing more to process */
            *pRemainingBytes = 0;
            return SDIO_STATUS_SUCCESS;
        }
        /* looking for start of response */
        if (pDataBuffer[0] & 0x80) {
            if (pConfig->HandleBitShift) {
                /* see if we can shift the input to get it correct */
                pConfig->ShiftOffset = ComputeErrorShift(pDataBuffer[0]);
                if (pConfig->ShiftOffset != 0) {
                    DBG_PRINT(SDDBG_TRACE, ("SDIO  SDSpi_ProcessResponse: shift %d\n", 
                              (INT)pConfig->ShiftOffset));
                    pConfig->State  = CMD_RESPONSE_START;
                    pConfig->TempCount = 0;
                    return SDIO_STATUS_BUS_RESP_TIMEOUT_SHIFTABLE;
                } else {
                    /* error, timeout */
                    DBG_PRINT(SDDBG_WARN, ("SDIO  SDSpi_ProcessResponse: could not compute buffer bit shift, 0x%X\n", 
                              (UINT)pDataBuffer[0]));
                    pConfig->State  = NO_STATE;
                    *pRemainingBytes = 0;
                    return SDIO_STATUS_BUS_RESP_TIMEOUT;
                }
            } else {
                /* error, timeout */
                pConfig->State  = NO_STATE;
                *pRemainingBytes = 0;
                return SDIO_STATUS_BUS_RESP_TIMEOUT;
            }
        }
        if (remaining <= DataLength) {
            /* have enough data to finish request */
            if (pConfig->ReverseResponse) {
                for (ii = 0; ii < remaining; ii++) {
                    pConfig->pRequest->Response[remaining-ii-1] = pDataBuffer[ii];
                }
            } else {
                for (ii = 0; ii < remaining; ii++) {
                    pConfig->pRequest->Response[ii] = pDataBuffer[ii];
                }
            }
            if (GET_SDREQ_RESP_TYPE(pConfig->pRequest->Flags) == SDREQ_FLAGS_RESP_R1B) {
                /* need to check for busy tokens */
                //?? this code is fully implemented. Need to handle unneeded block coming from card
                pConfig->State = CMD_RESPONSE_BUSY;
                if (DataLength > ii) {
                    /* handle the rest of the data recursively */
                    return SDSpi_ProcessResponse(pConfig, &pDataBuffer[ii+1], DataLength-ii-1, pRemainingBytes);
                } else {
                    *pRemainingBytes = SPILIB_LEADING_READ_BYTES;
                    return SDIO_STATUS_PENDING;    
                }
            } else {
                *pRemainingBytes = 0;
                pConfig->State  = NO_STATE;
            }
            return SDIO_STATUS_SUCCESS;
        } else {//??fix partial result save
            /* error, timeout */
            pConfig->State  = NO_STATE;
            *pRemainingBytes = 0;
            return SDIO_STATUS_BUS_RESP_TIMEOUT;
        }
      }     
      
      case CMD_RESPONSE_BUSY: {
        /* looking for possible busy tokens */
        for(ii = 0; ii < DataLength; ii++) {
            if (pDataBuffer[ii] == 0xFF) {
                pConfig->TempCount++;
                if (pConfig->TempCount >= SPILIB_LEADING_READ_BYTES) {
                    /* no busy, done */
                    *pRemainingBytes = 0;
                    pConfig->State  = NO_STATE;
                    return SDIO_STATUS_SUCCESS;
                }
            } else if (pDataBuffer[ii] == 0x00) {
                /* busy */
                pConfig->State = CMD_RESPONSE_BUSY_BUSY;
                if (DataLength > ii) {
                    /* handle the rest of the data recursively */
                    return SDSpi_ProcessResponse(pConfig, &pDataBuffer[ii+1], DataLength-ii-1, pRemainingBytes);
                } else {
                    *pRemainingBytes = SPILIB_LEADING_READ_BYTES;
                    return SDIO_STATUS_PENDING;
                }
                
            } else {
                /* error condition */
                *pRemainingBytes = 0;
                pConfig->State  = NO_STATE;
                return SDIO_STATUS_DEVICE_ERROR;
            }
        }
        if (ii == DataLength) {
            /* need to check for more busies */
            *pRemainingBytes = SPILIB_LEADING_READ_BYTES;
            return SDIO_STATUS_PENDING;    
        }
      }
      
      case CMD_RESPONSE_BUSY_BUSY: {
        /* in busy state */
        for(ii = 0; ii < DataLength; ii++) {
            if (pDataBuffer[ii] != 0x00) {
                *pRemainingBytes = 0;
                pConfig->State  = NO_STATE;
                return SDIO_STATUS_SUCCESS;
            }
        }
        /* need to check for more busies */
        *pRemainingBytes = SPILIB_LEADING_READ_BYTES;
        return SDIO_STATUS_PENDING;    
      }
            
      default:
      case NO_STATE:
            DBG_PRINT(SDDBG_ERROR, ("SDIO SPIlib SDSpi_ProcessResponse - invalid state:%d\n", pConfig->State));
            return SDIO_STATUS_INVALID_PARAMETER;
    }
}

/**++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  @function: processes data blocks to be sent to the device

  @category: HD_Reference
  @input:  DataLength - size of pDataBuffer
  
  @output: pDataBuffer - the data for the SPI bus
  @output: pLength - the number of bytes in pDataBuffer
  @output: pRemainingBytes - the number of bytes to read back from device

  @return: SDIO Status - SDIO_STATUS_PENDING - more data needed to complete response
                         SDIO_STATUS_SUCCESS - data complete
  
  @notes:  This function creates the output buffer for a data block writes. 
           The DataLength must be at least the size of 1 block plus 4 bytes.
  
  @see also: 
  
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
SDIO_STATUS SDSpi_ProcessDataBlock(PSDSPI_CONFIG pConfig, PUCHAR pDataBuffer, UINT DataLength, 
                                   PUINT pLength, PUINT pRemainingBytes)
{
    int ii;
    DBG_PRINT(SDDBG_TRACE, ("SDIO SPIlib SDSpi_ProcessDataBlock - State: %d, DataRemaining: %d, BlockLen: %d, pDataBuffer: 0x%x, Length: %d\n",
              pConfig->State, pConfig->pRequest->DataRemaining, (UINT)pConfig->pRequest->BlockLen, (UINT)pDataBuffer, DataLength));

    switch(pConfig->State) {
      /* write data processing, leading FF */
      case DATA_RESPONSE_WRITE_STATUS: {
        /* waiting for the leading high bytes */
        /* we expect 1 to 8 high bytes */
        for (ii = 0; (ii < (SPILIB_LEADING_FF - pConfig->TempCount)) && (ii < DataLength); ii++) {
            if (pDataBuffer[ii] != 0xFF) {
                pConfig->State = DATA_RESPONSE_WRITE_RESPONSE;
                pConfig->TempCount = pConfig->DataSize;
                break;
            }
        }
        if (pConfig->State == DATA_RESPONSE_WRITE_STATUS) {
            if (ii == SPILIB_LEADING_FF) {
                pConfig->State = DATA_RESPONSE_WRITE_RESPONSE;
                pConfig->TempCount = pConfig->DataSize;
            } else {
                pConfig->TempCount += ii;
                if (pConfig->TempCount >= SPILIB_LEADING_FF) {
                    return SDIO_STATUS_BUS_RESP_TIMEOUT;
                }
            }
        }
        if (ii < DataLength) {
            /* we have more data to process, call recursively */
            pConfig->TempCount = ii;
            return SDSpi_ProcessDataBlock(pConfig, &pDataBuffer[ii], DataLength - ii, pLength, pRemainingBytes);
        } else {
            if (pConfig->State == DATA_RESPONSE_WRITE_RESPONSE) {
                *pRemainingBytes = pConfig->TempCount;
                pConfig->TempCount = 0;
            } else {
                *pRemainingBytes = SPILIB_LEADING_FF - pConfig->TempCount + pConfig->DataSize;
                pConfig->TempCount = ii;
            }
            /* set output bytes to FF */
            memset(pDataBuffer, 0xFF, *pRemainingBytes);
            return SDIO_STATUS_PENDING;
        }

      }
      
      case DATA_RESPONSE_WRITE_RESPONSE: {
        /* waiting for the command response */
        UINT remaining = pConfig->DataSize;
        ii = 0;
            /* looking for start of response */
        if (pDataBuffer[0] & 0x80) {
            if (pConfig->HandleBitShift) {
                /* see if we can shift the input to get it correct */
                pConfig->ShiftOffset = ComputeErrorShift(pDataBuffer[0]);
                if (pConfig->ShiftOffset != 0) {
                    DBG_PRINT(SDDBG_TRACE, ("SDIO  SDSpi_ProcessDataBlock: shift %d\n", 
                              (INT)pConfig->ShiftOffset));
                    pConfig->State  = DATA_RESPONSE_WRITE_STATUS;
                    pConfig->TempCount = 0;
                    return SDIO_STATUS_BUS_RESP_TIMEOUT_SHIFTABLE;
                } else {
                    DBG_PRINT(SDDBG_TRACE, ("SDIO  SDSpi_ProcessDataBlock: could not compute buffer bit shift, 0x%X\n", 
                              (UINT)pDataBuffer[0]));
                    /* error, timeout */
                    pConfig->State  = NO_STATE;
                    *pRemainingBytes = 0;
                    return SDIO_STATUS_BUS_RESP_TIMEOUT;
                }
            }
        }
        if (remaining <= DataLength) {
            /* have enough data to finish request */
            if (pConfig->ReverseResponse) {
                for (ii = 0; ii < remaining; ii++) {
                    pConfig->pRequest->Response[remaining-ii-1] = pDataBuffer[ii];
                }
            } else {
                for (ii = 0; ii < remaining; ii++) {
                    pConfig->pRequest->Response[ii] = pDataBuffer[ii];
                }
            }
        }
        if (pConfig->pRequest->Response[ii] & SPI_CS_CMD_CRC_ERR) {
            DBG_PRINT(SDDBG_TRACE, ("SDIO  SDSpi_ProcessDataBlock: write R1 response error, 0x%X\n", 
                              (UINT)pConfig->pRequest->Response[ii]));
            pConfig->State  = NO_STATE;
            *pRemainingBytes = 0;
            return SDIO_STATUS_BUS_RESP_CRC_ERR ;
        }
        if (pConfig->pRequest->Response[ii] != 0) {
            DBG_PRINT(SDDBG_TRACE, ("SDIO  SDSpi_ProcessDataBlock: write R1 response error, 0x%X\n", 
                              (UINT)pConfig->pRequest->Response[ii]));
            pConfig->State  = NO_STATE;
            *pRemainingBytes = 0;
            return SDIO_STATUS_DEVICE_ERROR;
        }
        
        /* this is a data write transfer, if the response is OK, then the data blocks
           will be transfered. */
        pConfig->State = DATA_RESPONSE_WRITE;
        /* Size is a block plus the Data Token and CRC16 */
        *pRemainingBytes = 0; 
        /* setup the data buffers for reading into */
        pConfig->pRequest->DataRemaining = pConfig->BlockSize * pConfig->BlockCount;
        pConfig->pRequest->pHcdContext = pConfig->pRequest->pDataBuffer;
        pConfig->DataSize = pConfig->BlockSize * pConfig->BlockCount;
        /* setup the write buffers, passing total buffer length */
        return SDSpi_ProcessDataBlock(pConfig, pDataBuffer, *pLength, pLength, pRemainingBytes);
      }
          
      /* write data processing */
      case DATA_RESPONSE_WRITE: {
        PUINT8 pBuf = pConfig->pRequest->pHcdContext;
        INT dataCopy = min(pConfig->pRequest->DataRemaining, (UINT)pConfig->pRequest->BlockLen);
        INT size = dataCopy;  
        UINT16 crc; 
        /* setup the output buffer */
        if (DataLength < (pConfig->BlockSize+3+SPILIB_LEADING_READ_BYTES)) {
            DBG_PRINT(SDDBG_WARN, ("SDIO SPIlib SDSpi_ProcessDataBlock - buffer too short:%d\n", DataLength));
            return SDIO_STATUS_INVALID_PARAMETER;
        }
        /* is this a single or multi- block transfer,
           the minus TempCount handles the recursive call case, otherwise it is zero */
        if (pConfig->BlockCount == 1) {
            pDataBuffer[0-pConfig->TempCount] = SPI_WRITE_START_TOKEN;
        } else {
            pDataBuffer[0-pConfig->TempCount] = SPI_WRITE_START_TOKEN_MULTIBLOCK;
        }
        /* copy the data to the output buffer */
        for(ii = 1; 
            (ii <= DataLength) && (dataCopy > 0);
            ii++, dataCopy--) {
            pDataBuffer[ii-pConfig->TempCount] = *pBuf++;
        }
        /* set FF to whats left in buffer */
        if (DataLength > ii) {
            memset(&pDataBuffer[ii-pConfig->TempCount], 0xFF, *pLength - ii - pConfig->TempCount);
        }
        pConfig->pRequest->DataRemaining -= ii-1;
        pConfig->pRequest->pHcdContext = pBuf;
        /* add in the crc */
        crc = ComputeCRC16(&pDataBuffer[1-pConfig->TempCount], size);
        pDataBuffer[size+1-pConfig->TempCount] = (crc & 0xFF00) >> 8; 
        pDataBuffer[size+2-pConfig->TempCount] = (crc & 0xFF); 
        *pLength = size+4;
        
        /* set one response byte, status */
        *pRemainingBytes = SPILIB_LEADING_READ_BYTES;
        pConfig->State = DATA_RESPONSE_WRITE_DATA_STATUS;
        return SDIO_STATUS_PENDING;
      }
      
      case DATA_RESPONSE_WRITE_DATA_STATUS: 
      {
        INT dataCopy = min(pConfig->pRequest->DataRemaining, (UINT)pConfig->pRequest->BlockLen);
        
        if (DataLength < (pConfig->BlockSize+4+SPILIB_LEADING_READ_BYTES)) {
            DBG_PRINT(SDDBG_WARN, ("SDIO SPIlib SDSpi_ProcessDataBlock - buffer too short:%d\n", DataLength));
            return SDIO_STATUS_INVALID_PARAMETER;
        }

        /* shift the buffer if needed */
        if (pConfig->HandleBitShift) {
            pConfig->ShiftOffset = ComputeDataResponseErrorShift(&pDataBuffer[pConfig->BlockSize], 4+SPILIB_LEADING_READ_BYTES);
            if (pConfig->ShiftOffset != 0) {
                DBG_PRINT(SDDBG_TRACE, ("SDIO  SDSpi_ProcessDataBlock: shift %d\n", 
                        (INT)pConfig->ShiftOffset));
                pConfig->State  = DATA_RESPONSE_WRITE_DATA_STATUS;
                pConfig->TempCount = 0;
                return SDIO_STATUS_BUS_RESP_TIMEOUT_SHIFTABLE;
            }
        }
        
        /* find the response byte, looking after the block sent */
        for (ii = pConfig->BlockSize; ii < (pConfig->BlockSize + 4+SPILIB_LEADING_READ_BYTES); ii++) {
            if ((pDataBuffer[ii] & SPI_DATA_RESPONSE_STATUS_MASK) != SPI_DATA_RESPONSE_STATUS_MASK) {
                /* have the response token */
                if ((pDataBuffer[ii] & SPI_DATA_RESPONSE_STATUS_MASK) == SPI_DATA_RESPONSE_STATUS_CRCERROR) {
                    pConfig->State = NO_STATE;
                    return SDIO_STATUS_BUS_RESP_CRC_ERR;
                }
                if ((pDataBuffer[ii] & SPI_DATA_RESPONSE_STATUS_MASK) == SPI_DATA_RESPONSE_STATUS_WRITEERROR) {
                    pConfig->State = NO_STATE;
                    return SDIO_STATUS_BUS_WRITE_ERROR;
                }
                if ((pDataBuffer[ii] & SPI_DATA_RESPONSE_STATUS_MASK) == SPI_DATA_RESPONSE_STATUS_ACCEPTED) {
                    break;
                } else {
                    /* can't be here */
                    DBG_PRINT(SDDBG_ERROR, ("SDIO SPI Lib SDSpi_ProcessDataBlock - invalid data response token: 0x%X\n",
                              pDataBuffer[ii]));
                    return SDIO_STATUS_DATA_ERROR_UNKNOWN;            
                }
            }
        }
        
        if (dataCopy == 0) {
            /* we are done with this transfer. If it is multiple block, send the stop */
            if (pConfig->BlockCount > 1) {
                pDataBuffer[0] = SPI_WRITE_STOP_TOKEN_MULTIBLOCK;
                pDataBuffer[1] = 0xFF;
                *pLength = 2;
                pConfig->State = DATA_RESPONSE_WRITE_BUSY;
                *pRemainingBytes = SPILIB_LEADING_FF;
                return SDIO_STATUS_PENDING;
            }
        } 
        pDataBuffer[0] = 0xFF;
        pDataBuffer[1] = 0xFF;
        *pLength = 2;
        pConfig->State = DATA_RESPONSE_WRITE_BUSY;
        *pRemainingBytes = SPILIB_LEADING_FF;
        return SDIO_STATUS_PENDING;
      }
      
      case DATA_RESPONSE_WRITE_BUSY:
      { /* looking for busy tokens */
        INT dataCopy = min(pConfig->pRequest->DataRemaining, (UINT)pConfig->pRequest->BlockLen);
          
        for(ii = 0; 
            (ii < DataLength);
            ii++) {
            if (pDataBuffer[ii] == 0xFF) {
                if (dataCopy == 0) {
                    /* write complete */
                    pConfig->State = DATA_STOPPED;
                    *pRemainingBytes = 0;
                    return SDIO_STATUS_SUCCESS;
                } else {
                    /* multi-block write */
                    /* this is a data write transfer, if the response is OK, then the data blocks
                       will be transfered. */
                    pConfig->State = DATA_RESPONSE_WRITE;
                    /* Size is a block plus the Data Token and CRC16 */
                    *pRemainingBytes = 0; 
                    pConfig->TempCount = 0;
                    return SDSpi_ProcessDataBlock(pConfig, pDataBuffer, *pLength, pLength, pRemainingBytes);
                }
            }
        }
        *pRemainingBytes = min(SPILIB_WRITE_BUSY_FF, *pLength);
        *pLength = 0;
        memset(pDataBuffer, 0xFF, *pRemainingBytes);
        return SDIO_STATUS_PENDING;
      }
      
      case DATA_STOPPED:
      {
        *pLength = 0;
        return SDIO_STATUS_SUCCESS;
      }
      
      /* read data processing */
      case DATA_RESPONSE_READ_STATUS: {
        /* waiting for the leading high bytes */
        /* we expect 1 to 8 high bytes */
        for (ii = 0; (ii < (SPILIB_LEADING_FF - pConfig->TempCount)) && (ii < DataLength); ii++) {
            if (pDataBuffer[ii] != 0xFF) {
                pConfig->State = DATA_RESPONSE_READ_RESPONSE;
                pConfig->TempCount = pConfig->DataSize;
                break;
            }
        }
        if (pConfig->State == DATA_RESPONSE_READ_STATUS) {
            if (ii == SPILIB_LEADING_FF) {
                pConfig->State = DATA_RESPONSE_READ_RESPONSE;
                pConfig->TempCount = pConfig->DataSize;
            } else {
                pConfig->TempCount += ii;
                if (pConfig->TempCount >= SPILIB_LEADING_FF) {
                    return SDIO_STATUS_BUS_RESP_TIMEOUT;
                }
            }
        }
        if (ii < DataLength) {
            /* we have more data to process, call recursively */
            pConfig->TempCount = ii;
            return SDSpi_ProcessDataBlock(pConfig, &pDataBuffer[ii], DataLength - ii, pLength, pRemainingBytes);
        } else {
            if (pConfig->State == DATA_RESPONSE_READ_RESPONSE) {
                *pRemainingBytes = pConfig->TempCount;
            } else {
                *pRemainingBytes = SPILIB_LEADING_FF - pConfig->TempCount + pConfig->DataSize;
            }
            pConfig->TempCount = ii;
            return SDIO_STATUS_PENDING;
        }

      }
      
      case DATA_RESPONSE_READ_RESPONSE: {
        /* waiting for the command response */
        UINT remaining = pConfig->DataSize;
        ii = 0;
            /* looking for start of response */
        if (pDataBuffer[0] & 0x80) {
            if (pConfig->HandleBitShift) {
                /* see if we can shift the input to get it correct */
                pConfig->ShiftOffset = ComputeErrorShift(pDataBuffer[0]);
                if (pConfig->ShiftOffset != 0) {
                    DBG_PRINT(SDDBG_TRACE, ("SDIO  SDSpi_ProcessDataBlock: shift %d\n", 
                              (INT)pConfig->ShiftOffset));
                    pConfig->State  = DATA_RESPONSE_READ_STATUS;
                    pConfig->TempCount = 0;
                    return SDIO_STATUS_BUS_RESP_TIMEOUT_SHIFTABLE;
                } else {
                    DBG_PRINT(SDDBG_TRACE, ("SDIO  SDSpi_ProcessDataBlock: could not compute buffer bit shift, 0x%X\n", 
                              (UINT)pDataBuffer[0]));
                    /* error, timeout */
                    pConfig->State  = NO_STATE;
                    *pRemainingBytes = 0;
                    return SDIO_STATUS_BUS_RESP_TIMEOUT;
                }
            }
        }
        
        if (remaining <= DataLength) {
            /* have enough data to finish request */
            if (pConfig->ReverseResponse) {
                for (ii = 0; ii < remaining; ii++) {
                    pConfig->pRequest->Response[remaining-ii-1] = pDataBuffer[ii];
                }
            } else {
                for (ii = 0; ii < remaining; ii++) {
                    pConfig->pRequest->Response[ii] = pDataBuffer[ii];
                }
            }
        }
        /* check the response, assume R1 */
        if ((pConfig->pRequest->Response[0] & 0xFC) != 0) {
            /* card reported error */
            DBG_PRINT(SDDBG_TRACE, ("SDIO  SDSpi_ProcessDataBlock: transfer error repsone[0]:0x%X\n", 
                         (UINT)pConfig->pRequest->Response[0]));
            pConfig->State  = NO_STATE;
            *pRemainingBytes = 0;
            return SDIO_STATUS_DEVICE_ERROR;
        }
        /* this is a data read transfer, if the response is OK, then the data blocks
           will be transfered. */
        pConfig->State = DATA_RESPONSE_READ_START;
        /* Size is a block plus the Data Token and CRC16, plus estimated leading 0xFF bytes*/
        *pRemainingBytes = pConfig->BlockSize + 4 + SPILIB_LEADING_READ_BYTES; 
        if (GET_SDREQ_RESP_TYPE(pConfig->pRequest->Flags) == SDREQ_FLAGS_RESP_R1B) {
            /* get an extra byte response to check for busy */
            (*pRemainingBytes)++;
        }
        pConfig->TempCount = 0;
        /* setup the data buffers for reading into */
        pConfig->pRequest->DataRemaining = pConfig->BlockSize * pConfig->BlockCount;
        pConfig->pRequest->pHcdContext = pConfig->pRequest->pDataBuffer;
        pConfig->DataSize = pConfig->BlockSize * pConfig->BlockCount;
        DBG_PRINT(SDDBG_TRACE, ("SDIO  SDSpi_ProcessDataBlock (DATA_RESPONSE_READ_RESPONSE): data[0]:0x%X, data[1]:0x%X, ii:%d, DataLenght: %d\n", 
                         (UINT)pDataBuffer[0], (UINT)pDataBuffer[1], ii, DataLength));
        if (DataLength > ii) {
            /* handle the rest of the data recursively */
            return SDSpi_ProcessDataBlock(pConfig, &pDataBuffer[ii], DataLength-ii, pLength, pRemainingBytes);
        } else {
            /* we need to call the data block processing to handle possible data starts */
            return SDIO_STATUS_PENDING;
        }
    }
      
      case DATA_RESPONSE_READ_START: {
        /* reading single or multiple blocks */
        /* we are looking for the start of data */
        for(ii = 0; ii < DataLength; ii++, pConfig->TempCount++) {
            if (pDataBuffer[ii] != 0xFF) {
                break;
            }
        }
        
        if (ii == DataLength) {
            /* we ran off the end */
            if (pConfig->TempCount > pConfig->MaxReadTimeoutBytes) {                
                /* no start of data, timeout */
                DBG_PRINT(SDDBG_TRACE, ("SDIO  SDSpi_ProcessDataBlock: DATA_RESPONSE_READ_START TO %d, 0x%X\n", 
                              pConfig->TempCount, (INT)pDataBuffer[ii-1]));
                return SDIO_STATUS_BUS_RESP_TIMEOUT;
            } else {
                *pRemainingBytes = pConfig->BlockSize + 4 + SPILIB_LEADING_READ_BYTES; 
                return SDIO_STATUS_PENDING;
            }
        }
        if (pDataBuffer[ii] == SPI_READ_START_TOKEN) {
            
            /* data is starting */
            pConfig->TempCount = 0;
            pConfig->State = DATA_RESPONSE_READ;
            if (DataLength > ii) {
                /* handle the rest of the data recursively */
                return SDSpi_ProcessDataBlock(pConfig, &pDataBuffer[ii+1], DataLength-ii-1, pLength, pRemainingBytes);
            } else {
                *pRemainingBytes = pConfig->BlockSize + 2;
                return SDIO_STATUS_PENDING;
            }
        } else
            if ((pDataBuffer[ii] & SPI_DATAERR_OUTOFRANGE) && ((pDataBuffer[ii] & 0xF0) == 0)) {
                pConfig->State  = NO_STATE;
                *pRemainingBytes = 0;
                return SDIO_STATUS_CIS_OUT_OF_RANGE;
            }
        else    
                if ((pDataBuffer[ii] & SPI_DATAERR_ECCERROR)  && ((pDataBuffer[ii] & 0xF0) == 0)){ 
                    pConfig->State  = NO_STATE;
                    *pRemainingBytes = 0;
                    return SDIO_STATUS_DATA_ERROR_UNKNOWN;
                }
        else        
                    if ((pDataBuffer[ii] & SPI_DATAERR_CCERROR)  && ((pDataBuffer[ii] & 0xF0) == 0)) {
                        pConfig->State  = NO_STATE;
                        *pRemainingBytes = 0;
                        return SDIO_STATUS_DEVICE_ERROR;
                    }
        else                    
                   if ((pDataBuffer[ii] & SPI_DATAERR_ERROR)  && ((pDataBuffer[ii] & 0xF0) == 0)) {
                       pConfig->State  = NO_STATE;
                       *pRemainingBytes = 0;
                       return SDIO_STATUS_DATA_ERROR_UNKNOWN;
                   }
        else
            if (pConfig->HandleBitShift) {
                /* see if we can shift the input to get it correct */
                pConfig->ShiftOffset = ComputeDataReadStartResponseErrorShift(&pDataBuffer[ii], 2);
                if (pConfig->ShiftOffset != 0) {
                    DBG_PRINT(SDDBG_TRACE, ("SDIO  SDSpi_ProcessDataBlock: shift %d\n", 
                              (INT)pConfig->ShiftOffset));
                    pConfig->State  = DATA_RESPONSE_READ_START;
                    pConfig->TempCount = 0;
                    return SDIO_STATUS_BUS_RESP_TIMEOUT_SHIFTABLE;
                } else {
                    DBG_PRINT(SDDBG_TRACE, ("SDIO  SDSpi_ProcessDataBlock: could not compute buffer bit shift, 0x%X\n", 
                              (UINT)pDataBuffer[0]));
                    /* error, timeout */
                    pConfig->State  = NO_STATE;
                    *pRemainingBytes = 0;
                    return SDIO_STATUS_BUS_RESP_TIMEOUT;
                }
        }
        return SDIO_STATUS_DATA_ERROR_UNKNOWN;         
      }

      case DATA_RESPONSE_READ: {
        /* reading single or multiple blocks */
        PUINT8 pBuf = &((PUINT8)pConfig->pRequest->pHcdContext)[pConfig->DataSize - pConfig->pRequest->DataRemaining];
        INT dataCopy = min(pConfig->pRequest->DataRemaining, (UINT)pConfig->pRequest->BlockLen-pConfig->TempCount);   
        dataCopy = min(dataCopy , (INT)DataLength);
        for(ii=0; 
            (ii < DataLength) && (dataCopy > 0);
            ii++, dataCopy--, pConfig->TempCount++) {
            *pBuf++ = pDataBuffer[ii];
        }
        pConfig->pRequest->DataRemaining -= ii;
        if (pConfig->TempCount == pConfig->BlockSize) {
            /* we are now looking for the CRC bytes */
            pConfig->TempCount = 0;
            pConfig->State = DATA_RESPONSE_READ_CRC;
            if (DataLength > ii) {
                /* more data to process, call recursively */
                return SDSpi_ProcessDataBlock(pConfig, &pDataBuffer[ii], DataLength-ii, pLength, pRemainingBytes);
            } else {
                *pRemainingBytes = 2;
                return SDIO_STATUS_PENDING;
            }
        } else {
            *pRemainingBytes = pConfig->BlockSize - pConfig->TempCount + 2;
            return SDIO_STATUS_PENDING;
        }
      }       
      
      case DATA_RESPONSE_READ_CRC: {

        /* looking for the CRC bytes on read transaction */
        for(ii = 0; (ii < DataLength) && (pConfig->TempCount < 2); ii++, pConfig->TempCount++) {
            pConfig->TempData[pConfig->TempCount] = pDataBuffer[ii];
        }
        if (pConfig->TempCount == 2) {
            /* get the CRC, now test it  */
            UINT pos = (pConfig->BlockSize * pConfig->BlockCount) - pConfig->pRequest->DataRemaining
                       - pConfig->BlockSize;
            if (TestCRC16(&(((PUINT8)pConfig->pRequest->pHcdContext)[pos]), 
                          pConfig->BlockSize,
                          (UINT16)(pConfig->TempData[0] << 8 | (pConfig->TempData[1])))) {
                /* block is OK, see if we need more blocks */
                if (pConfig->pRequest->DataRemaining > 0) {
                    /* more data to read */
                    pConfig->State = DATA_RESPONSE_READ_START;
                    /* Size is a block plus the Data Token and CRC16, plus estimated leading 0xFF bytes*/
                    *pRemainingBytes = pConfig->BlockSize + 4 + SPILIB_LEADING_READ_BYTES; 
                    pConfig->TempCount = 0;
                    if (DataLength > ii) {
                        /* handle additional data recursively */   
                        pConfig->TempCount = 0;
                        return SDSpi_ProcessDataBlock(pConfig, &pDataBuffer[ii], DataLength-ii, pLength, pRemainingBytes);
                    } else {
                        return SDIO_STATUS_PENDING;
                    }
                } else {
                    *pRemainingBytes = 1;
                    pConfig->State = DATA_RESPONSE_READ_TRAIL;
                    return SDIO_STATUS_PENDING;
                }
            } else {
                return SDIO_STATUS_BUS_RESP_CRC_ERR;
            }
        } else {
            *pRemainingBytes = 2 - pConfig->TempCount;
            return SDIO_STATUS_PENDING;
        }
      }
      
      case DATA_RESPONSE_READ_TRAIL:
          /* force an extra 8-clocks after a read, some cards seem to need it */
          *pRemainingBytes = 0;
          pConfig->State = NO_STATE;
          return SDIO_STATUS_SUCCESS;
          
      default:
      case NO_STATE:
            DBG_PRINT(SDDBG_ERROR, ("SDIO SPIlib SDSpi_ProcessDataBlock - invalid state:%d\n", pConfig->State));
            return SDIO_STATUS_INVALID_PARAMETER;
    }
}

/* ComputeErrorShift - determine the number of bits the data requires shifting.
 *                     looks for a leading bit zero.
 */
static UINT ComputeErrorShift(UINT8 Response) 
{
    INT ii;
    /* look for first zero bit */
    for (ii = 7; ii >= 0; ii--) {
        if (!(Response & (1 << ii))) {
            DBG_PRINT(SDDBG_TRACE, ("SDIO SPIlib ComputeErrorShift - shift:%d 0x%X\n", (7-ii), (UINT)Response));
            return 7 - ii;
        }
    }
    /* this is an error case */
    DBG_PRINT(SDDBG_TRACE, ("SDIO SPIlib ComputeErrorShift - no zero bit:0x%X\n", (UINT)Response));
    return 0;
}

/* ComputeDataResponseErrorShift - determine the number of bits the data response byte requires shifting.
 *                     looking for a signature of bits xxx0xxx1
 */
static UINT ComputeDataResponseErrorShift(PUINT8 pInBuffer, UINT Length) 
{
    INT ii;
    INT jj;
    UINT8 data0;
    UINT8 data1;

    if (Length < 2) {
        return 0;
    }
    for (ii = 0; ii < Length-1; ii++) {
        data0 = pInBuffer[ii];
        data1 = pInBuffer[ii+1];
        for (jj = 0; jj < 8; jj++) {
            /* look for zero in bit 4 */
            if (jj < 5) {
                if ((data0 << jj) & 0x10) {
                    /* no zero */
                    continue;
                }
            } else {
                if ((data1 >> 8-jj) & 0x10) {
                    /* no zero */
                    continue;
                }
            }
            /* now look for the 1 bit */
            if (jj == 0) {
                if (data0 & 0x01) {
                    /* found */
                    return jj;
                }
            }
            if (data1 & (1 << (8-jj))) {
                return jj;
            }
        }
    }
    /* this is an error case */
    DBG_PRINT(SDDBG_TRACE, ("SDIO SPIlib ComputeDataResponseErrorShift - no zero bit:0x%X\n", (UINT)pInBuffer[0]));
    return 0;
}

/* ComputeDataReadStartResponseErrorShift - determine the number of bits the data response byte requires shifting.
 *                     looking for a signature of bits oxFE or 0x0n
 */
static UINT ComputeDataReadStartResponseErrorShift(PUINT8 pInBuffer, UINT Length) 
{
    INT jj;
    UINT8 data0;
    UINT8 data1;

    if (Length < 2) {
        return 0;
    }
    data0 = pInBuffer[0];
    data1 = pInBuffer[1];
    /* look right shift */
    for (jj = 0; jj < 8; jj++) {
        /* look for 0xFE */
        if (data0 == 0xFE) {
            return -jj;
        }
        /* look for leading zeros */
        if ((data0 & 0xF0) == 0) {
            return -jj;
        }
        data0 = (data0 >> 1) | 0x80;
    }
    /* look left shift */
    for (jj = 0; jj < 8; jj++) {
        /* look for 0xFE */
        if (data0 == 0xFE) {
            return jj;
        }
        /* look for leading zeros */
        if ((data0 & 0xF0) == 0) {
            return jj;
        }
        data0 = (data0 << 1) | ((data1 & 0x80) >> 7);
        data1 = data1 << 1;
    }
    /* this is an error case */
    DBG_PRINT(SDDBG_TRACE, ("SDIO SPIlib ComputeDataReadStartResponseErrorShift - no zero bit:0x%X\n", (UINT)pInBuffer[0]));
    return 0;
}

/* buffer must be at least 1 byte longer than the length, looking for a leading zero bit */
void SDSpi_CorrectErrorShift(PSDSPI_CONFIG pConfig, PUINT8 pInBuffer, UINT Length, PUINT8 pOutputBuffer, UINT8 StartByte) 
{
    INT   ii;
    UINT8 temp;
    UINT8 temp2;
    UINT8 temp3;
    UINT8 mask;
    const UINT8 maskTable[] = {
        0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFF
    };
    if (pConfig->ShiftOffset >= 0) {
        mask = maskTable[pConfig->ShiftOffset];
    } else {
        mask = maskTable[-pConfig->ShiftOffset];
    }
    for (ii = 0; ii < Length; ii++) {
        if (ii == 0) {
            temp = StartByte;
        } else {
           temp = temp3;
        }
        if (pConfig->ShiftOffset >= 0) {
            temp3 = pInBuffer[ii];
            temp2 = temp << pConfig->ShiftOffset;
            pOutputBuffer[ii] = temp2 | (pInBuffer[ii] & mask) >> (8 - pConfig->ShiftOffset);
        } else {
            /* shift right */
            temp3 = pOutputBuffer[ii];
            temp2 = pOutputBuffer[ii] >> -pConfig->ShiftOffset;
            pOutputBuffer[ii] = temp2 | (temp & mask);
        }
    }
    return;
}


/* local functions */
#define CRCMASK    0x1021

static unsigned int crc16table[256] =
{
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12, 
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A, 
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41, 
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49, 
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78, 
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067, 
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};    
static UINT16 ComputeCRC16(PUCHAR pBuffer, UINT Length) 
{
    UINT ii;
    UINT index;
    UINT16 crc16 = 0x0;
    
    for(ii = 0; ii < Length; ii++) {
        index = ((crc16 >> 8) ^ pBuffer[ii]) & 0xff;
        crc16 = ((crc16 << 8) ^ crc16table[index]) & 0xffff;
    }
    return crc16;
}


#define CRC7_POLYNOMIAL 0x89

static UINT8 crc7Table[] = {
0x00, 0x09, 0x12, 0x1B, 0x24, 0x2D, 0x36, 0x3F,
0x48, 0x41, 0x5A, 0x53, 0x6C, 0x65, 0x7E, 0x77,
0x19, 0x10, 0x0B, 0x02, 0x3D, 0x34, 0x2F, 0x26,
0x51, 0x58, 0x43, 0x4A, 0x75, 0x7C, 0x67, 0x6E,
0x32, 0x3B, 0x20, 0x29, 0x16, 0x1F, 0x04, 0x0D,
0x7A, 0x73, 0x68, 0x61, 0x5E, 0x57, 0x4C, 0x45,
0x2B, 0x22, 0x39, 0x30, 0x0F, 0x06, 0x1D, 0x14,
0x63, 0x6A, 0x71, 0x78, 0x47, 0x4E, 0x55, 0x5C,
0x64, 0x6D, 0x76, 0x7F, 0x40, 0x49, 0x52, 0x5B,
0x2C, 0x25, 0x3E, 0x37, 0x08, 0x01, 0x1A, 0x13,
0x7D, 0x74, 0x6F, 0x66, 0x59, 0x50, 0x4B, 0x42,
0x35, 0x3C, 0x27, 0x2E, 0x11, 0x18, 0x03, 0x0A,
0x56, 0x5F, 0x44, 0x4D, 0x72, 0x7B, 0x60, 0x69,
0x1E, 0x17, 0x0C, 0x05, 0x3A, 0x33, 0x28, 0x21,
0x4F, 0x46, 0x5D, 0x54, 0x6B, 0x62, 0x79, 0x70,
0x07, 0x0E, 0x15, 0x1C, 0x23, 0x2A, 0x31, 0x38,
0x41, 0x48, 0x53, 0x5A, 0x65, 0x6C, 0x77, 0x7E,
0x09, 0x00, 0x1B, 0x12, 0x2D, 0x24, 0x3F, 0x36,
0x58, 0x51, 0x4A, 0x43, 0x7C, 0x75, 0x6E, 0x67,
0x10, 0x19, 0x02, 0x0B, 0x34, 0x3D, 0x26, 0x2F,
0x73, 0x7A, 0x61, 0x68, 0x57, 0x5E, 0x45, 0x4C,
0x3B, 0x32, 0x29, 0x20, 0x1F, 0x16, 0x0D, 0x04,
0x6A, 0x63, 0x78, 0x71, 0x4E, 0x47, 0x5C, 0x55,
0x22, 0x2B, 0x30, 0x39, 0x06, 0x0F, 0x14, 0x1D,
0x25, 0x2C, 0x37, 0x3E, 0x01, 0x08, 0x13, 0x1A,
0x6D, 0x64, 0x7F, 0x76, 0x49, 0x40, 0x5B, 0x52,
0x3C, 0x35, 0x2E, 0x27, 0x18, 0x11, 0x0A, 0x03,
0x74, 0x7D, 0x66, 0x6F, 0x50, 0x59, 0x42, 0x4B,
0x17, 0x1E, 0x05, 0x0C, 0x33, 0x3A, 0x21, 0x28,
0x5F, 0x56, 0x4D, 0x44, 0x7B, 0x72, 0x69, 0x60,
0x0E, 0x07, 0x1C, 0x15, 0x2A, 0x23, 0x38, 0x31,
0x46, 0x4F, 0x54, 0x5D, 0x62, 0x6B, 0x70, 0x79,
};

static UINT8 ComputeCRC7(PUCHAR pBuffer, UINT Length) 
{
  UINT8 crc7_accum = 0;
  UINT ii;
  for (ii = 0;  ii < Length;  ++ii)
    {
      crc7_accum =
           crc7Table[(crc7_accum << 1) ^ pBuffer[ii]];
    }
    return crc7_accum <<1;
}
static BOOL TestCRC16(PUCHAR pBuffer, UINT Length, UINT16 Crc) 
{
    UINT16 tempCrc;

    tempCrc = ComputeCRC16(pBuffer, Length);
    DBG_PRINT(SDDBG_TRACE, ("SDIO SPIlib TestCRC16 - computed: 0x%X, buffer crc: 0x%X, buf[0,1] 0x%X, 0x%X, Length: %d\n", 
        (UINT)tempCrc, (UINT)Crc, (UINT)pBuffer[0], (UINT)pBuffer[1], Length));
    if (tempCrc  == Crc) {
        return true;
    } else {
        return false;
    }
}
#if 0
static BOOL TestCRC7(PUCHAR pBuffer, UINT Length, UINT8 Crc) 
{
    return ComputeCRC7(pBuffer, Length) == Crc;
}
#endif
