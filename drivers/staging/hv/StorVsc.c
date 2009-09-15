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

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include "include/logging.h"

#include "include/StorVscApi.h"
#include "include/VmbusPacketFormat.h"
#include "include/vstorage.h"



/* #defines */



/* Data types */


typedef struct _STORVSC_REQUEST_EXTENSION {
	/* LIST_ENTRY						ListEntry; */

	STORVSC_REQUEST					*Request;
	DEVICE_OBJECT					*Device;

	/* Synchronize the request/response if needed */
	HANDLE							WaitEvent;

	VSTOR_PACKET					VStorPacket;
} STORVSC_REQUEST_EXTENSION;


/* A storvsc device is a device object that contains a vmbus channel */
typedef struct _STORVSC_DEVICE{
	DEVICE_OBJECT				*Device;

	int							RefCount; /* 0 indicates the device is being destroyed */

	int							NumOutstandingRequests;

	/*
	 * Each unique Port/Path/Target represents 1 channel ie scsi
	 * controller. In reality, the pathid, targetid is always 0
	 * and the port is set by us
	 */
	unsigned int						PortNumber;
    unsigned char						PathId;
    unsigned char						TargetId;

	/* LIST_ENTRY					OutstandingRequestList; */
	/* HANDLE						OutstandingRequestLock; */

	/* Used for vsc/vsp channel reset process */
	STORVSC_REQUEST_EXTENSION	InitRequest;

	STORVSC_REQUEST_EXTENSION	ResetRequest;

} STORVSC_DEVICE;



/* Globals */

static const char* gDriverName="storvsc";

/* {ba6163d9-04a1-4d29-b605-72e2ffb1dc7f} */
static const GUID gStorVscDeviceType={
	.Data = {0xd9, 0x63, 0x61, 0xba, 0xa1, 0x04, 0x29, 0x4d, 0xb6, 0x05, 0x72, 0xe2, 0xff, 0xb1, 0xdc, 0x7f}
};


/* Internal routines */

static int
StorVscOnDeviceAdd(
	DEVICE_OBJECT	*Device,
	void			*AdditionalInfo
	);

static int
StorVscOnDeviceRemove(
	DEVICE_OBJECT	*Device
	);

static int
StorVscOnIORequest(
	DEVICE_OBJECT	*Device,
	STORVSC_REQUEST	*Request
	);

static int
StorVscOnHostReset(
	DEVICE_OBJECT	*Device
	);

static void
StorVscOnCleanup(
	DRIVER_OBJECT	*Device
	);

static void
StorVscOnChannelCallback(
	void * Context
	);

static void
StorVscOnIOCompletion(
	DEVICE_OBJECT	*Device,
	VSTOR_PACKET	*VStorPacket,
	STORVSC_REQUEST_EXTENSION *RequestExt
	);

static void
StorVscOnReceive(
	DEVICE_OBJECT	*Device,
	VSTOR_PACKET	*VStorPacket,
	STORVSC_REQUEST_EXTENSION *RequestExt
	);

static int
StorVscConnectToVsp(
	DEVICE_OBJECT	*Device
	);

static inline STORVSC_DEVICE* AllocStorDevice(DEVICE_OBJECT *Device)
{
	STORVSC_DEVICE *storDevice;

	storDevice = kzalloc(sizeof(STORVSC_DEVICE), GFP_KERNEL);
	if (!storDevice)
		return NULL;

	/* Set to 2 to allow both inbound and outbound traffics */
	/* (ie GetStorDevice() and MustGetStorDevice()) to proceed. */
	InterlockedCompareExchange(&storDevice->RefCount, 2, 0);

	storDevice->Device = Device;
	Device->Extension = storDevice;

	return storDevice;
}

static inline void FreeStorDevice(STORVSC_DEVICE *Device)
{
	ASSERT(Device->RefCount == 0);
	kfree(Device);
}

