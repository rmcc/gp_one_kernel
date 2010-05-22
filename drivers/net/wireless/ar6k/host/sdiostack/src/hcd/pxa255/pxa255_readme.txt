// Copyright (c) 2005 Atheros Communications Inc.
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

Intel PXA255 (Gumstix reference design) Host Controller Driver

General Notes:
-  SD 1-bit mode, up to 20 Mhz clock rates.
-  SDIO 1-bit IRQ detection via GPIO pin.
-  Programmed I/O operation (non-DMA).
-  Short Transfer Optimization implemented
-  Mechanical Card Switch detect disabled (uses card detect polling).

Linux Notes:

Module Parameters:
   
    "debug" =  set module debug level (default = 4).
     
    Module Debug Level:      Description of Kernel Prints:
           7                   Setup/Initialization
           8                   Card Insertion (if mechanical card switch implemented)
           9                   SDIO Card Interrupt  
          10                   Processing bus requests w/ Data
          11                   Processing for all bus requests.
          12                   Configuration Requests.
          13                   SDIO controller IRQ processing
          

