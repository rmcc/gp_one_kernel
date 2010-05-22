/*
 *  Copyright © 2000-2001 Atheros Communications, Inc.,  All Rights Reserved.
 */

/* vxdrv.h  vxWorks specific declarations */

#ifndef	__INCvxdrvh
#define	__INCvxdrvh

#ident  "ACI $Header: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/common/include/vxdrv.h#1 $"

/*
DESCRIPTION
This file contains the vxWorks declarations for some
data structures and macros
*/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "wlantype.h"
#include "semLib.h"
#include "tickLib.h"
#include "string.h"
#include "assert.h"
#include "end.h"
#include "stdlib.h"
#include "memLib.h"
#include "cacheLib.h"

#define A_MALLOC(a)					(malloc(a))
#define A_FREE(a)					(free(a))
#define A_DRIVER_MALLOC(a)			(malloc(a))
#define A_DRIVER_FREE(a, b)				(free(a))
#define A_MEM_SET(addr, value, size) 			memset((char *)(addr), (int)(value), (int)(size))
#define	A_MEM_ZERO(addr, size)				bzero((char *)(addr), (int)(size))
#define A_DRIVER_BCOPY(from, to, len) 			bcopy((char *)(from), \
							(char *)(to), (int)(len))
#define	A_BCOPY(a, b, n)				bcopy((char *)(a), (char *)(b), (int)(n))
#define A_BCOMP(p1, p2, n) 				bcmp((char *)(p1), (char *)(p2), (int)(n))

/*
 * Hosts that perform unaligned acess well can use the fast
 * comparision code.
 */
#if CPU_FAMILY == PPC
#define A_MACADDR_COPY(from, to) \
	do { \
		(to)->st.word = (from)->st.word; \
		(to)->st.half = (from)->st.half; \
	} while (0)
#define A_MACADDR_COMP(m1, m2) \
	(((m1)->st.half != (m2)->st.half) || \
	 ((m1)->st.word != (m2)->st.word))
#else
#define A_MACADDR_COPY(from, to) \
	((void *) memcpy(&((to)->octets[0]),&((from)->octets[0]),\
		WLAN_MAC_ADDR_SIZE))
#define A_MACADDR_COMP(m1, m2) ((void *)bcmp((char *)&((m1)->octets[0]),\
	  (char *)&((m2)->octets[0]), WLAN_MAC_ADDR_SIZE))
#endif

#if defined(SPIRIT_AP) || defined(FREEDOM_AP)
#define SWCOHERENCY

#if defined(AR5312)
#include "ar531x.h"
#endif

#if defined(AR5315)
#include "ar531xPlus.h"
#endif

#ifdef DELTA
#undef DELTA
#endif
#define A_DATA_CACHE_INVAL(addr, len)   cacheDataInvalidate(addr.ptr, (len))
#define A_DATA_CACHE_FLUSH(addr, len)
#define A_DATA_V2P(addr)                ((A_UINT32)(addr))
#define A_DATA_P2V(addr)                (addr)
#define A_DESC_CACHE_INVAL(addr)        cacheDescInvalidate((void *)(&(addr)->hw[0]))
#define A_DESC_CACHE_FLUSH(addr)
#define A_DESC_V2P(addr)                ((A_UINT32)(addr))
#define A_DESC_P2V(addr)                (addr)
#define A_PIPEFLUSH()                   sysWbFlush()
#endif

#ifdef AP22_AP
#define A_DATA_CACHE_INVAL(addr, len)
#define A_DATA_CACHE_FLUSH(addr, len)
#define A_DATA_V2P(addr)                ((A_UINT32)(addr))
#define A_DATA_P2V(addr)                (addr)
#define A_DESC_CACHE_INVAL(addr)
#define A_DESC_CACHE_FLUSH(addr)
#define A_DESC_V2P(addr)                ((A_UINT32)(addr))
#define A_DESC_P2V(addr)                (addr)
#define A_PIPEFLUSH()
#endif

#ifdef SENAO_AP
#include "pb32.h"
#define SWCOHERENCY
#define VX_CACHE_DMA_MALLOC
#define A_DATA_CACHE_INVAL(addr, len)   cacheDataInvalidate(addr.ptr, (len))
#define A_DATA_CACHE_FLUSH(addr, len)
#define A_DATA_V2P(addr)                ((A_UINT32)(CACHE_DRV_VIRT_TO_PHYS(&cacheDmaFuncs, addr)))
#define A_DATA_P2V(addr)                (CACHE_DRV_PHYS_TO_VIRT(&cacheDmaFuncs, addr)) 
#define A_DESC_CACHE_INVAL(addr)        cacheDescInvalidate((void *)(&(addr)->hw[0]))
#define A_DESC_CACHE_FLUSH(addr)
#define A_DESC_V2P(addr)                ((A_UINT32)A_DATA_V2P(addr))
#define A_DESC_P2V(addr)                A_DATA_P2V(addr)
#define A_PIPEFLUSH()                   sysWbFlush()
#endif

#define ASSERT assert

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __INCvxdrvh */
