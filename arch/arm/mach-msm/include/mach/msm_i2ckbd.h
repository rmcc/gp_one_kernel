/*
 *  Copyright (c) 2008-2009 QUALCOMM USA, INC.
 *
 *  All source code in this file is licensed under the following license
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  version 2 as published by the Free Software Foundation.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, you can find it at http://www.fsf.org
 */

#ifndef _MSM_I2CKBD_H_
#define _MSM_I2CKBD_H_

struct msm_i2ckbd_platform_data {
	int  gpioreset;
	int  gpioirq;

/* FIH_ADQ, AudiPCHuang, 2009/03/27, { */
/* ZEUS_ANDROID_CR, I2C Configuration for Keypad Controller */
///+FIH_ADQ
	int gpio_vol_up;
	int gpio_vol_dn;
	int gpio_hall_sensor;
	int gpio_ring_switch;
	int gpio_hook_switch;
///-FIH_ADQ
/* } FIH_ADQ, AudiPCHuang, 2009/03/27 */
};

#endif
