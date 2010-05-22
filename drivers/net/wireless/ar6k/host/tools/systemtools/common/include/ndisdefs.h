/*
 * $Id: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/common/include/ndisdefs.h#1 $
 *
 * NDIS-specific needed by diags
 *
 * Copyright (c) 2000-2003 Atheros Communications, Inc., All Rights Reserved
 */

#ifndef _NDISDEFS_H_
#define _NDISDEFS_H_

//#include <ndis.h>
//#include "wlantype.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Workaround to create our own ASSERT() macro bearing the same name as that
 * defined in ndis.h.  Pretty hacky as the following is copied directly from
 * a particular version of ndis.h
 */
/*#if DBG
extern int asserts; // defined in wlanglobal.c
#undef ASSERT
#define oldAssert( exp ) \
    ((!(exp)) ? \
        (RtlAssert( #exp, __FILE__, __LINE__, NULL ),FALSE) : \
        TRUE)
#define ASSERT(x) if (asserts) oldAssert(x)
#endif // DBG
*/
/* temporary workaround for Unix definition of FILE */

//#define FILE    void

/* LOCAL keyword doesn't exist under VS C++ */
#define LOCAL   

/* Driver specific data types */
struct osDevInfo;
typedef struct osDevInfo OS_DEV_INFO;
typedef struct {
    int initialized;    /* for now, just need to track initialized or not */
} A_SEM_TYPE;


/* Buffer management */


#ifdef __cplusplus
}
#endif

#endif // _NDISDEFS_H_
