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


#ifndef _NETVSC_API_H_
#define _NETVSC_API_H_

#include "VmbusApi.h"

//
// Defines
//
#define NETVSC_DEVICE_RING_BUFFER_SIZE			64*PAGE_SIZE

#define HW_MACADDR_LEN		6

//
// Fwd declaration
//
typedef struct _NETVSC_PACKET	*PNETVSC_PACKET;


//
// Data types
//

typedef int (*PFN_ON_OPEN)(DEVICE_OBJECT *Device);
typedef int (*PFN_ON_CLOSE)(DEVICE_OBJECT *Device);

typedef void (*PFN_QUERY_LINKSTATUS)(DEVICE_OBJECT *Device);
typedef int (*PFN_ON_SEND)(DEVICE_OBJECT *dev, PNETVSC_PACKET packet);
typedef void (*PFN_ON_SENDRECVCOMPLETION)(PVOID Context);

typedef int (*PFN_ON_RECVCALLBACK)(DEVICE_OBJECT *dev, PNETVSC_PACKET packet);
typedef void (*PFN_ON_LINKSTATUS_CHANGED)(DEVICE_OBJECT *dev, UINT32 Status);

// Represent the xfer page packet which contains 1 or more netvsc packet
typedef struct _XFERPAGE_PACKET {
	DLIST_ENTRY			ListEntry;

	// # of netvsc packets this xfer packet contains
	UINT32				Count;
} XFERPAGE_PACKET;


// The number of pages which are enough to cover jumbo frame buffer.
#define NETVSC_PACKET_MAXPAGE  4

// Represent netvsc packet which contains 1 RNDIS and 1 ethernet frame within the RNDIS
typedef struct _NETVSC_PACKET {
	// Bookkeeping stuff
	DLIST_ENTRY				ListEntry;

	DEVICE_OBJECT			*Device;
	BOOL					IsDataPacket;

	// Valid only for receives when we break a xfer page packet into multiple netvsc packets
	XFERPAGE_PACKET		*XferPagePacket;

	union {
		struct{
			UINT64						ReceiveCompletionTid;
			PVOID						ReceiveCompletionContext;
			PFN_ON_SENDRECVCOMPLETION	OnReceiveCompletion;
		} Recv;
		struct{
			UINT64						SendCompletionTid;
			PVOID						SendCompletionContext;
			PFN_ON_SENDRECVCOMPLETION	OnSendCompletion;
		} Send;
	} Completion;

	// This points to the memory after PageBuffers
	PVOID					Extension;

	UINT32					TotalDataBufferLength;
	// Points to the send/receive buffer where the ethernet frame is
	UINT32					PageBufferCount;
	PAGE_BUFFER				PageBuffers[NETVSC_PACKET_MAXPAGE];

} NETVSC_PACKET;


// Represents the net vsc driver
typedef struct _NETVSC_DRIVER_OBJECT {
	DRIVER_OBJECT				Base; // Must be the first field

	UINT32						RingBufferSize;
	UINT32						RequestExtSize;

	// Additional num  of page buffers to allocate
	UINT32						AdditionalRequestPageBufferCount;

	// This is set by the caller to allow us to callback when we receive a packet
	// from the "wire"
	PFN_ON_RECVCALLBACK			OnReceiveCallback;

	PFN_ON_LINKSTATUS_CHANGED	OnLinkStatusChanged;

	// Specific to this driver
	PFN_ON_OPEN					OnOpen;
	PFN_ON_CLOSE				OnClose;
	PFN_ON_SEND					OnSend;
	//PFN_ON_RECVCOMPLETION	OnReceiveCompletion;

	//PFN_QUERY_LINKSTATUS		QueryLinkStatus;

	void*						Context;
} NETVSC_DRIVER_OBJECT;


typedef struct _NETVSC_DEVICE_INFO {
    UCHAR	MacAddr[6];
    BOOL	LinkState;	// 0 - link up, 1 - link down
} NETVSC_DEVICE_INFO;

//
// Interface
//
int
NetVscInitialize(
	DRIVER_OBJECT* drv
	);

#endif // _NETVSC_API_H_
