// Copyright (c) 2005, 2006 Atheros Communications Inc.
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

Intel PXA 270 SD/SDIO Host Controller Driver

General Notes:
- SD 1 and 4 bit modes, up to 19.5 Mhz Clock rates.
- SDIO IRQ detection 1,4 bit modes.
- Programmed I/O mode supported.
- Common Buffer DMA implemented for most data transfers.
- Scatter-Gather DMA enabled for function drivers that can use it. 
- Mechanical Card Switch detect disabled (uses card detect polling).
- Short Transfer Optimization implemented

Linux Notes:

Module Parameters:
    "noDMA" = force driver to operate without DMA
    "debug" =  set module debug level (default = 4).
    "builtin_card" = if set to one, card detect polling is bypassed and the card in
                     the slot will be enumerated immediately.  
    
    Module Debug Level:      Description of Kernel Prints:
           7                   Setup/Initialization
           8                   Card Insertion (if mechanical card switch implemented)
           9                   SDIO Card Interrupt  
          10                   Processing bus requests w/ Data
          11                   Processing for all bus requests.
          12                   Dump data associated with bus requests
          13                   Configuration Requests.
          14                   SDIO controller IRQ processing
          

