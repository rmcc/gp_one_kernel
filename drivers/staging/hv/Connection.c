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


#include "include/logging.h"

#include "VmbusPrivate.h"

/* Globals */


struct VMBUS_CONNECTION gVmbusConnection = {
	.ConnectState		= Disconnected,
	.NextGpadlHandle	= 0xE1E10,
};


/*++

Name:
	VmbusConnect()

Description:
	Sends a connect request on the partition service connection

--*/
int
VmbusConnect(
	)
{
	int ret=0;
	VMBUS_CHANNEL_MSGINFO *msgInfo=NULL;
	VMBUS_CHANNEL_INITIATE_CONTACT *msg;
	unsigned long flags;

	DPRINT_ENTER(VMBUS);

	/* Make sure we are not connecting or connected */
	if (gVmbusConnection.ConnectState != Disconnected)
		return -1;

	/* Initialize the vmbus connection */
	gVmbusConnection.ConnectState = Connecting;
	gVmbusConnection.WorkQueue = WorkQueueCreate("vmbusQ");

	INITIALIZE_LIST_HEAD(&gVmbusConnection.ChannelMsgList);
	spin_lock_init(&gVmbusConnection.channelmsg_lock);

	INITIALIZE_LIST_HEAD(&gVmbusConnection.ChannelList);
	spin_lock_init(&gVmbusConnection.channel_lock);

	/*
	 * Setup the vmbus event connection for channel interrupt
	 * abstraction stuff
	 */
	gVmbusConnection.InterruptPage = PageAlloc(1);
	if (gVmbusConnection.InterruptPage == NULL)
	{
		ret = -1;
		goto Cleanup;
	}

	gVmbusConnection.RecvInterruptPage = gVmbusConnection.InterruptPage;
	gVmbusConnection.SendInterruptPage = (void*)((unsigned long)gVmbusConnection.InterruptPage + (PAGE_SIZE >> 1));

	/* Setup the monitor
	 * notification facility. The 1st page for parent->child and
	 * the 2nd page for child->parent
	 */
	gVmbusConnection.MonitorPages = PageAlloc(2);
	if (gVmbusConnection.MonitorPages == NULL)
	{
		ret = -1;
		goto Cleanup;
	}

	msgInfo = kzalloc(sizeof(VMBUS_CHANNEL_MSGINFO) + sizeof(VMBUS_CHANNEL_INITIATE_CONTACT), GFP_KERNEL);
	if (msgInfo == NULL)
	{
		ret = -1;
		goto Cleanup;
	}

	msgInfo->WaitEvent = WaitEventCreate();
	msg = (VMBUS_CHANNEL_INITIATE_CONTACT*)msgInfo->Msg;

	msg->Header.MessageType = ChannelMessageInitiateContact;
	msg->VMBusVersionRequested = VMBUS_REVISION_NUMBER;
	msg->InterruptPage = GetPhysicalAddress(gVmbusConnection.InterruptPage);
	msg->MonitorPage1 = GetPhysicalAddress(gVmbusConnection.MonitorPages);
	msg->MonitorPage2 = GetPhysicalAddress((void *)((unsigned long)gVmbusConnection.MonitorPages + PAGE_SIZE));

	/*
	 * Add to list before we send the request since we may
	 * receive the response before returning from this routine
	 */
	spin_lock_irqsave(&gVmbusConnection.channelmsg_lock, flags);
	INSERT_TAIL_LIST(&gVmbusConnection.ChannelMsgList, &msgInfo->MsgListEntry);
	spin_unlock_irqrestore(&gVmbusConnection.channelmsg_lock, flags);

	DPRINT_DBG(VMBUS, "Vmbus connection -  interrupt pfn %llx, monitor1 pfn %llx,, monitor2 pfn %llx",
		msg->InterruptPage, msg->MonitorPage1, msg->MonitorPage2);

	DPRINT_DBG(VMBUS, "Sending channel initiate msg...");

	ret = VmbusPostMessage(msg, sizeof(VMBUS_CHANNEL_INITIATE_CONTACT));
	if (ret != 0)
	{
		REMOVE_ENTRY_LIST(&msgInfo->MsgListEntry);
		goto Cleanup;
	}

	/* Wait for the connection response */
	WaitEventWait(msgInfo->WaitEvent);

	REMOVE_ENTRY_LIST(&msgInfo->MsgListEntry);

	/* Check if successful */
	if (msgInfo->Response.VersionResponse.VersionSupported)
	{
		DPRINT_INFO(VMBUS, "Vmbus connected!!");
		gVmbusConnection.ConnectState = Connected;

	}
	else
	{
		DPRINT_ERR(VMBUS, "Vmbus connection failed!!...current version (%d) not supported", VMBUS_REVISION_NUMBER);
		ret = -1;

		goto Cleanup;
	}


	WaitEventClose(msgInfo->WaitEvent);
	kfree(msgInfo);
	DPRINT_EXIT(VMBUS);

	return 0;

Cleanup:

	gVmbusConnection.ConnectState = Disconnected;

	WorkQueueClose(gVmbusConnection.WorkQueue);

	if (gVmbusConnection.InterruptPage)
	{
		PageFree(gVmbusConnection.InterruptPage, 1);
		gVmbusConnection.InterruptPage = NULL;
	}

	if (gVmbusConnection.MonitorPages)
	{
		PageFree(gVmbusConnection.MonitorPages, 2);
		gVmbusConnection.MonitorPages = NULL;
	}

	if (msgInfo)
	{
		if (msgInfo->WaitEvent)
			WaitEventClose(msgInfo->WaitEvent);

		kfree(msgInfo);
	}

	DPRINT_EXIT(VMBUS);

	return ret;
}


