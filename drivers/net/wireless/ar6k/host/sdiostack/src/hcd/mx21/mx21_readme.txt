// Copyright (c) 2006 Atheros Communications Inc.
// 
//
// The software source and binaries included in this development package are
// licensed, not sold. You, or your company, received the package under one
// or more license agreements. The rights granted to you are specifically
// listed in these license agreement(s). All other rights remain with Atheros
// Communications, Inc., its subsidiaries, or the respective owner including
// those listed on the included copyright notices.  Distribution of any
// portion of this package must be in strict compliance with the license
// agreement(s) terms.
//
//

Codetelligence Embedded SDIO Stack Host Controller Driver Read Me.

Freescale iMX21 SD/SDIO Host Controller Driver

General Notes:
- SD 1 and 4 bit modes, up to 20 Mhz (max) Clock rates (depends on Peripheral Clock Rate).
  ** note ** On the MX21ADS board the peripheral clock rate is set to 33.25 Mhz.
     This clock is divided down to a base clock of ~16.625 Mhz to satisfy the 
     maximum 20 Mhz operating clock for the SD controller.  The SD bus operates 
     at maximum of 16.6 Mhz.
  ** note ** The MX21 has a DMA HCLK errata which prevents DMA use for multi-block writes.
     This errata forces certain multi-block write operations (depending on SDIO module clock)
     to use PIO mode instead of DMA.
- By design, the SDIO controller can only handle multi-block data transfers if the block
  length follows the following guidelines:
     a. In 4-bit mode, the block length must be a multiple of 64 bytes.
     b. In 1-bit mode, the block length must be a multiple of 16 bytes.
  There is no restriction for single block data transfers (1-2048 bytes is allowed).
- The MX21 does not appear to handle the busy signal correctly for memory cards, if the
  number of blocks in a memory card write transfer is high, there is a possibility that the
  next read or write operation will fail.
- SDIO IRQ detection 1,4 bit modes.
- Mechanical Card Switch detect disabled (uses card detect polling).
- Short Transfer Optimization implemented

Linux Notes:

Module Parameters:
   
    "debug" =  set module debug level (default = 4).
     
    Module Debug Level:      Description of Kernel Prints:
           7                   Setup/Initialization
           8                   Card Insertion (if mechanical card switch implemented)
           9                   SDIO Card Interrupt  
          10                   Processing bus requests w/ Data
          11                   Processing for all bus requests.
          12                   Dump data associated with bus requests
          13                   Configuration Requests.
          14                   SDIO controller IRQ processing
          

