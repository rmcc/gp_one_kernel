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


#ifndef _VMBUS_API_H_
#define _VMBUS_API_H_

#include "osd.h"

//
// Defines
//

#define MAX_PAGE_BUFFER_COUNT				16
#define MAX_MULTIPAGE_BUFFER_COUNT			32 // 128K


//
// Fwd declarations
//
typedef struct _DRIVER_OBJECT *PDRIVER_OBJECT;
typedef struct _DEVICE_OBJECT *PDEVICE_OBJECT;

//
// Data types
//

#pragma pack(push,1)

// Single-page buffer
typedef struct _PAGE_BUFFER {
	UINT32	Length;
	UINT32	Offset;
	UINT64	Pfn;
} PAGE_BUFFER;

// Multiple-page buffer
typedef struct _MULTIPAGE_BUFFER {
	// Length and Offset determines the # of pfns in the array
	UINT32	Length;
	UINT32	Offset;
	UINT64	PfnArray[MAX_MULTIPAGE_BUFFER_COUNT];
}MULTIPAGE_BUFFER;

//0x18 includes the proprietary packet header
#define MAX_PAGE_BUFFER_PACKET			(0x18 + (sizeof(PAGE_BUFFER) * MAX_PAGE_BUFFER_COUNT))
#define MAX_MULTIPAGE_BUFFER_PACKET		(0x18 + sizeof(MULTIPAGE_BUFFER))


#pragma pack(pop)

// All drivers
typedef int (*PFN_ON_DEVICEADD)(PDEVICE_OBJECT Device, void* AdditionalInfo);
typedef int (*PFN_ON_DEVICEREMOVE)(PDEVICE_OBJECT Device);
typedef char** (*PFN_ON_GETDEVICEIDS)(void);
typedef void (*PFN_ON_CLEANUP)(PDRIVER_OBJECT Driver);

// Vmbus extensions
//typedef int (*PFN_ON_MATCH)(PDEVICE_OBJECT dev, PDRIVER_OBJECT drv);
//typedef int (*PFN_ON_PROBE)(PDEVICE_OBJECT dev);
typedef int	(*PFN_ON_ISR)(PDRIVER_OBJECT drv);
typedef void (*PFN_ON_DPC)(PDRIVER_OBJECT drv);
typedef void (*PFN_GET_CHANNEL_OFFERS)(void);

typedef PDEVICE_OBJECT (*PFN_ON_CHILDDEVICE_CREATE)(GUID DeviceType, GUID DeviceInstance, void *Context);
typedef void (*PFN_ON_CHILDDEVICE_DESTROY)(PDEVICE_OBJECT Device);
typedef int (*PFN_ON_CHILDDEVICE_ADD)(PDEVICE_OBJECT RootDevice, PDEVICE_OBJECT ChildDevice);
typedef void (*PFN_ON_CHILDDEVICE_REMOVE)(PDEVICE_OBJECT Device);

// Vmbus channel interface
typedef void (*VMBUS_CHANNEL_CALLBACK)(PVOID context);

typedef int	(*VMBUS_CHANNEL_OPEN)(
	PDEVICE_OBJECT		Device,
	UINT32				SendBufferSize,
	UINT32				RecvRingBufferSize,
	PVOID				UserData,
	UINT32				UserDataLen,
	VMBUS_CHANNEL_CALLBACK ChannelCallback,
	PVOID				Context
	);

typedef void (*VMBUS_CHANNEL_CLOSE)(
	PDEVICE_OBJECT		Device
	);

typedef int	(*VMBUS_CHANNEL_SEND_PACKET)(
	PDEVICE_OBJECT		Device,
	const PVOID			Buffer,
	UINT32				BufferLen,
	UINT64				RequestId,
	UINT32				Type,
	UINT32				Flags
);

typedef int	(*VMBUS_CHANNEL_SEND_PACKET_PAGEBUFFER)(
	PDEVICE_OBJECT		Device,
	PAGE_BUFFER			PageBuffers[],
	UINT32				PageCount,
	PVOID				Buffer,
	UINT32				BufferLen,
	UINT64				RequestId
	);

typedef int	(*VMBUS_CHANNEL_SEND_PACKET_MULTIPAGEBUFFER)(
	PDEVICE_OBJECT		Device,
	MULTIPAGE_BUFFER	*MultiPageBuffer,
	PVOID				Buffer,
	UINT32				BufferLen,
	UINT64				RequestId
);

typedef int	(*VMBUS_CHANNEL_RECV_PACKET)(
	PDEVICE_OBJECT		Device,
	PVOID				Buffer,
	UINT32				BufferLen,
	UINT32*				BufferActualLen,
	UINT64*				RequestId
	);

typedef int	(*VMBUS_CHANNEL_RECV_PACKET_PAW)(
	PDEVICE_OBJECT		Device,
	PVOID				Buffer,
	UINT32				BufferLen,
	UINT32*				BufferActualLen,
	UINT64*				RequestId
	);

