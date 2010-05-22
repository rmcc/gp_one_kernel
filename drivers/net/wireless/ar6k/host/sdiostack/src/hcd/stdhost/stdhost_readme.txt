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

Standard Host Controller Driver Core

This sample driver source (stdhost) provides reference code for a
SDIO Standard Host 2.0 compliant controller.  The source code is designed to be compiled 
within a bus-specific implementation (i.e PCI).  The bus implementation is responsible for
allocating I/O, interrupt and DMA resources.  The reference code may contain OS-specific helper
functions that may handle common resource allocations (i.e. DMA, I/O Work items).
Refer to the pci_std sample to view how the reference code is used on some OS implementations. 

The core provides the following features:

   1. Host Controller Driver interface to the SDIO stack (request and configuration processing).
   2. Abstraction of interrupt handling, deferred work and card insertion/removal.
   3. OS-specific implementation of Direct Memory Access (DMA) (see pci_std reference).
   4. OS-specific management of multiple slot instances (see pci_std reference).



          