/*++

Name:
	VmbusDisconnect()

Description:
	Sends a disconnect request on the partition service connection

--*/
int
VmbusDisconnect(
	void
	)
{
	int ret=0;
	VMBUS_CHANNEL_UNLOAD *msg;

	DPRINT_ENTER(VMBUS);

	/* Make sure we are connected */
	if (gVmbusConnection.ConnectState != Connected)
		return -1;

	msg = kzalloc(sizeof(VMBUS_CHANNEL_UNLOAD), GFP_KERNEL);

	msg->MessageType = ChannelMessageUnload;

	ret = VmbusPostMessage(msg, sizeof(VMBUS_CHANNEL_UNLOAD));

	if (ret != 0)
	{
		goto Cleanup;
	}

	PageFree(gVmbusConnection.InterruptPage, 1);

	/* TODO: iterate thru the msg list and free up */

	WorkQueueClose(gVmbusConnection.WorkQueue);

	gVmbusConnection.ConnectState = Disconnected;

	DPRINT_INFO(VMBUS, "Vmbus disconnected!!");

Cleanup:
	if (msg)
	{
		kfree(msg);
	}

	DPRINT_EXIT(VMBUS);

	return ret;
}


/*++

Name:
	GetChannelFromRelId()

Description:
	Get the channel object given its child relative id (ie channel id)

--*/
VMBUS_CHANNEL*
GetChannelFromRelId(
	u32 relId
	)
{
	VMBUS_CHANNEL* channel;
	VMBUS_CHANNEL* foundChannel=NULL;
	LIST_ENTRY* anchor;
	LIST_ENTRY* curr;
	unsigned long flags;

	spin_lock_irqsave(&gVmbusConnection.channel_lock, flags);
	ITERATE_LIST_ENTRIES(anchor, curr, &gVmbusConnection.ChannelList)
	{
		channel = CONTAINING_RECORD(curr, VMBUS_CHANNEL, ListEntry);

		if (channel->OfferMsg.ChildRelId == relId)
		{
			foundChannel = channel;
			break;
		}
	}
	spin_unlock_irqrestore(&gVmbusConnection.channel_lock, flags);

	return foundChannel;
}



/*++

Name:
	VmbusProcessChannelEvent()

Description:
	Process a channel event notification

--*/
static void
VmbusProcessChannelEvent(
	void * context
	)
{
	VMBUS_CHANNEL* channel;
	u32 relId = (u32)(unsigned long)context;

	ASSERT(relId > 0);

	/*
	 * Find the channel based on this relid and invokes the
	 * channel callback to process the event
	 */
	channel = GetChannelFromRelId(relId);

	if (channel)
	{
		VmbusChannelOnChannelEvent(channel);
		/* WorkQueueQueueWorkItem(channel->dataWorkQueue, VmbusChannelOnChannelEvent, (void*)channel); */
	}
	else
	{
	DPRINT_ERR(VMBUS, "channel not found for relid - %d.", relId);
	}
}


/*++

Name:
	VmbusOnEvents()

Description:
	Handler for events

--*/
void
VmbusOnEvents(
  void
	)
{
	int dword;
	/* int maxdword = PAGE_SIZE >> 3; // receive size is 1/2 page and divide that by 4 bytes */
	int maxdword = MAX_NUM_CHANNELS_SUPPORTED >> 5;
	int bit;
	int relid;
	u32* recvInterruptPage = gVmbusConnection.RecvInterruptPage;
	/* VMBUS_CHANNEL_MESSAGE* receiveMsg; */

	DPRINT_ENTER(VMBUS);

	/* Check events */
	if (recvInterruptPage)
	{
		for (dword = 0; dword < maxdword; dword++)
		{
			if (recvInterruptPage[dword])
			{
				for (bit = 0; bit < 32; bit++)
				{
					if (BitTestAndClear(&recvInterruptPage[dword], bit))
					{
						relid = (dword << 5) + bit;

						DPRINT_DBG(VMBUS, "event detected for relid - %d", relid);

						if (relid == 0) /* special case - vmbus channel protocol msg */
						{
							DPRINT_DBG(VMBUS, "invalid relid - %d", relid);

							continue;						}
						else
						{
							/* QueueWorkItem(VmbusProcessEvent, (void*)relid); */
							/* ret = WorkQueueQueueWorkItem(gVmbusConnection.workQueue, VmbusProcessChannelEvent, (void*)relid); */
							VmbusProcessChannelEvent((void*)(unsigned long)relid);
						}
					}
				}
			}
		 }
	}
	DPRINT_EXIT(VMBUS);

	return;
}

/*++

Name:
	VmbusPostMessage()

Description:
	Send a msg on the vmbus's message connection

--*/
int
VmbusPostMessage(
	void *			buffer,
	size_t			bufferLen
	)
{
	int ret=0;
	HV_CONNECTION_ID connId;


	connId.Asu32 =0;
	connId.u.Id = VMBUS_MESSAGE_CONNECTION_ID;
	ret = HvPostMessage(
			connId,
			1,
			buffer,
			bufferLen);

	return  ret;
}

/*++

Name:
	VmbusSetEvent()

Description:
	Send an event notification to the parent

--*/
int
VmbusSetEvent(u32 childRelId)
{
	int ret=0;

	DPRINT_ENTER(VMBUS);

	/* Each u32 represents 32 channels */
	BitSet((u32*)gVmbusConnection.SendInterruptPage + (childRelId >> 5), childRelId & 31);
	ret = HvSignalEvent();

	DPRINT_EXIT(VMBUS);

	return ret;
}

/* EOF */