/* Get the stordevice object iff exists and its refcount > 1 */
static inline STORVSC_DEVICE* GetStorDevice(DEVICE_OBJECT *Device)
{
	STORVSC_DEVICE *storDevice;

	storDevice = (STORVSC_DEVICE*)Device->Extension;
	if (storDevice && storDevice->RefCount > 1)
	{
		InterlockedIncrement(&storDevice->RefCount);
	}
	else
	{
		storDevice = NULL;
	}

	return storDevice;
}

/* Get the stordevice object iff exists and its refcount > 0 */
static inline STORVSC_DEVICE* MustGetStorDevice(DEVICE_OBJECT *Device)
{
	STORVSC_DEVICE *storDevice;

	storDevice = (STORVSC_DEVICE*)Device->Extension;
	if (storDevice && storDevice->RefCount)
	{
		InterlockedIncrement(&storDevice->RefCount);
	}
	else
	{
		storDevice = NULL;
	}

	return storDevice;
}

static inline void PutStorDevice(DEVICE_OBJECT *Device)
{
	STORVSC_DEVICE *storDevice;

	storDevice = (STORVSC_DEVICE*)Device->Extension;
	ASSERT(storDevice);

	InterlockedDecrement(&storDevice->RefCount);
	ASSERT(storDevice->RefCount);
}

/* Drop ref count to 1 to effectively disable GetStorDevice() */
static inline STORVSC_DEVICE* ReleaseStorDevice(DEVICE_OBJECT *Device)
{
	STORVSC_DEVICE *storDevice;

	storDevice = (STORVSC_DEVICE*)Device->Extension;
	ASSERT(storDevice);

	/* Busy wait until the ref drop to 2, then set it to 1 */
	while (InterlockedCompareExchange(&storDevice->RefCount, 1, 2) != 2)
	{
		udelay(100);
	}

	return storDevice;
}

/* Drop ref count to 0. No one can use StorDevice object. */
static inline STORVSC_DEVICE* FinalReleaseStorDevice(DEVICE_OBJECT *Device)
{
	STORVSC_DEVICE *storDevice;

	storDevice = (STORVSC_DEVICE*)Device->Extension;
	ASSERT(storDevice);

	/* Busy wait until the ref drop to 1, then set it to 0 */
	while (InterlockedCompareExchange(&storDevice->RefCount, 0, 1) != 1)
	{
		udelay(100);
	}

	Device->Extension = NULL;
	return storDevice;
}

/*++;


Name:
	StorVscInitialize()

Description:
	Main entry point

--*/
int
StorVscInitialize(
	DRIVER_OBJECT *Driver
	)
{
	STORVSC_DRIVER_OBJECT* storDriver = (STORVSC_DRIVER_OBJECT*)Driver;
	int ret=0;

	DPRINT_ENTER(STORVSC);

	DPRINT_DBG(STORVSC, "sizeof(STORVSC_REQUEST)=%d sizeof(STORVSC_REQUEST_EXTENSION)=%d sizeof(VSTOR_PACKET)=%d, sizeof(VMSCSI_REQUEST)=%d",
		sizeof(STORVSC_REQUEST), sizeof(STORVSC_REQUEST_EXTENSION), sizeof(VSTOR_PACKET), sizeof(VMSCSI_REQUEST));

	/* Make sure we are at least 2 pages since 1 page is used for control */
	ASSERT(storDriver->RingBufferSize >= (PAGE_SIZE << 1));

	Driver->name = gDriverName;
	memcpy(&Driver->deviceType, &gStorVscDeviceType, sizeof(GUID));

	storDriver->RequestExtSize			= sizeof(STORVSC_REQUEST_EXTENSION);

	/*
	 * Divide the ring buffer data size (which is 1 page less
	 * than the ring buffer size since that page is reserved for
	 * the ring buffer indices) by the max request size (which is
	 * VMBUS_CHANNEL_PACKET_MULITPAGE_BUFFER + VSTOR_PACKET + u64)
	 */
	storDriver->MaxOutstandingRequestsPerChannel =
		((storDriver->RingBufferSize - PAGE_SIZE) / ALIGN_UP(MAX_MULTIPAGE_BUFFER_PACKET + sizeof(VSTOR_PACKET) + sizeof(u64),sizeof(u64)));

	DPRINT_INFO(STORVSC, "max io %u, currently %u\n", storDriver->MaxOutstandingRequestsPerChannel, STORVSC_MAX_IO_REQUESTS);

	/* Setup the dispatch table */
	storDriver->Base.OnDeviceAdd			= StorVscOnDeviceAdd;
	storDriver->Base.OnDeviceRemove		= StorVscOnDeviceRemove;
	storDriver->Base.OnCleanup			= StorVscOnCleanup;

	storDriver->OnIORequest				= StorVscOnIORequest;
	storDriver->OnHostReset				= StorVscOnHostReset;

	DPRINT_EXIT(STORVSC);

	return ret;
}

