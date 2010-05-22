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

OMAP 2420 SD/SDIO Host Controller Driver

General Notes:
-  SD 1 and 4 bit( memory cards and some SDIO cards), up to 24 Mhz Clock rates.
-  Common buffer DMA for most data transfers.
-  Short Data Transfer Optimization
-  Scatter-Gather DMA enabled for function drivers that support it. 
-  Card detect via bus driver polling (no mechanical switch)
-  MMCPlus (4.1 or higher) cards supported using wide 4-bit buses and up to 48 Mhz clock rates.
-  SD Slot 1 Tranceiver (TWL) control for OMAP2420 SDP platform.

Work around for DAT0-busy bug:

The OMAP2420 host controller does not correctly handle DAT0-busy (card write busy).  It appears to abort the check
early even though the card may still be busy.  From analysis, the controller detects that the card enters
the busy state but the controller indicates that the card exited the busy state (via EOFB).  However it
was found that the card infact was still busy.  A workaround to this issue requires the use of an
external GPIO pin tied to the DAT0 line to "poll" for the busy state until it releases.

The gpio pin used is defined using module parameters (only slot 0 is supported with this workaround).

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
    "sd0_dat0_gpio_pin" = gpio pin (1,2,3....) 
    "sd0_dat0_gpio_pad_conf_offset"  - pad control register offset
    "sd0_dat0_gpio_pad_conf_byte"    - byte index into register (each register configure 4 pins)   
    "sd0_dat0_gpio_pad_mode_value"   - the mode value to switch the pin for gpio operation
               
    Module Debug Level:      Description of Kernel Prints:
           7                   Setup/Initialization
           8                   Reserved
           9                   SDIO Card Interrupt            
          10                   Work item prints   
          11                   Processing bus requests w/ Data
          12                   Processing for all bus requests.
          13                   Configuration Requests.
          14                   DMA register dump
          15                   SDIO controller IRQ processing
          16                   Clock Control



          

