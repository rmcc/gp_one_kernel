/*
 * ------------------------------------------------------------------------------
 * Copyright (c) 2008 Atheros Corporation.  All rights reserved.
 * 
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
 * ------------------------------------------------------------------------------
 * ==============================================================================
 * Author(s): ="Atheros"
 * ==============================================================================
 * athloader.h - definitions for atheros firmware loader.
 *
 * Firmware image format:
 *
 * unsigned char 0xFF                //  "magic"
 * unsigned char version             //  fw loader / tool version, not fw version major/minor 4 bits each
 * char hostname[variable]           //  zero delimited hostname where the image was built
 * unsigned char month               //
 * unsigned char day                 //  Date and time when the image was built
 * unsigned char year                //  year is a number of years after 2008, i.e. 2008 is coided as 0
 * unsigned char hour                //
 * unsigned char min                 //
 * // the rest are optional blocks that may be repeated and/or mixed
 * unsigned char instruction
 *   unsigned char argument[4]       // optional constant, mask or offset
 *   unsigned char image[variable]   // optional loadable image
 * // repeated as necessary
 * unsigned char crc32[4]            // inverted crc32
 *
 */
#ifndef ATHLOADER_IS_SEEN
#define ATHLOADER_IS_SEEN

#define VERSION (0x01)

/* Opcodes */
#define RLoad (0x10)
#define Ror   (0x20)
#define Rand  (0x30)
#define Add   (0x40)
#define Rstor (0x50)
#define Shift (0x60)
#define Nneg  (0x70)
#define Trr   (0x80)
#define Trw   (0x90)
#define Trx   (0xA0)
#define Exit  (0xB0)
#define Cmp   (0xC0)
#define Ldprn (0xD0)
#define Jump  (0xE0)

#endif /* ATHLOADER_IS_SEEN */