/*++

Name:
	StorVscOnDeviceAdd()

Description:
	Callback when the device belonging to this driver is added

--*/
int
StorVscOnDeviceAdd(
	DEVICE_OBJECT	*Device,
	void			*AdditionalInfo
	)
{
	int ret=0;
	STORVSC_DEVICE *storDevice;
	/* VMSTORAGE_CHANNEL_PROPERTIES *props; */
	STORVSC_DEVICE_INFO *deviceInfo = (STORVSC_DEVICE_INFO*)AdditionalInfo;

	DPRINT_ENTER(STORVSC);

	storDevice = AllocStorDevice(Device);
	if (!storDevice)
	{
		ret = -1;
		goto Cleanup;
	}

	/* Save the channel properties to our storvsc channel */
	/* props = (VMSTORAGE_CHANNEL_PROPERTIES*) channel->offerMsg.Offer.u.Standard.UserDefined; */

	/* FIXME: */
	/*
	 * If we support more than 1 scsi channel, we need to set the
	 * port number here to the scsi channel but how do we get the
	 * scsi channel prior to the bus scan
	 */

	/* storChannel->PortNumber = 0;
	storChannel->PathId = props->PathId;
	storChannel->TargetId = props->TargetId; */

	storDevice->PortNumber = deviceInfo->PortNumber;
	/* Send it back up */
	ret = StorVscConnectToVsp(Device);

	/* deviceInfo->PortNumber = storDevice->PortNumber; */
	deviceInfo->PathId = storDevice->PathId;
	deviceInfo->TargetId = storDevice->TargetId;

	DPRINT_DBG(STORVSC, "assigned port %lu, path %u target %u\n", storDevice->PortNumber, storDevice->PathId, storDevice->TargetId);

Cleanup:
	DPRINT_EXIT(STORVSC);

	return ret;
}

