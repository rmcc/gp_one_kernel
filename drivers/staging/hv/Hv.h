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


#ifndef __HV_H__
#define __HV_H__

#include "include/osd.h"

#include "include/HvTypes.h"
#include "include/HvStatus.h"
//#include "HvVmApi.h"
//#include "HvKeApi.h"
//#include "HvMmApi.h"
//#include "HvCpuApi.h"
#include "include/HvHalApi.h"
#include "include/HvVpApi.h"
//#include "HvTrApi.h"
#include "include/HvSynicApi.h"
//#include "HvAmApi.h"
//#include "HvHkApi.h"
//#include "HvValApi.h"
#include "include/HvHcApi.h"
#include "include/HvPtApi.h"

enum
{
    VMBUS_MESSAGE_CONNECTION_ID = 1,
    VMBUS_MESSAGE_PORT_ID       = 1,
    VMBUS_EVENT_CONNECTION_ID   = 2,
    VMBUS_EVENT_PORT_ID         = 2,
    VMBUS_MONITOR_CONNECTION_ID = 3,
    VMBUS_MONITOR_PORT_ID       = 3,
    VMBUS_MESSAGE_SINT          = 2
};
//
// #defines
//
#define HV_PRESENT_BIT				0x80000000

#define HV_XENLINUX_GUEST_ID_LO     0x00000000
#define HV_XENLINUX_GUEST_ID_HI		0x0B00B135
#define HV_XENLINUX_GUEST_ID		(((u64)HV_XENLINUX_GUEST_ID_HI << 32) | HV_XENLINUX_GUEST_ID_LO)

#define HV_LINUX_GUEST_ID_LO		0x00000000
#define HV_LINUX_GUEST_ID_HI		0xB16B00B5
#define HV_LINUX_GUEST_ID			(((u64)HV_LINUX_GUEST_ID_HI << 32) | HV_LINUX_GUEST_ID_LO)

#define HV_CPU_POWER_MANAGEMENT     (1 << 0)
#define HV_RECOMMENDATIONS_MAX      4

#define HV_X64_MAX                  5
#define HV_CAPS_MAX                 8


#define HV_HYPERCALL_PARAM_ALIGN	sizeof(u64)

//
// Service definitions
//
#define HV_SERVICE_PARENT_PORT (0)
#define HV_SERVICE_PARENT_CONNECTION (0)

#define HV_SERVICE_CONNECT_RESPONSE_SUCCESS             (0)
#define HV_SERVICE_CONNECT_RESPONSE_INVALID_PARAMETER   (1)
#define HV_SERVICE_CONNECT_RESPONSE_UNKNOWN_SERVICE     (2)
#define HV_SERVICE_CONNECT_RESPONSE_CONNECTION_REJECTED (3)

#define HV_SERVICE_CONNECT_REQUEST_MESSAGE_ID		(1)
#define HV_SERVICE_CONNECT_RESPONSE_MESSAGE_ID		(2)
#define HV_SERVICE_DISCONNECT_REQUEST_MESSAGE_ID	(3)
#define HV_SERVICE_DISCONNECT_RESPONSE_MESSAGE_ID	(4)
#define HV_SERVICE_MAX_MESSAGE_ID					(4)

#define HV_SERVICE_PROTOCOL_VERSION (0x0010)
#define HV_CONNECT_PAYLOAD_BYTE_COUNT 64

//#define VMBUS_REVISION_NUMBER	6
//#define VMBUS_PORT_ID			11		// Our local vmbus's port and connection id. Anything >0 is fine

// 628180B8-308D-4c5e-B7DB-1BEB62E62EF4
static const GUID VMBUS_SERVICE_ID = {.Data = {0xb8, 0x80, 0x81, 0x62, 0x8d, 0x30, 0x5e, 0x4c, 0xb7, 0xdb, 0x1b, 0xeb, 0x62, 0xe6, 0x2e, 0xf4} };

#define MAX_NUM_CPUS	1


typedef struct {
	u64					Align8;
	HV_INPUT_SIGNAL_EVENT	Event;
} HV_INPUT_SIGNAL_EVENT_BUFFER;

typedef struct {
	u64	GuestId;			// XenLinux or native Linux. If XenLinux, the hypercall and synic pages has already been initialized
	void*	HypercallPage;

	BOOL	SynICInitialized;
	// This is used as an input param to HvCallSignalEvent hypercall. The input param is immutable
	// in our usage and must be dynamic mem (vs stack or global).
	HV_INPUT_SIGNAL_EVENT_BUFFER *SignalEventBuffer;
	HV_INPUT_SIGNAL_EVENT *SignalEventParam; // 8-bytes aligned of the buffer above

	HANDLE	synICMessagePage[MAX_NUM_CPUS];
	HANDLE	synICEventPage[MAX_NUM_CPUS];
} HV_CONTEXT;

extern HV_CONTEXT gHvContext;


//
// Inline routines
//
static inline unsigned long long ReadMsr(int msr)
{
	unsigned long long val;

	RDMSR(msr, val);

	return val;
}

static inline void WriteMsr(int msr, u64 val)
{
	WRMSR(msr, val);

	return;
}

//
// Hv Interface
//
static int
HvInit(
    void
    );

static void
HvCleanup(
    void
    );

static HV_STATUS
HvPostMessage(
	HV_CONNECTION_ID connectionId,
	HV_MESSAGE_TYPE  messageType,
	void *            payload,
	SIZE_T           payloadSize
	);

static HV_STATUS
HvSignalEvent(
	void
	);

static int
HvSynicInit(
	u32		irqVector
	);

static void
HvSynicCleanup(
	void
	);

#endif // __HV_H__
