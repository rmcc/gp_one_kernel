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

#define MAX_PAGE_BUFFER_COUNT				16
#define MAX_MULTIPAGE_BUFFER_COUNT			32 /* 128K */

#pragma pack(push, 1)

/* Single-page buffer */
struct hv_page_buffer {
	u32 Length;
	u32 Offset;
	u64 Pfn;
};

/* Multiple-page buffer */
struct hv_multipage_buffer {
	/* Length and Offset determines the # of pfns in the array */
	u32 Length;
	u32 Offset;
	u64 PfnArray[MAX_MULTIPAGE_BUFFER_COUNT];
};

/* 0x18 includes the proprietary packet header */
#define MAX_PAGE_BUFFER_PACKET		(0x18 +			\
					(sizeof(struct hv_page_buffer) * \
					 MAX_PAGE_BUFFER_COUNT))
#define MAX_MULTIPAGE_BUFFER_PACKET	(0x18 +			\
					 sizeof(struct hv_multipage_buffer))


#pragma pack(pop)

struct hv_driver;
struct hv_device;

/* All drivers */
typedef int (*PFN_ON_DEVICEADD)(struct hv_device *Device,
				void *AdditionalInfo);
typedef int (*PFN_ON_DEVICEREMOVE)(struct hv_device *Device);
typedef char** (*PFN_ON_GETDEVICEIDS)(void);
typedef void (*PFN_ON_CLEANUP)(struct hv_driver *Driver);

/* Vmbus extensions */
typedef int (*PFN_ON_ISR)(struct hv_driver *drv);
typedef void (*PFN_ON_DPC)(struct hv_driver *drv);
typedef void (*PFN_GET_CHANNEL_OFFERS)(void);

typedef struct hv_device * (*PFN_ON_CHILDDEVICE_CREATE)
				(struct hv_guid *DeviceType,
				 struct hv_guid *DeviceInstance,
				 void *Context);
typedef void (*PFN_ON_CHILDDEVICE_DESTROY)(struct hv_device *Device);
typedef int (*PFN_ON_CHILDDEVICE_ADD)(struct hv_device *RootDevice,
				      struct hv_device *ChildDevice);
typedef void (*PFN_ON_CHILDDEVICE_REMOVE)(struct hv_device *Device);

/* Vmbus channel interface */
typedef void (*VMBUS_CHANNEL_CALLBACK)(void *context);
typedef int (*VMBUS_CHANNEL_OPEN)(struct hv_device *Device, u32 SendBufferSize,
				  u32 RecvRingBufferSize,
				  void *UserData,
				  u32 UserDataLen,
				  VMBUS_CHANNEL_CALLBACK ChannelCallback,
				  void *Context);
typedef void (*VMBUS_CHANNEL_CLOSE)(struct hv_device *Device);
typedef int (*VMBUS_CHANNEL_SEND_PACKET)(struct hv_device *Device,
					 const void *Buffer,
					 u32 BufferLen,
					 u64 RequestId,
					 u32 Type,
					 u32 Flags);
typedef int (*VMBUS_CHANNEL_SEND_PACKET_PAGEBUFFER)(struct hv_device *Device,
					struct hv_page_buffer PageBuffers[],
					u32 PageCount,
					void *Buffer,
					u32 BufferLen,
					u64 RequestId);
typedef int (*VMBUS_CHANNEL_SEND_PACKET_MULTIPAGEBUFFER)
					(struct hv_device *Device,
					 struct hv_multipage_buffer *mpb,
					 void *Buffer,
					 u32 BufferLen,
					 u64 RequestId);
typedef int (*VMBUS_CHANNEL_RECV_PACKET)(struct hv_device *Device,
					 void *Buffer,
					 u32 BufferLen,
					 u32 *BufferActualLen,
					 u64 *RequestId);
typedef int(*VMBUS_CHANNEL_RECV_PACKET_PAW)(struct hv_device *Device,
					    void *Buffer,
					    u32 BufferLen,
					    u32 *BufferActualLen,
					    u64 *RequestId);