static int StorVscChannelInit(DEVICE_OBJECT *Device)
{
	int ret=0;
	STORVSC_DEVICE *storDevice;
	STORVSC_REQUEST_EXTENSION *request;
	VSTOR_PACKET *vstorPacket;

	storDevice = GetStorDevice(Device);
	if (!storDevice)
	{
		DPRINT_ERR(STORVSC, "unable to get stor device...device being destroyed?");
		DPRINT_EXIT(STORVSC);
		return -1;
	}

	request = &storDevice->InitRequest;
	vstorPacket = &request->VStorPacket;

	/* Now, initiate the vsc/vsp initialization protocol on the open channel */

	memset(request, sizeof(STORVSC_REQUEST_EXTENSION), 0);
	request->WaitEvent = WaitEventCreate();

	vstorPacket->Operation = VStorOperationBeginInitialization;
	vstorPacket->Flags = REQUEST_COMPLETION_FLAG;

	/*SpinlockAcquire(gDriverExt.packetListLock);
	INSERT_TAIL_LIST(&gDriverExt.packetList, &packet->listEntry.entry);
	SpinlockRelease(gDriverExt.packetListLock);*/

	DPRINT_INFO(STORVSC, "BEGIN_INITIALIZATION_OPERATION...");

	ret = Device->Driver->VmbusChannelInterface.SendPacket(Device,
															vstorPacket,
															sizeof(VSTOR_PACKET),
															(unsigned long)request,
															VmbusPacketTypeDataInBand,
															VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);
	if ( ret != 0)
	{
		DPRINT_ERR(STORVSC, "unable to send BEGIN_INITIALIZATION_OPERATION");
		goto Cleanup;
	}

	WaitEventWait(request->WaitEvent);

	if (vstorPacket->Operation != VStorOperationCompleteIo || vstorPacket->Status != 0)
	{
		DPRINT_ERR(STORVSC, "BEGIN_INITIALIZATION_OPERATION failed (op %d status 0x%lx)", vstorPacket->Operation, vstorPacket->Status);
		goto Cleanup;
	}

	DPRINT_INFO(STORVSC, "QUERY_PROTOCOL_VERSION_OPERATION...");

	/* reuse the packet for version range supported */
	memset(vstorPacket, sizeof(VSTOR_PACKET), 0);
	vstorPacket->Operation = VStorOperationQueryProtocolVersion;
	vstorPacket->Flags = REQUEST_COMPLETION_FLAG;

    vstorPacket->Version.MajorMinor = VMSTOR_PROTOCOL_VERSION_CURRENT;
    FILL_VMSTOR_REVISION(vstorPacket->Version.Revision);

	ret = Device->Driver->VmbusChannelInterface.SendPacket(Device,
															vstorPacket,
															sizeof(VSTOR_PACKET),
															(unsigned long)request,
															VmbusPacketTypeDataInBand,
															VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);
	if ( ret != 0)
	{
		DPRINT_ERR(STORVSC, "unable to send BEGIN_INITIALIZATION_OPERATION");
		goto Cleanup;
	}

	WaitEventWait(request->WaitEvent);

	/* TODO: Check returned version */
	if (vstorPacket->Operation != VStorOperationCompleteIo || vstorPacket->Status != 0)
	{
		DPRINT_ERR(STORVSC, "QUERY_PROTOCOL_VERSION_OPERATION failed (op %d status 0x%lx)", vstorPacket->Operation, vstorPacket->Status);
		goto Cleanup;
	}

	/* Query channel properties */
	DPRINT_INFO(STORVSC, "QUERY_PROPERTIES_OPERATION...");

	memset(vstorPacket, sizeof(VSTOR_PACKET), 0);
    vstorPacket->Operation = VStorOperationQueryProperties;
	vstorPacket->Flags = REQUEST_COMPLETION_FLAG;
    vstorPacket->StorageChannelProperties.PortNumber = storDevice->PortNumber;

	ret = Device->Driver->VmbusChannelInterface.SendPacket(Device,
															vstorPacket,
															sizeof(VSTOR_PACKET),
															(unsigned long)request,
															VmbusPacketTypeDataInBand,
															VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);

	if ( ret != 0)
	{
		DPRINT_ERR(STORVSC, "unable to send QUERY_PROPERTIES_OPERATION");
		goto Cleanup;
	}

	WaitEventWait(request->WaitEvent);

	/* TODO: Check returned version */
	if (vstorPacket->Operation != VStorOperationCompleteIo || vstorPacket->Status != 0)
	{
		DPRINT_ERR(STORVSC, "QUERY_PROPERTIES_OPERATION failed (op %d status 0x%lx)", vstorPacket->Operation, vstorPacket->Status);
		goto Cleanup;
	}

	/* storDevice->PortNumber = vstorPacket->StorageChannelProperties.PortNumber; */
	storDevice->PathId = vstorPacket->StorageChannelProperties.PathId;
	storDevice->TargetId = vstorPacket->StorageChannelProperties.TargetId;

	DPRINT_DBG(STORVSC, "channel flag 0x%lx, max xfer len 0x%lx", vstorPacket->StorageChannelProperties.Flags, vstorPacket->StorageChannelProperties.MaxTransferBytes);

	DPRINT_INFO(STORVSC, "END_INITIALIZATION_OPERATION...");

	memset(vstorPacket, sizeof(VSTOR_PACKET), 0);
    vstorPacket->Operation = VStorOperationEndInitialization;
	vstorPacket->Flags = REQUEST_COMPLETION_FLAG;

	ret = Device->Driver->VmbusChannelInterface.SendPacket(Device,
															vstorPacket,
															sizeof(VSTOR_PACKET),
															(unsigned long)request,
															VmbusPacketTypeDataInBand,
															VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);

	if ( ret != 0)
	{
		DPRINT_ERR(STORVSC, "unable to send END_INITIALIZATION_OPERATION");
		goto Cleanup;
	}

	WaitEventWait(request->WaitEvent);

	if (vstorPacket->Operation != VStorOperationCompleteIo || vstorPacket->Status != 0)
	{
		DPRINT_ERR(STORVSC, "END_INITIALIZATION_OPERATION failed (op %d status 0x%lx)", vstorPacket->Operation, vstorPacket->Status);
		goto Cleanup;
	}

	DPRINT_INFO(STORVSC, "**** storage channel up and running!! ****");

Cleanup:
	if (request->WaitEvent)
	{
		WaitEventClose(request->WaitEvent);
		request->WaitEvent = NULL;
	}

	PutStorDevice(Device);

	DPRINT_EXIT(STORVSC);
	return ret;
}


