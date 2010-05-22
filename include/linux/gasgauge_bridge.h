/*
 * include/linux/gasgauge_bridge.h - platform data structure for f75375s sensor
 *
 * Copyright (C) 2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __GASGAUGE_BRIDGE_H_
#define __GASGAUGE_BRIDGE_H_

#define GASGAUGE_BRIDGE_NAME "gasgauge_bridge"

enum _battery_info_type_ {
	BATT_CAPACITY_INFO,			// 0x06 RARC - Remaining Active Relative Capacity
	BATT_VOLTAGE_INFO,			// 0x0c Voltage MSB
	BATT_TEMPERATURE_INFO,	// 0x0a Temperature MSB
	BATT_CURRENT_INFO,			// 0x0e Current MSB
	BATT_AVCURRENT_INFO,		// 0x08 Average Current MSB
	BATT_STATUS_REGISTER,		// 0x01 Status
	/* FIH_ADQ, Kenny { */
	BATT_ACR_REGISTER,		    // 0x10 Accumulated Current Register MSB
	BATT_AS_REGISTER,		    // 0x14 Age Scalar
	BATT_FULL_REGISTER,		    // 0x16 Full Capacity MSB
	BATT_FULL40_REGISTER,		    // 0x6A Full40 MSB
	/* } FIH_ADQ, Kenny */
};

int GetBatteryInfo(enum _battery_info_type_ info, int * data);
/* FIH_ADQ, Kenny { */
int SetBatteryInfo(enum _battery_info_type_ info, int  data);
/* } FIH_ADQ, Kenny */

#endif /* __GASGAUGE_BRIDGE_H_ */
