// Copyright (c) 2007 Atheros Communications Inc.
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

PCI Standard Host Controller Driver

General Notes:
-  SDIO Standard Host 2.0 compliant.
-  SD 1 and 4 bit modes, up to 50 Mhz Clock rates and SD High Speed Mode.
-  SDIO IRQ detection for 1,4 bit modes.
-  Programmed I/O mode (non-DMA).
-  Simple DMA Support
-  Advanced DMA Support (scatter-gather)
-  Supports Tokyo Electron Ellen II MMC8 bus widths.
-  Card detect via slot mechanical switch
-  Configurable idle bus clock rate
    
Linux Notes:

Module Parameters:
   
    "debug" =  set module debug level (default = 4).
    Module Debug Level:      Description of Kernel Prints:
           7                   Setup/Initialization
           8                   Card Insertion (if mechanical card switch implemented)
           9                   Processing bus requests w/ Data
          10                   Processing for all bus requests.
          11                   Configuration Requests.
          12                   SDIO controller IRQ processing
          13                   Clock Control
          14                   SDIO Card Interrupt  


    "IdleBusClockRate" - idle bus clock rate in Hz (active when 4-bit interrupt detection is required) 
                         Lower values will reduce power at the cost of higher interrupt detection
                         latency.  Typical values are 2000000 (2Mhz), 4000000 (4 Mhz) 