int
StorVscConnectToVsp(
	DEVICE_OBJECT	*Device
	)
{
	int ret=0;
    VMSTORAGE_CHANNEL_PROPERTIES props;

	STORVSC_DRIVER_OBJECT *storDriver = (STORVSC_DRIVER_OBJECT*) Device->Driver;;

	memset(&props, sizeof(VMSTORAGE_CHANNEL_PROPERTIES), 0);

	/* Open the channel */
	ret = Device->Driver->VmbusChannelInterface.Open(Device,
		storDriver->RingBufferSize,
		storDriver->RingBufferSize,
		(void *)&props,
		sizeof(VMSTORAGE_CHANNEL_PROPERTIES),
		StorVscOnChannelCallback,
		Device
		);

	DPRINT_DBG(STORVSC, "storage props: path id %d, tgt id %d, max xfer %ld", props.PathId, props.TargetId, props.MaxTransferBytes);

	if (ret != 0)
	{
		DPRINT_ERR(STORVSC, "unable to open channel: %d", ret);
		return -1;
	}

	ret = StorVscChannelInit(Device);

	return ret;
}


/*++

Name:
	StorVscOnDeviceRemove()

Description:
	Callback when the our device is being removed

--*/
int
StorVscOnDeviceRemove(
	DEVICE_OBJECT *Device
	)
{
	STORVSC_DEVICE *storDevice;
	int ret=0;

	DPRINT_ENTER(STORVSC);

	DPRINT_INFO(STORVSC, "disabling storage device (%p)...", Device->Extension);

	storDevice = ReleaseStorDevice(Device);

	/*
	 * At this point, all outbound traffic should be disable. We
	 * only allow inbound traffic (responses) to proceed so that
	 * outstanding requests can be completed.
	 */
	while (storDevice->NumOutstandingRequests)
	{
		DPRINT_INFO(STORVSC, "waiting for %d requests to complete...", storDevice->NumOutstandingRequests);

		udelay(100);
	}

	DPRINT_INFO(STORVSC, "removing storage device (%p)...", Device->Extension);

	storDevice = FinalReleaseStorDevice(Device);

	DPRINT_INFO(STORVSC, "storage device (%p) safe to remove", storDevice);

	/* Close the channel */
	Device->Driver->VmbusChannelInterface.Close(Device);

	FreeStorDevice(storDevice);

	DPRINT_EXIT(STORVSC);
	return ret;
}

