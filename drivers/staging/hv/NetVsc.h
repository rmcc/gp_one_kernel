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
 *   Hank Janssen  <hjanssen@microsoft.com>
 *
 */


#ifndef _NETVSC_H_
#define _NETVSC_H_

#include "VmbusPacketFormat.h"
#include "nvspprotocol.h"

#include "List.h"

#include "NetVscApi.h"
//
// #defines
//
//#define NVSC_MIN_PROTOCOL_VERSION                       1
//#define NVSC_MAX_PROTOCOL_VERSION                       1

#define NETVSC_SEND_BUFFER_SIZE				64*1024 // 64K
#define NETVSC_SEND_BUFFER_ID				0xface


#define NETVSC_RECEIVE_BUFFER_SIZE			1024*1024 // 1MB

#define NETVSC_RECEIVE_BUFFER_ID			0xcafe

#define NETVSC_RECEIVE_SG_COUNT				1

// Preallocated receive packets
#define NETVSC_RECEIVE_PACKETLIST_COUNT		256

//
// Data types
//

// Per netvsc channel-specific
typedef struct _NETVSC_DEVICE {
	DEVICE_OBJECT					*Device;

	int								RefCount;

	int								NumOutstandingSends;
	// List of free preallocated NETVSC_PACKET to represent receive packet
	LIST_ENTRY						ReceivePacketList;
	HANDLE							ReceivePacketListLock;

	// Send buffer allocated by us but manages by NetVSP
	PVOID							SendBuffer;
	UINT32							SendBufferSize;
	UINT32							SendBufferGpadlHandle;
	UINT32							SendSectionSize;

	// Receive buffer allocated by us but manages by NetVSP
	PVOID							ReceiveBuffer;
	UINT32							ReceiveBufferSize;
	UINT32							ReceiveBufferGpadlHandle;
	UINT32							ReceiveSectionCount;
	PNVSP_1_RECEIVE_BUFFER_SECTION	ReceiveSections;

	// Used for NetVSP initialization protocol
	HANDLE							ChannelInitEvent;
	NVSP_MESSAGE					ChannelInitPacket;

	NVSP_MESSAGE					RevokePacket;
	//UCHAR							HwMacAddr[HW_MACADDR_LEN];

	// Holds rndis device info
	void							*Extension;
} NETVSC_DEVICE;

#endif // _NETVSC_H_
