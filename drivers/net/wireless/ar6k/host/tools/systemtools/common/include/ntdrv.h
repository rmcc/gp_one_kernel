/*
 *  Copyright © 2000-2001 Atheros Communications, Inc.,  All Rights Reserved.
 */
/* ntdrv.h (WinNT) specific declarations */

#ifndef	__INCntdrvh
#define	__INCntdrvh

#ident  "ACI $Header: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/common/include/ntdrv.h#1 $"

/*
DESCRIPTION
This file contains the WinNT declarations for some 
data structures and macros to be used at application level
*/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#ifndef __ATH_DJGPPDOS__
#include <windows.h>
typedef	HANDLE	FHANDLE;
#else
#define HANDLE A_INT32
#endif

#ifndef A_MEM_ZERO
#define A_MEM_ZERO(addr, size)          memset(addr, 0, size)
#endif

#define A_MALLOC(a)	(malloc(a))
#define A_FREE(a)	(free(a))
#define A_DRIVER_MALLOC(a)	(malloc(a))
#define A_DRIVER_FREE(a, b)	(free(a))
#define A_DRIVER_BCOPY(from, to, len) (memcpy((void *)(to), (void *)(from), (len)))
#define A_BCOPY(from, to, len) (memcpy((void *)(to), (void *)(from), (len)))
#define A_BCOMP(s1, s2, len) (memcmp((void *)(s1), (void *)(s2), (len)))
#define A_MACADDR_COPY(from, to) ((void *) memcpy(&((to)->octets[0]),&((from)->octets[0]),WLAN_MAC_ADDR_SIZE))
#define A_MACADDR_COMP(m1, m2) ( memcmp((char *)&((m1)->octets[0]),\
											(char *)&((m2)->octets[0]),\
											WLAN_MAC_ADDR_SIZE))

/* Locking macros 
 * Currently, they are all empty. They will be eventually
 * written as call to NDIS dependent function.
 */

#define A_SEM_LOCK(sem, param)
#define A_SEM_UNLOCK(sem)
#define A_SIB_TAB_LOCK()
#define A_SIB_TAB_UNLOCK()
#define A_SIB_ENTRY_LOCK(s)
#define A_SIB_ENTRY_UNLOCK(s)

#define A_INIT_TIMER(func, param)
#define A_TIMEOUT(func, period, param)
#define A_UNTIMEOUT(handle)

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __INCntdrvh */