/* ***************
static void
StorVscOnTargetRescan(
void *Context
)
{
DEVICE_OBJECT *device=(DEVICE_OBJECT*)Context;
STORVSC_DRIVER_OBJECT *storDriver;

DPRINT_ENTER(STORVSC);

storDriver = (STORVSC_DRIVER_OBJECT*) device->Driver;
storDriver->OnHostRescan(device);

DPRINT_EXIT(STORVSC);
}
*********** */

int
StorVscOnHostReset(
	DEVICE_OBJECT *Device
	)
{
	int ret=0;

	STORVSC_DEVICE *storDevice;
	STORVSC_REQUEST_EXTENSION *request;
	VSTOR_PACKET *vstorPacket;

	DPRINT_ENTER(STORVSC);

	DPRINT_INFO(STORVSC, "resetting host adapter...");

	storDevice = GetStorDevice(Device);
	if (!storDevice)
	{
		DPRINT_ERR(STORVSC, "unable to get stor device...device being destroyed?");
		DPRINT_EXIT(STORVSC);
		return -1;
	}

	request = &storDevice->ResetRequest;
	vstorPacket = &request->VStorPacket;

	request->WaitEvent = WaitEventCreate();

    vstorPacket->Operation = VStorOperationResetBus;
    vstorPacket->Flags = REQUEST_COMPLETION_FLAG;
    vstorPacket->VmSrb.PathId = storDevice->PathId;

	ret = Device->Driver->VmbusChannelInterface.SendPacket(Device,
															vstorPacket,
															sizeof(VSTOR_PACKET),
															(unsigned long)&storDevice->ResetRequest,
															VmbusPacketTypeDataInBand,
															VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);
	if (ret != 0)
	{
		DPRINT_ERR(STORVSC, "Unable to send reset packet %p ret %d", vstorPacket, ret);
		goto Cleanup;
	}

	/* FIXME: Add a timeout */
	WaitEventWait(request->WaitEvent);

	WaitEventClose(request->WaitEvent);
	DPRINT_INFO(STORVSC, "host adapter reset completed");

	/*
	 * At this point, all outstanding requests in the adapter
	 * should have been flushed out and return to us
	 */

Cleanup:
	PutStorDevice(Device);
	DPRINT_EXIT(STORVSC);
	return ret;
}