typedef int	(*VMBUS_CHANNEL_ESTABLISH_GPADL)(
	PDEVICE_OBJECT		Device,
	PVOID				Buffer,	// from kmalloc()
	UINT32				BufferLen,		// page-size multiple
	UINT32*				GpadlHandle
	);

typedef int	(*VMBUS_CHANNEL_TEARDOWN_GPADL)(
	PDEVICE_OBJECT		Device,
	UINT32				GpadlHandle
	);


typedef struct _PORT_INFO {
	UINT32		InterruptMask;
	UINT32		ReadIndex;
	UINT32		WriteIndex;
	UINT32		BytesAvailToRead;
	UINT32		BytesAvailToWrite;
} PORT_INFO;


typedef struct _DEVICE_INFO {
	UINT32		ChannelId;
	UINT32		ChannelState;
	GUID		ChannelType;
	GUID		ChannelInstance;

	UINT32						MonitorId;
	UINT32						ServerMonitorPending;
	UINT32						ServerMonitorLatency;
	UINT32						ServerMonitorConnectionId;
	UINT32						ClientMonitorPending;
	UINT32						ClientMonitorLatency;
	UINT32						ClientMonitorConnectionId;

	PORT_INFO	Inbound;
	PORT_INFO	Outbound;
} DEVICE_INFO;

typedef void (*VMBUS_GET_CHANNEL_INFO)(PDEVICE_OBJECT Device, DEVICE_INFO* DeviceInfo);

typedef struct _VMBUS_CHANNEL_INTERFACE {
	VMBUS_CHANNEL_OPEN							Open;
	VMBUS_CHANNEL_CLOSE							Close;
	VMBUS_CHANNEL_SEND_PACKET					SendPacket;
	VMBUS_CHANNEL_SEND_PACKET_PAGEBUFFER		SendPacketPageBuffer;
	VMBUS_CHANNEL_SEND_PACKET_MULTIPAGEBUFFER	SendPacketMultiPageBuffer;
	VMBUS_CHANNEL_RECV_PACKET					RecvPacket;
	VMBUS_CHANNEL_RECV_PACKET_PAW				RecvPacketRaw;
	VMBUS_CHANNEL_ESTABLISH_GPADL				EstablishGpadl;
	VMBUS_CHANNEL_TEARDOWN_GPADL				TeardownGpadl;
	VMBUS_GET_CHANNEL_INFO						GetInfo;
} VMBUS_CHANNEL_INTERFACE;

typedef void (*VMBUS_GET_CHANNEL_INTERFACE)(VMBUS_CHANNEL_INTERFACE *Interface);

// Base driver object
typedef struct _DRIVER_OBJECT {
	const char*				name;
	GUID					deviceType; // the device type supported by this driver

	PFN_ON_DEVICEADD		OnDeviceAdd;
	PFN_ON_DEVICEREMOVE		OnDeviceRemove;
	PFN_ON_GETDEVICEIDS		OnGetDeviceIds; // device ids supported by this driver
	PFN_ON_CLEANUP			OnCleanup;

	VMBUS_CHANNEL_INTERFACE VmbusChannelInterface;
} DRIVER_OBJECT;


// Base device object
typedef struct _DEVICE_OBJECT {
	DRIVER_OBJECT*		Driver;		// the driver for this device
	char				name[64];
	GUID				deviceType; // the device type id of this device
	GUID				deviceInstance; // the device instance id of this device
	void*				context;
	void*				Extension;		// Device extension;
} DEVICE_OBJECT;


// Vmbus driver object
typedef struct _VMBUS_DRIVER_OBJECT {
	DRIVER_OBJECT		Base; // !! Must be the 1st field !!

	// Set by the caller
	PFN_ON_CHILDDEVICE_CREATE	OnChildDeviceCreate;
	PFN_ON_CHILDDEVICE_DESTROY	OnChildDeviceDestroy;
	PFN_ON_CHILDDEVICE_ADD		OnChildDeviceAdd;
	PFN_ON_CHILDDEVICE_REMOVE	OnChildDeviceRemove;

	// Set by the callee
	//PFN_ON_MATCH		OnMatch;
	//PFN_ON_PROBE		OnProbe;
	PFN_ON_ISR				OnIsr;
	PFN_ON_DPC				OnMsgDpc;
	PFN_ON_DPC				OnEventDpc;
	PFN_GET_CHANNEL_OFFERS	GetChannelOffers;

	VMBUS_GET_CHANNEL_INTERFACE GetChannelInterface;
	VMBUS_GET_CHANNEL_INFO		GetChannelInfo;
} VMBUS_DRIVER_OBJECT;


//
// Interface
//
int
VmbusInitialize(
	DRIVER_OBJECT* drv
	);

#endif // _VMBUS_API_H_
