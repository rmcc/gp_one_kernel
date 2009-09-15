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


#ifndef _CHANNEL_INTERFACE_H_
#define _CHANNEL_INTERFACE_H_

#include "include/VmbusApi.h"

INTERNAL void
GetChannelInterface(
	VMBUS_CHANNEL_INTERFACE *ChannelInterface
	);

INTERNAL void
GetChannelInfo(
	PDEVICE_OBJECT		Device,
	DEVICE_INFO			*DeviceInfo
	);

#endif // _CHANNEL_INTERFACE_H_