/*++

Name:
	StorVscOnIORequest()

Description:
	Callback to initiate an I/O request

--*/
int
StorVscOnIORequest(
	DEVICE_OBJECT	*Device,
	STORVSC_REQUEST	*Request
	)
{
	STORVSC_DEVICE *storDevice;
	STORVSC_REQUEST_EXTENSION* requestExtension = (STORVSC_REQUEST_EXTENSION*) Request->Extension;
	VSTOR_PACKET* vstorPacket =&requestExtension->VStorPacket;
	int ret=0;

	DPRINT_ENTER(STORVSC);

	storDevice = GetStorDevice(Device);

	DPRINT_DBG(STORVSC, "enter - Device %p, DeviceExt %p, Request %p, Extension %p",
		Device, storDevice, Request, requestExtension);

	DPRINT_DBG(STORVSC, "req %p len %d bus %d, target %d, lun %d cdblen %d",
		Request, Request->DataBuffer.Length, Request->Bus, Request->TargetId, Request->LunId, Request->CdbLen);

	if (!storDevice)
	{
		DPRINT_ERR(STORVSC, "unable to get stor device...device being destroyed?");
		DPRINT_EXIT(STORVSC);
		return -2;
	}

	/* print_hex_dump_bytes("", DUMP_PREFIX_NONE, Request->Cdb, Request->CdbLen); */

	requestExtension->Request = Request;
	requestExtension->Device  = Device;

	memset(vstorPacket, 0 , sizeof(VSTOR_PACKET));

	vstorPacket->Flags |= REQUEST_COMPLETION_FLAG;

    vstorPacket->VmSrb.Length = sizeof(VMSCSI_REQUEST);

	vstorPacket->VmSrb.PortNumber = Request->Host;
    vstorPacket->VmSrb.PathId = Request->Bus;
    vstorPacket->VmSrb.TargetId = Request->TargetId;
    vstorPacket->VmSrb.Lun = Request->LunId;

	vstorPacket->VmSrb.SenseInfoLength = SENSE_BUFFER_SIZE;

	/* Copy over the scsi command descriptor block */
    vstorPacket->VmSrb.CdbLength = Request->CdbLen;
	memcpy(&vstorPacket->VmSrb.Cdb, Request->Cdb, Request->CdbLen);

	vstorPacket->VmSrb.DataIn = Request->Type;
	vstorPacket->VmSrb.DataTransferLength = Request->DataBuffer.Length;

	vstorPacket->Operation = VStorOperationExecuteSRB;

	DPRINT_DBG(STORVSC, "srb - len %d port %d, path %d, target %d, lun %d senselen %d cdblen %d",
		vstorPacket->VmSrb.Length,
		vstorPacket->VmSrb.PortNumber,
		vstorPacket->VmSrb.PathId,
		vstorPacket->VmSrb.TargetId,
		vstorPacket->VmSrb.Lun,
		vstorPacket->VmSrb.SenseInfoLength,
		vstorPacket->VmSrb.CdbLength);

	if (requestExtension->Request->DataBuffer.Length)
	{
		ret = Device->Driver->VmbusChannelInterface.SendPacketMultiPageBuffer(Device,
				&requestExtension->Request->DataBuffer,
				vstorPacket,
				sizeof(VSTOR_PACKET),
				(unsigned long)requestExtension);
	}
	else
	{
		ret = Device->Driver->VmbusChannelInterface.SendPacket(Device,
															vstorPacket,
															sizeof(VSTOR_PACKET),
															(unsigned long)requestExtension,
															VmbusPacketTypeDataInBand,
															VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);
	}

	if (ret != 0)
	{
		DPRINT_DBG(STORVSC, "Unable to send packet %p ret %d", vstorPacket, ret);
	}

	InterlockedIncrement(&storDevice->NumOutstandingRequests);

	PutStorDevice(Device);

	DPRINT_EXIT(STORVSC);
	return ret;
}

/*++

Name:
	StorVscOnCleanup()

Description:
	Perform any cleanup when the driver is removed

--*/
void
StorVscOnCleanup(
	DRIVER_OBJECT *Driver
	)
{
	DPRINT_ENTER(STORVSC);
	DPRINT_EXIT(STORVSC);
}


static void
StorVscOnIOCompletion(
	DEVICE_OBJECT	*Device,
	VSTOR_PACKET	*VStorPacket,
	STORVSC_REQUEST_EXTENSION *RequestExt
	)
{
	STORVSC_REQUEST *request;
	STORVSC_DEVICE *storDevice;

	DPRINT_ENTER(STORVSC);

	storDevice = MustGetStorDevice(Device);
	if (!storDevice)
	{
		DPRINT_ERR(STORVSC, "unable to get stor device...device being destroyed?");
		DPRINT_EXIT(STORVSC);
		return;
	}

	DPRINT_DBG(STORVSC, "IO_COMPLETE_OPERATION - request extension %p completed bytes xfer %lu",
		RequestExt, VStorPacket->VmSrb.DataTransferLength);

	ASSERT(RequestExt != NULL);
	ASSERT(RequestExt->Request != NULL);

	request = RequestExt->Request;

	ASSERT(request->OnIOCompletion != NULL);

	/* Copy over the status...etc */
	request->Status = VStorPacket->VmSrb.ScsiStatus;

	if (request->Status != 0 || VStorPacket->VmSrb.SrbStatus != 1)
	{
		DPRINT_WARN(STORVSC, "cmd 0x%x scsi status 0x%x srb status 0x%x\n",
			request->Cdb[0],
			VStorPacket->VmSrb.ScsiStatus,
			VStorPacket->VmSrb.SrbStatus);
	}

	if ((request->Status & 0xFF) == 0x02) /* CHECK_CONDITION */
	{
		if (VStorPacket->VmSrb.SrbStatus & 0x80) /* autosense data available */
		{
			DPRINT_WARN(STORVSC, "storvsc pkt %p autosense data valid - len %d\n",
				RequestExt, VStorPacket->VmSrb.SenseInfoLength);

			ASSERT(VStorPacket->VmSrb.SenseInfoLength <=  request->SenseBufferSize);
			memcpy(request->SenseBuffer,
				VStorPacket->VmSrb.SenseData,
				VStorPacket->VmSrb.SenseInfoLength);

			request->SenseBufferSize = VStorPacket->VmSrb.SenseInfoLength;
		}
	}

	/* TODO: */
	request->BytesXfer = VStorPacket->VmSrb.DataTransferLength;

	request->OnIOCompletion(request);

	InterlockedDecrement(&storDevice->NumOutstandingRequests);

	PutStorDevice(Device);

	DPRINT_EXIT(STORVSC);
}


