Atheros Embedded SDIO Stack Source Kit Read Me.

Embedded SDIO Stack Version 2.6 for Linux

Copyright 2004-2006 Atheros Communications, Inc.

April 18,2006


To install:

1. Unzip/untar the emsdio_src tar file into your home directory:
       
       > cp emsdio_src.tar.gz $HOME
       > cd $HOME
       > tar -zxf emsdio_src.tar.gz

   This will create a $HOME/sdiostack directory. 


TO VIEW HELP DOCUMENTATION:

To read the help documentation open the following HTML file in your browser:
  
    <installpath>/doc/index.htm.  

A compiled HTML help file (.chm) can also be found in :
   
     <installpath>/doc/Help. 
     
This file can be viewed under windows or Linux (with the appropriate viewer).

Please read QuickStart in the Help manual.


LINUX BUILDS:

To compile modules, the target linux kernel source must be compiled before invoking the 
build script.  Please review the QuickStart guide in the help for additional linux kernel compilation
instructions.

!!!! Special Note for 2.6 Kernels !!!!!!!
For linux kernels 2.6 or higher the SDIO bus driver uses the PnP subsystem to register device instances. 
In order to enable the PnP subsystem on non-x86 platforms, you must modify the Kconfig file 
located in /drivers/pnp. Comment out the line: " depends on ISA". 
You will then need to run "make menuconfig" and select "Plug and Play Support". 
You do not need to enable "ISA Plug and Play" or "PnP Debug Messages". 


To build the stack a script (build.scr.*) is provided in the /src directory. The wildcard (*) can be
"hdk","pdk" or "all", depending on whether this is a Host, Peripheral or Source Kit.
This script is used in conjunction with the provided Makefile and localmake.linux.inc file.  Modify 
localmake.linux.inc to set linux kernel source and tool paths. 
The build script can be invoked using:
   
      ./build.scr.all default <Build Type Target> 

   for example:

      ./build.scr.all default LOCAL_i686 

      Will build the SDIO stack against the kernel and tool settings defined in localmake.linux.inc from 
      the "LOCAL_i686" section.

   Please review localmake.linux.inc for available target types and 
   review build.scr.* for additional options. 

   The module binaries can be found in "sdiostack/output/<Build Type Target>".

LIST OF BINARIES:

   sdio_busdriver  - bus driver core that implements card enumeration, muxing and APIs for 
                     host and function drivers.

   sdio_lib - helper library

   sdio_memory_fd - SD/MMC card function driver

   sdio_sample_fd - sample function driver 

   sdio_bluetooth_fd - bluetooth Type-A function driver (Linux 2.6 and higher).

   sdio_gps_fd - GPS class function driver (Linux 2.6 and higher).

   sdio_bench_fd - benchmark driver for memory cards and FPGA-based SDIO test card.

   sdio_*_hcd - platform specific host controller driver


CHANGE LIST:

Changes to 2.6 release:
1. New Standard Host Controller HCD (Ellen II).
2. Improved recursion handling for better async request processing performance.
3. OMAP2420 HCD
4. Add OMAP5912 HCD support.
5. SD High Speed Support.
6. MMC High Speed Support.

Changes to 2.4 release:
1. Function drivers should use SDCONFIG_FUNC_CHANGE_BUS_MODE to change bus mode
   at card initialization. This command replaces SDCONFIG_BUS_MODE_CTRL.

Changes to 2.3 release:
1. Freescale MX21 Host Controller Driver (via MX21ADS reference board).
2. Host Controller Module reference counting.
3. Updated build script to allow compilation of separate user projects.
4. Removed redundant SDIO INT_PENDING register reads.  The register
   was being fetched up to 3 times increasing interrupt processing time.
 
Changes in 2.1-2.2:
1. Customer-specific releases.
  
Changes to 2.0 release:
1. DMA Support (Simple DMA and Scatter Gather DMA)
2. Texas Instruments OMAP 1610 HCD support (supports DMA).
3. Intel PXA270 HCD support (supports DMA).
4. Plug and Play architecture change for compatibility with future versions of Linux.
5. Linux 2.4.20.
6. SD Physical 1.10 (High Speed Supported).
7. MMC 4.1 (High Speed and 4,8-bit wide bus modes).
8. New Short Data transfer bus request mode.
9. SD/MMC memory driver updated to use DMA (if available).
10. SDIO sample driver update to demonstrate DMA I/O.

Changes to 1.1 release:
1. Improved stack performance for bus requests.
2. Improved synchronization locks and bus request debugging.
3. Fixed issues with device/function removal when outstanding requests are present.

Changes to 1.0 release:
1. SDIO 1.1 high current mode support.  Any SDIO 1.1 card that supports the EMPC bit will have the
bit "turned on" if the host supports a slot that can source more than 200mA of current.
2. Individual function driver slot current APIs. Function Drivers should now register their 
hardware power requirement (electrical current in mA) and only proceed with enabling their I/O function
if the registration succeeds.  This prevents high-current consuming I/O functions from damaging or
triggering an over-current situation on some SDIO hosts.  
See sample driver (\src\function\sample\sample.c) for code changes/examples.
3. Minor fixes for combo cards.
4. Module parameters are now visible through /sys/modules/sdio module name.
You can view a parameter using cat and echo.
Examples:
display:
cat /sys/module/sdio_busdriver/debuglevel
set:
echo 3 > /sys/module/sdio_busdriver/debuglevel
The bus driver now exposes a number of parameters, including:
DefaultOperClock - maximum operational clock limit
debuglevel - debuglevel 0-7, controls debug prints
RequestRetries - number of command retries
CardReadyPollingRetry - number of card ready retries
PowerSettleDelay - delay in ms for power to settle after power changes
DefaultOperBlockLen - operational block length
DefaultOperBlockCount - operational block count
5. SDIO 1.1 Cards can supply a CISTPL_MANFID tuple in the card's Function CIS space to use
as a plug and play identifier. Drivers will be passed this value instead of the value found
in the Common CIS.
6. Fix the card test check in the sample function driver and remove the class check.
7. Modules for Linux i586,i686 and XSCALE are installed into the modules directories. 
You will need to move the appropriate module builds to your target system.
8.Fix loadsdiosample.scr script to select either PCI Ellen or PXA 255 (gumstix) host controller
The script should be invoked using: "loadsdiosample.scr pci_ellen" or "loadsdiosample.scr pxa255".
To unload use "loadsdiosample <hcd-type> unloadall".

Linux is a registered trademark of Linux Torvalds
