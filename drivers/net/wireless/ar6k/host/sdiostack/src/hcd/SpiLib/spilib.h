// Copyright (c) 2005 Atheros Communications Inc.
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
@file: spilib.h

@abstract: include file for SPI library functions
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __SDIO_SPILIB_H___
#define __SDIO_SPILIB_H___

#include "../../include/ctsystem.h"
#include "../../include/sdio_busdriver.h"
#include "../../include/sdio_lib.h"


typedef enum _SDSPI_STATE {
    NO_STATE = 0,
    CMD_RESPONSE_START,
    CMD_RESPONSE_START_OR_RESPONSE,
    CMD_RESPONSE,
    CMD_RESPONSE_BUSY,
    CMD_RESPONSE_BUSY_BUSY,
    DATA_RESPONSE_READ_STATUS,
    DATA_RESPONSE_READ_RESPONSE,
    DATA_RESPONSE_READ_START,
    DATA_RESPONSE_READ,
    DATA_RESPONSE_READ_CRC,
    DATA_RESPONSE_READ_TRAIL,
    DATA_RESPONSE_WRITE_STATUS,
    DATA_RESPONSE_WRITE_RESPONSE,
    DATA_RESPONSE_WRITE_START,
    DATA_RESPONSE_WRITE,
    DATA_RESPONSE_WRITE_DATA_STATUS,
    DATA_RESPONSE_WRITE_BUSY,
    DATA_STOPPED,
    CRC_ERROR
}SDSPI_STATE, *PSDSPI_STATE;

typedef struct _SDSPI_CONFIG {
    SDSPI_STATE State;          /* internal, state of SPI library */
    PSDREQUEST  pRequest;       /* internal, pointer to request */
    INT         DataSize;       /* internal, expected size of returned data */
    INT         BlockSize;      /* internal, block size for data transfers */
    INT         BlockCount;     /* internal, number of blocks for data transfers */
    INT         TempCount;      /* internal, temp count of leading bytes */
    INT         MaxReadTimeoutBytes; /* the maximum number of leading 0xFF bytes on a read before a timeout */
    UINT8       TempData[2];    /* internal, temp data value */
    INT         ShiftOffset;    /* bit shift offset for imcoming buffer */
    BOOL        ExpectBusy;     /* internal, expect busy response */ 
    BOOL        EnableCRC;      /* TRUE, CRC checking is enabled */ 
    BOOL        ReverseResponse;/* TRUE, reverse the order of the response bytes */
    BOOL        HandleBitShift; /* shift incoming bits if start bits are offset */
}SDSPI_CONFIG, *PSDSPI_CONFIG;

/* number of bytes to request as leading high bytes in a read, plus the number of bytes to add
   to request to keep clock on past end of command */
#ifndef SPILIB_LEADING_READ_BYTES
#define SPILIB_LEADING_READ_BYTES 8+1
#endif

/* need extra bytes to handle bit shifting */
#define SPILIB_LEADING_FF   10
/* max bytes read looking for SPI_READ_START_TOKEN */
#define SPILIB_READ_LEADING_FF   49152
/* number of bytes sent at one time looking for non-busy */
#define SPILIB_WRITE_BUSY_FF   256

/* room we need in the data buffer when doing writes for the command portion */
#define SPILIB_DATA_BUFFER_CMD_SPACE 6

/* size of buffer required for commands */
#define SPILIB_COMMAND_BUFFER_SIZE          6
#define SPILIB_COMMAND_MAX_RESPONSE_SIZE    5

#define SPI_WRITE_START_TOKEN               0xFE
#define SPI_WRITE_START_TOKEN_MULTIBLOCK    0xFC
#define SPI_WRITE_STOP_TOKEN_MULTIBLOCK     0xFD
#define SPI_READ_START_TOKEN                0xFE

#define SPI_DATA_RESPONSE_STATUS_MASK       0x0E
#define SPI_DATA_RESPONSE_STATUS_ACCEPTED   0x04
#define SPI_DATA_RESPONSE_STATUS_CRCERROR   0x0A
#define SPI_DATA_RESPONSE_STATUS_WRITEERROR 0x0C

#define SPI_DATAERR_ERROR      (1 << 0)
#define SPI_DATAERR_CCERROR    (1 << 1)
#define SPI_DATAERR_ECCERROR   (1 << 2)
#define SPI_DATAERR_OUTOFRANGE (1 << 3)

/* prototypes */
SDIO_STATUS SDSpi_PrepCommand(PSDSPI_CONFIG pConfig, PSDREQUEST pRequest, UINT Length, PUINT8 pDataBuffer, PUINT pRemainingBytes);
SDIO_STATUS SDSpi_ProcessResponse(PSDSPI_CONFIG pConfig, PUCHAR pDataBuffer, UINT DataLength, PUINT pRemainingBytes);
SDIO_STATUS SDSpi_ProcessDataBlock(PSDSPI_CONFIG pConfig, PUCHAR pDataBuffer, UINT DataLength, 
                                   PUINT pLength, PUINT pRemainingBytes);
void SDSpi_CorrectErrorShift(PSDSPI_CONFIG pConfig, PUINT8 pInBuffer, UINT Length, PUINT8 pOutputBuffer, UINT8 StartByte);

#endif /* __SDIO_SPILIB_H___ */