typedef int (*VMBUS_CHANNEL_ESTABLISH_GPADL)(struct hv_device *Device,
					     void *Buffer,
					     u32 BufferLen,
					     u32 *GpadlHandle);
typedef int (*VMBUS_CHANNEL_TEARDOWN_GPADL)(struct hv_device *Device,
					    u32 GpadlHandle);


struct hv_dev_port_info {
	u32 InterruptMask;
	u32 ReadIndex;
	u32 WriteIndex;
	u32 BytesAvailToRead;
	u32 BytesAvailToWrite;
};

struct hv_device_info {
	u32 ChannelId;
	u32 ChannelState;
	struct hv_guid ChannelType;
	struct hv_guid ChannelInstance;

	u32 MonitorId;
	u32 ServerMonitorPending;
	u32 ServerMonitorLatency;
	u32 ServerMonitorConnectionId;
	u32 ClientMonitorPending;
	u32 ClientMonitorLatency;
	u32 ClientMonitorConnectionId;

	struct hv_dev_port_info Inbound;
	struct hv_dev_port_info Outbound;
};

typedef void (*VMBUS_GET_CHANNEL_INFO)(struct hv_device *Device,
				       struct hv_device_info *DeviceInfo);

struct vmbus_channel_interface {
	VMBUS_CHANNEL_OPEN Open;
	VMBUS_CHANNEL_CLOSE Close;
	VMBUS_CHANNEL_SEND_PACKET SendPacket;
	VMBUS_CHANNEL_SEND_PACKET_PAGEBUFFER SendPacketPageBuffer;
	VMBUS_CHANNEL_SEND_PACKET_MULTIPAGEBUFFER SendPacketMultiPageBuffer;
	VMBUS_CHANNEL_RECV_PACKET RecvPacket;
	VMBUS_CHANNEL_RECV_PACKET_PAW RecvPacketRaw;
	VMBUS_CHANNEL_ESTABLISH_GPADL EstablishGpadl;
	VMBUS_CHANNEL_TEARDOWN_GPADL TeardownGpadl;
	VMBUS_GET_CHANNEL_INFO GetInfo;
};

typedef void (*VMBUS_GET_CHANNEL_INTERFACE)(struct vmbus_channel_interface *i);

/* Base driver object */
struct hv_driver {
	const char *name;

	/* the device type supported by this driver */
	struct hv_guid deviceType;

	PFN_ON_DEVICEADD OnDeviceAdd;
	PFN_ON_DEVICEREMOVE OnDeviceRemove;

	/* device ids supported by this driver */
	PFN_ON_GETDEVICEIDS OnGetDeviceIds;
	PFN_ON_CLEANUP OnCleanup;

	struct vmbus_channel_interface VmbusChannelInterface;
};

/* Base device object */
struct hv_device {
	/* the driver for this device */
	struct hv_driver *Driver;

	char name[64];

	/* the device type id of this device */
	struct hv_guid deviceType;

	/* the device instance id of this device */
	struct hv_guid deviceInstance;

	void *context;

	/* Device extension; */
	void *Extension;
};

/* Vmbus driver object */
struct vmbus_driver {
	/* !! Must be the 1st field !! */
	/* FIXME if ^, then someone is doing somthing stupid */
	struct hv_driver Base;

	/* Set by the caller */
	PFN_ON_CHILDDEVICE_CREATE OnChildDeviceCreate;
	PFN_ON_CHILDDEVICE_DESTROY OnChildDeviceDestroy;
	PFN_ON_CHILDDEVICE_ADD OnChildDeviceAdd;
	PFN_ON_CHILDDEVICE_REMOVE OnChildDeviceRemove;

	/* Set by the callee */
	PFN_ON_ISR OnIsr;
	PFN_ON_DPC OnMsgDpc;
	PFN_ON_DPC OnEventDpc;
	PFN_GET_CHANNEL_OFFERS GetChannelOffers;

	VMBUS_GET_CHANNEL_INTERFACE GetChannelInterface;
	VMBUS_GET_CHANNEL_INFO GetChannelInfo;
};

int VmbusInitialize(struct hv_driver *drv);

#endif /* _VMBUS_API_H_ */
