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
@file: cpsystem.h

@abstract: common system include file.
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __CPSYSTEM_H___
#define __CPSYSTEM_H___

/* SDIO stack status defines */
/* < 0 error, >0 warning, 0 success */
#define SDIO_IS_WARNING(status) ((status) > 0)
#define SDIO_IS_ERROR(status) ((status) < 0)
#define SDIO_SUCCESS(status) ((SDIO_STATUS)(status) >= 0)
#define SDIO_STATUS_SUCCESS             0
#define SDIO_STATUS_ERROR              -1
#define SDIO_STATUS_INVALID_PARAMETER  -2
#define SDIO_STATUS_PENDING             3
#define SDIO_STATUS_DEVICE_NOT_FOUND   -4
#define SDIO_STATUS_DEVICE_ERROR       -5
#define SDIO_STATUS_INTERRUPTED        -6
#define SDIO_STATUS_NO_RESOURCES       -7  
#define SDIO_STATUS_CANCELED           -8  
#define SDIO_STATUS_BUFFER_TOO_SMALL   -9
#define SDIO_STATUS_NO_MORE_MESSAGES   -10
#define SDIO_STATUS_BUS_RESP_TIMEOUT   -20    /* response timed-out */
#define SDIO_STATUS_BUS_READ_TIMEOUT   -21    /* read data timed-out */
#define SDIO_STATUS_BUS_READ_CRC_ERR   -22   /* data CRC failed */
#define SDIO_STATUS_BUS_WRITE_ERROR    -23   /* write failed */
#define SDIO_STATUS_BUS_RESP_CRC_ERR   -24   /* response received with a CRC error */
#define SDIO_STATUS_INVALID_TUPLE_LENGTH -25 /* tuple length was invalid */
#define SDIO_STATUS_TUPLE_NOT_FOUND      -26 /* tuple could not be found */
#define SDIO_STATUS_CIS_OUT_OF_RANGE     -27 /* CIS is out of range in the tuple scan */
#define SDIO_STATUS_FUNC_ENABLE_TIMEOUT  -28 /* card timed out enabling or disabling */
#define SDIO_STATUS_DATA_STATE_INVALID   -29 /* card is in an invalid state for data */
#define SDIO_STATUS_DATA_ERROR_UNKNOWN   -30 /* card cannot process data transfer */ 
#define SDIO_STATUS_INVALID_FUNC         -31 /* sdio request is not valid for the function */ 
#define SDIO_STATUS_FUNC_ARG_ERROR       -32 /* sdio request argument is invalid or out of range */ 
#define SDIO_STATUS_INVALID_COMMAND      -33 /* SD COMMAND is invalid for the card state */ 
#define SDIO_STATUS_SDREQ_QUEUE_FAILED   -34 /* request failed to insert into queue */ 
#define SDIO_STATUS_BUS_RESP_TIMEOUT_SHIFTABLE -35  /* response timed-out, possibily shiftable to correct  */
#define SDIO_STATUS_UNSUPPORTED          -36  /* not supported  */
#define SDIO_STATUS_PROGRAM_TIMEOUT      -37  /* memory card programming timeout  */
#define SDIO_STATUS_PROGRAM_STATUS_ERROR -38  /* memory card programming errors  */

#ifdef VXWORKS
/* Wind River VxWorks support */
#include "vxworks/ctsystem_vxworks.h"
#endif /* VXWORKS */

#if defined(LINUX) || defined(__linux__)
/* Linux support */
#include "linux/ctsystem_linux.h"
#endif /* LINUX */

#ifdef QNX
/* QNX Neutrino support */
#include "nto/ctsystem_qnx.h"
#endif /* QNX */

#ifdef INTEGRITY
/* Greenhils Integrity support */
#include "integrity/ctsystem_integrity.h"
#endif /* INTEGRITY */

#ifdef NUCLEUS_PLUS
/* Mentor Graphics Nucleus support */
#include "nucleus/ctsystem_nucleus.h"
#endif /* NUCLEUS_PLUS */

#ifdef UNDER_CE
#define CTSYSTEM_NO_FUNCTION_PROXIES
/* Windows CE  support */
#include "wince/ctsystem_wince.h"
#endif /* WINCE */


/* get structure from contained field */
#define CONTAINING_STRUCT(address, struct_type, field_name)\
            ((struct_type *)((ULONG_PTR)(address) - (ULONG_PTR)(&((struct_type *)0)->field_name)))

#define ZERO_OBJECT(obj) memset(&(obj),0,sizeof(obj))    
#define ZERO_POBJECT(pObj) memset((pObj),0,sizeof(*(pObj)))  
    
    
/* bit field support functions */
static INLINE void SetBit(PULONG pField, UINT position) {
    *pField |= 1 << position;
}
static INLINE void ClearBit(PULONG pField, UINT position) {
    *pField &= ~(1 << position);
}
static INLINE BOOL IsBitSet(PULONG pField, UINT position) {
    return (*pField & (1 << position));
}
static INLINE INT FirstClearBit(PULONG pField) {
    UINT ii;
    for(ii = 0; ii < sizeof(ULONG)*8; ii++) {
        if (!IsBitSet(pField, ii)) {
            return ii;
        }  
    }
    /* no clear bits found */
    return -1;
}

#endif /* __CPSYSTEM_H___ */
