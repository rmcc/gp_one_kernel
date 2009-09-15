/*
 *
 * Copyright (c) 2009, Microsoft Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307 USA.
 *
 * Authors:
 *   Haiyang Zhang <haiyangz@microsoft.com>
 *   Hank Janssen  <hjanssen@microsoft.com>
 *
 */


#ifndef _OSD_H_
#define _OSD_H_


/* Defines */



#define ALIGN_UP(value, align)			( ((value) & (align-1))? ( ((value) + (align-1)) & ~(align-1) ): (value) )
#define ALIGN_DOWN(value, align)		( (value) & ~(align-1) )
#define NUM_PAGES_SPANNED(addr, len)	( (ALIGN_UP(addr+len, PAGE_SIZE) - ALIGN_DOWN(addr, PAGE_SIZE)) >> PAGE_SHIFT )

#define LOWORD(dw)		((unsigned short) (dw))
#define HIWORD(dw)		((unsigned short) (((unsigned int) (dw) >> 16) & 0xFFFF))

typedef struct _DLIST_ENTRY {
   struct _DLIST_ENTRY *Flink;
   struct _DLIST_ENTRY *Blink;
} DLIST_ENTRY;


/* Other types */

/* typedef unsigned char		GUID[16]; */
typedef void*				HANDLE;

typedef struct {
	unsigned char	Data[16];
} GUID;

typedef void (*PFN_WORKITEM_CALLBACK)(void* context);
typedef void (*PFN_TIMER_CALLBACK)(void* context);


#ifdef __x86_64__

#define RDMSR(reg, v) {                                                        \
    u32 h, l;                                                                 \
     __asm__ __volatile__("rdmsr"                                                               \
    : "=a" (l), "=d" (h)                                                       \
    : "c" (reg));                                                              \
    v = (((u64)h) << 32) | l;                                                         \
}

#define WRMSR(reg, v) {                                                        \
    u32 h, l;                                                               \
    l = (u32)(((u64)(v)) & 0xFFFFFFFF);                                  \
    h = (u32)((((u64)(v)) >> 32) & 0xFFFFFFFF);                          \
     __asm__ __volatile__("wrmsr"                                              \
    : /* no outputs */                                                         \
    : "c" (reg), "a" (l), "d" (h));                                            \
}

#else

#define RDMSR(reg, v) 			                                               \
     __asm__ __volatile__("rdmsr" 	                                           \
    : "=A" (v) 			                                                       \
    : "c" (reg))

#define WRMSR(reg, v) 			                                               \
     __asm__ __volatile__("wrmsr" 	                                           \
    : /* no outputs */ 				                                           \
    : "c" (reg), "A" ((u64)v))

#endif


static inline void do_cpuid(unsigned int op, unsigned int *eax, unsigned int *ebx, unsigned int *ecx, unsigned int *edx)
{
	__asm__ __volatile__("cpuid" : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx) : "0" (op), "c" (ecx));
}


/* Osd routines */

extern void BitSet(unsigned int* addr, int value);
extern void BitClear(unsigned int* addr, int value);
extern int BitTest(unsigned int* addr, int value);
extern int BitTestAndClear(unsigned int* addr, int value);
extern int BitTestAndSet(unsigned int* addr, int value);

extern int InterlockedIncrement(int *val);
extern int InterlockedDecrement(int *val);
extern int InterlockedCompareExchange(int *val, int new, int curr);

extern void* VirtualAllocExec(unsigned int size);
extern void VirtualFree(void* VirtAddr);

extern void* PageAlloc(unsigned int count);
extern void PageFree(void* page, unsigned int count);

extern void* MemMapIO(unsigned long phys, unsigned long size);
extern void MemUnmapIO(void* virt);

extern HANDLE TimerCreate(PFN_TIMER_CALLBACK pfnTimerCB, void* context);
extern void TimerClose(HANDLE hTimer);
extern int TimerStop(HANDLE hTimer);
extern void TimerStart(HANDLE hTimer, u32 expirationInUs);

extern HANDLE WaitEventCreate(void);
extern void WaitEventClose(HANDLE hWait);
extern void WaitEventSet(HANDLE hWait);
extern int	WaitEventWait(HANDLE hWait);

/* If >0, hWait got signaled. If ==0, timeout. If < 0, error */
extern int	WaitEventWaitEx(HANDLE hWait, u32 TimeoutInMs);


#define GetVirtualAddress Physical2LogicalAddr
void* Physical2LogicalAddr(unsigned long PhysAddr);

#define GetPhysicalAddress Logical2PhysicalAddr
unsigned long Logical2PhysicalAddr(void * LogicalAddr);

unsigned long Virtual2Physical(void * VirtAddr);

void* PageMapVirtualAddress(unsigned long Pfn);
void PageUnmapVirtualAddress(void* VirtAddr);


extern HANDLE WorkQueueCreate(char* name);
extern void WorkQueueClose(HANDLE hWorkQueue);
extern int WorkQueueQueueWorkItem(HANDLE hWorkQueue, PFN_WORKITEM_CALLBACK workItem, void* context);

extern void QueueWorkItem(PFN_WORKITEM_CALLBACK workItem, void* context);

#endif /* _OSD_H_ */
