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

OMAP 1610/5912 SD/SDIO Host Controller Driver

General Notes:
-  SD 1 and 4 bit(memory cards only), up to 24 Mhz Clock rates.
-  SDIO IRQ detection for 1 bit only (see OMAP1610/5912 errata)
-  Common buffer DMA for most data transfers.
-  Short Data Transfer Optimization
-  Scatter-Gather DMA enabled for function drivers that support it. 
-  Card detect via bus driver polling (no mechanical switch)
-  MMCPlus (4.1 or higher) cards supported using wide 4-bit buses and up to 48 Mhz clock rates.
    
Linux Notes:
    
Module Parameters:
   
    "debug" =  set module debug level (default = 4).
    "builtin_card" = if set to one, card detect polling is bypassed and the card in
                     the slot will be enumerated immediately.  
    "async_irq" = If this flag is set to 1, the driver will allow SDIO cards to operate in
                  4 bit mode.  This should only be set if the SDIO card can assert a card
                  interrupt without the presence of an SD clock.  The OMAP controller stops
                  the clock after each bus transaction, however it is still capable of 
                  detecting interrupts on the DAT1 line.
                    
    Module Debug Level:      Description of Kernel Prints:
           7                   Setup/Initialization
           8                   Reserved
           9                   SDIO Card Interrupt            
          10                   Work item prints   
          11                   Processing bus requests w/ Data
          12                   Processing for all bus requests.
          13                   Configuration Requests.
          14                   SDIO controller IRQ processing
          15                   Clock Control



          

