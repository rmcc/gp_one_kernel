//------------------------------------------------------------------------------
// <copyright file="miscdrv.h" company="Atheros">
//    Copyright (c) 2004-2007 Atheros Corporation.  All rights reserved.
// 
// The software source and binaries included in this development package are
// licensed, not sold. You, or your company, received the package under one
// or more license agreements. The rights granted to you are specifically
// listed in these license agreement(s). All other rights remain with Atheros
// Communications, Inc., its subsidiaries, or the respective owner including
// those listed on the included copyright notices.  Distribution of any
// portion of this package must be in strict compliance with the license
// agreement(s) terms.
// </copyright>
// 
// <summary>
// 	Wifi driver for AR6002
// </summary>
//
//------------------------------------------------------------------------------
//==============================================================================
// Author(s): ="Atheros"
//==============================================================================
#ifndef _MISCDRV_H
#define _MISCDRV_H


#define HOST_INTEREST_ITEM_ADDRESS(target, item)    \
(((target) == TARGET_TYPE_AR6001) ?     \
   AR6001_HOST_INTEREST_ITEM_ADDRESS(item) :    \
   AR6002_HOST_INTEREST_ITEM_ADDRESS(item))

A_UINT32 ar6kRev1Array[][128]   = { {0x52dfe0,  0x00239100}, // 0
                                    {0x52dfe4,  0xf0102282}, // 1
                                    {0x52dfe8,  0x23a20020}, // 2
                                    {0x52dfec,  0x0008e09e}, // 3
                                    {0x52dff0,  0xc0002441}, // 4
                                    {0x52dff4,  0x24a20020}, // 5
                                    {0x52dff8,  0x046a67b0}, // 6
                                    {0x52dffc,  0x63b20b0c}, // 7
                                    {0x80fc,    0x0},        // 8
                                    {0x817c,    0x4e1020},   // 9
                                    {0x81fc,    0x12dfe0},   // 10
                                    {0x807c,    0x1},        // 11
                                    {0x52dfc0,  0x00203130}, // 12
                                    {0x52dfc4,  0x220020c0}, // 13
                                    {0x52dfc8,  0x63f2b76e}, // 14
                                    {0x52dfcc,  0x0c0a0c9e}, // 15
                                    {0x52dfd0,  0x21a4d20b}, // 16
                                    {0x52dfd4,  0xf00020c0}, // 17
                                    {0x52dfd8,  0x34c10020}, // 18
                                    {0x52dfdc,  0x00359100}, // 19
                                    {0x80f8,    0x0},        // 20
                                    {0x8178,    0x4e12e0},   // 21
                                    {0x81f8,    0x12dfc0},   // 22
                                    {0x8078,    0x1},        // 23
                                    {0x52dfa0,  0x04a87685}, // 24
                                    {0x52dfa4,  0x1b0020f0}, // 25
                                    {0x52dfa8,  0x00029199}, // 26
                                    {0x52dfac,  0x3dc0dd90}, // 27
                                    {0x52dfb0,  0x04ad76f0}, // 28
                                    {0x52dfb4,  0x1b0049a2}, // 29
                                    {0x52dfb8,  0xb19a0c99}, // 30
                                    {0x52dfbc,  0x8ce5003c}, // 31
                                    {0x80f4,    0x0},        // 32
                                    {0x8174,    0x4e1380},   // 33
                                    {0x81f4,    0x12dfa0},   // 34
                                    {0x8074,    0x1},        // 35
                                    {0x4000,    0x100},      // 36 - Reset Command
                                    {0x8074,    0x00},       // 37
                                    {0x8078,    0x00},       // 38
                                    {0x40d8,    0x2e7ddb},   // 39
                                    {0x40dc,    0x0},        // 40
                               };

A_UINT32 ar6kRev2Array[][128]   = {
                                    {0xFFFF, 0xFFFF},      // No Patches
                               };

#define CFG_REV1_ITEMS                40    // 40 patches so far
#define CFG_REV2_ITEMS                0     // no patches so far
#define AR6K_RESET_ADDR               0x4000
#define AR6K_RESET_VAL                0x100

#define EEPROM_SZ                     768
#define EEPROM_WAIT_LIMIT             4

#endif