static void
StorVscOnReceive(
	DEVICE_OBJECT	*Device,
	VSTOR_PACKET	*VStorPacket,
	STORVSC_REQUEST_EXTENSION *RequestExt
	)
{
	switch(VStorPacket->Operation)
	{
		case VStorOperationCompleteIo:

			DPRINT_DBG(STORVSC, "IO_COMPLETE_OPERATION");
			StorVscOnIOCompletion(Device, VStorPacket, RequestExt);
			break;

		/* case ENUMERATE_DEVICE_OPERATION: */

		/* DPRINT_INFO(STORVSC, "ENUMERATE_DEVICE_OPERATION"); */

		/* StorVscOnTargetRescan(Device); */
		/* break; */

	case VStorOperationRemoveDevice:

			DPRINT_INFO(STORVSC, "REMOVE_DEVICE_OPERATION");
			/* TODO: */
			break;

		default:
			DPRINT_INFO(STORVSC, "Unknown operation received - %d", VStorPacket->Operation);
			break;
	}
}

void
StorVscOnChannelCallback(
	void * Context
	)
{
	int ret=0;
	DEVICE_OBJECT *device = (DEVICE_OBJECT*)Context;
	STORVSC_DEVICE *storDevice;
	u32 bytesRecvd;
	u64 requestId;
	unsigned char packet[ALIGN_UP(sizeof(VSTOR_PACKET),8)];
	STORVSC_REQUEST_EXTENSION *request;

	DPRINT_ENTER(STORVSC);

	ASSERT(device);

	storDevice = MustGetStorDevice(device);
	if (!storDevice)
	{
		DPRINT_ERR(STORVSC, "unable to get stor device...device being destroyed?");
		DPRINT_EXIT(STORVSC);
		return;
	}

	do
	{
		ret = device->Driver->VmbusChannelInterface.RecvPacket(device,
																packet,
																ALIGN_UP(sizeof(VSTOR_PACKET),8),
																&bytesRecvd,
																&requestId);
		if (ret == 0 && bytesRecvd > 0)
		{
			DPRINT_DBG(STORVSC, "receive %d bytes - tid %llx", bytesRecvd, requestId);

			/* ASSERT(bytesRecvd == sizeof(VSTOR_PACKET)); */

			request = (STORVSC_REQUEST_EXTENSION*)(unsigned long)requestId;
			ASSERT(request);

			/* if (vstorPacket.Flags & SYNTHETIC_FLAG) */
			if ((request == &storDevice->InitRequest) || (request == &storDevice->ResetRequest))
			{
				/* DPRINT_INFO(STORVSC, "reset completion - operation %u status %u", vstorPacket.Operation, vstorPacket.Status); */

				memcpy(&request->VStorPacket, packet, sizeof(VSTOR_PACKET));

				WaitEventSet(request->WaitEvent);
			}
			else
			{
				StorVscOnReceive(device, (VSTOR_PACKET*)packet, request);
			}
		}
		else
		{
			/* DPRINT_DBG(STORVSC, "nothing else to read..."); */
			break;
		}
	} while (1);

	PutStorDevice(device);

	DPRINT_EXIT(STORVSC);
	return;
}
