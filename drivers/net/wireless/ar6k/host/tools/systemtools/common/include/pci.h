/*
 * $Id: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/include/pci.h#1 $
 *
 * PCI config space definitions and probing support.
 *
 * Copyright © 2000-2003 Atheros Communications, Inc., All Rights Reserved
 *
 * Sample Code from Microsoft Windows 2000 Driver Development Kit is
 * used under license from Microsoft Corporation and was developed for
 * Microsoft by Intel Corp., Hillsboro, Oregon: Copyright (c) 1994-1997
 * by Intel Corporation.
 */

#ifndef _PCI_H
#define _PCI_H

#ifdef __cplusplus
extern "C" {
#endif

//-------------------------------------------------------------------------
// PCI Register Definitions
// Refer To The PCI Specification For Detailed Explanations
//-------------------------------------------------------------------------

// Register Offsets
#define PCI_VENDOR_ID_REGISTER      0x00    // PCI Vendor ID Register
#define PCI_DEVICE_ID_REGISTER      0x02    // PCI Device ID Register
#define PCI_CONFIG_ID_REGISTER      0x00    // PCI Configuration ID Register
#define PCI_COMMAND_REGISTER        0x04    // PCI Command Register
#define PCI_STATUS_REGISTER         0x06    // PCI Status Register
#define PCI_REV_ID_REGISTER         0x08    // PCI Revision ID Register
#define PCI_CLASS_CODE_REGISTER     0x09    // PCI Class Code Register
#define PCI_CACHE_LINE_REGISTER     0x0C    // PCI Cache Line Register
#define PCI_LATENCY_TIMER_REGISTER  0x0D    // PCI Latency Timer Register
#define PCI_HEADER_TYPE             0x0E    // PCI Header Type Register
#define PCI_BIST_REGISTER           0x0F    // PCI Built-In SelfTest Register
#define PCI_BAR_0_REGISTER          0x10    // PCI Base Address Register 0
#define PCI_BAR_1_REGISTER          0x14    // PCI Base Address Register 1
#define PCI_BAR_2_REGISTER          0x18    // PCI Base Address Register 2
#define PCI_BAR_3_REGISTER          0x1C    // PCI Base Address Register 3
#define PCI_BAR_4_REGISTER          0x20    // PCI Base Address Register 4
#define PCI_BAR_5_REGISTER          0x24    // PCI Base Address Register 5
#define PCI_SUBVENDOR_ID_REGISTER   0x2C    // PCI SubVendor ID Register
#define PCI_SUBDEVICE_ID_REGISTER   0x2E    // PCI SubDevice ID Register
#define PCI_EXPANSION_ROM           0x30    // PCI Expansion ROM Base Register
#define PCI_INTERRUPT_LINE          0x3C    // PCI Interrupt Line Register
#define PCI_INTERRUPT_PIN           0x3D    // PCI Interrupt Pin Register
#define PCI_MIN_GNT_REGISTER        0x3E    // PCI Min-Gnt Register
#define PCI_MAX_LAT_REGISTER        0x3F    // PCI Max_Lat Register

// Vendor-specific PCI registers
#define PCI_TIMEOUT_REGISTER        0x40    // PCI Bus timers

//-------------------------------------------------------------------------
// PCI configuration hardware ports
//-------------------------------------------------------------------------
#define CF1_CONFIG_ADDR_REGISTER    0x0CF8
#define CF1_CONFIG_DATA_REGISTER    0x0CFC
#define CF2_SPACE_ENABLE_REGISTER   0x0CF8
#define CF2_FORWARD_REGISTER        0x0CFA
#define CF2_BASE_ADDRESS            0xC000

//-------------------------------------------------------------------------
// Configuration Space Header
//-------------------------------------------------------------------------
typedef struct PciConfigStruc {
    A_UINT16    PciVendorId;
    A_UINT16    PciDeviceId;
    A_UINT16    PciCommand;
    A_UINT16    PciStatus;
    A_UCHAR     PciRevisionId;
    A_UCHAR     PciClassCode[3];
    A_UCHAR     PciCacheLineSize;
    A_UCHAR     PciLatencyTimer;
    A_UCHAR     PciHeaderType;
    A_UCHAR     PciBIST;
    A_UINT32    PciBaseReg0;
    A_UINT32    PciBaseReg1;
    A_UINT32    PciBaseReg2;
    A_UINT32    PciBaseReg3;
    A_UINT32    PciBaseReg4;
    A_UINT32    PciBaseReg5;
    A_UINT32    PciReserved0;
    A_UINT32    PciReserved1;
    A_UINT32    PciExpROMAddress;
    A_UINT32    PciReserved2;
    A_UINT32    PciReserved3;
    A_UCHAR     PciInterruptLine;
    A_UCHAR     PciInterruptPin;
    A_UCHAR     PciMinGnt;
    A_UCHAR     PciMaxLat;
} PCI_CONFIG_STRUC, *PPCI_CONFIG_STRUC;

//-------------------------------------------------------------------------
// PCI Class Code Definitions
// Configuration Space Header
//-------------------------------------------------------------------------
#define PCI_BASE_CLASS              0x02    // Base Class = Network Controller
#define PCI_SUB_CLASS               0x00    // Sub Class  = Ethernet Controller
#define PCI_PROG_INTERFACE          0x00    // Prog I/F   = Ethernet Controller
#define PCI_CLASS_BRIDGE            0x06

#define PCI_CLASS_BRIDGE_CARDBUS    0x0607

//-------------------------------------------------------------------------
// PCI Command Register Bit Definitions
// Configuration Space Header
//-------------------------------------------------------------------------
#define CMD_IO_SPACE                0x0001
#define CMD_MEMORY_SPACE            0x0002
#define CMD_BUS_MASTER              0x0004
#define CMD_SPECIAL_CYCLES          0x0008
#define CMD_MEM_WRT_INVALIDATE      0x0010
#define CMD_VGA_PALLETTE_SNOOP      0x0020
#define CMD_PARITY_RESPONSE         0x0040
#define CMD_WAIT_CYCLE_CONTROL      0x0080
#define CMD_SERR_ENABLE             0x0100
#define CMD_BACK_TO_BACK            0x0200

//-------------------------------------------------------------------------
// PCI Status Register Bit Definitions
// Configuration Space Header
//-------------------------------------------------------------------------
#define STAT_BACK_TO_BACK           0x0080
#define STAT_DATA_PARITY            0x0100
#define STAT_DEVSEL_TIMING          0x0600
#define STAT_SIGNAL_TARGET_ABORT    0x0800
#define STAT_RCV_TARGET_ABORT       0x1000
#define STAT_RCV_MASTER_ABORT       0x2000
#define STAT_SIGNAL_MASTER_ABORT    0x4000
#define STAT_DETECT_PARITY_ERROR    0x8000

//-------------------------------------------------------------------------
// PCI Base Address Register For Memory (BARM) Bit Definitions
// Configuration Space Header
//-------------------------------------------------------------------------
#define BARM_LOCATE_BELOW_1_MEG     0x0001
#define BARM_LOCATE_IN_64_SPACE     0x0002
#define BARM_PREFETCHABLE           0x0004

//-------------------------------------------------------------------------
// PCI Base Address Register For I/O (BARIO) Bit Definitions
// Configuration Space Header
//-------------------------------------------------------------------------
#define BARIO_SPACE_INDICATOR       0x0001

//-------------------------------------------------------------------------
// PCI BIOS Definitions
// Refer To The PCI BIOS Specification
//-------------------------------------------------------------------------
// Function Code List
#define PCI_FUNCTION_ID         0xB1    // AH Register
#define PCI_BIOS_PRESENT        0x01    // AL Register
#define FIND_PCI_DEVICE         0x02    // AL Register
#define FIND_PCI_CLASS_CODE     0x03    // AL Register
#define GENERATE_SPECIAL_CYCLE  0x06    // AL Register
#define READ_CONFIG_BYTE        0x08    // AL Register
#define READ_CONFIG_WORD        0x09    // AL Register
#define READ_CONFIG_DWORD       0x0A    // AL Register
#define WRITE_CONFIG_BYTE       0x0B    // AL Register
#define WRITE_CONFIG_WORD       0x0C    // AL Register
#define WRITE_CONFIG_DWORD      0x0D    // AL Register

#define PCI_VENID_O2_MICRO      0x1217
#define PCI_VENID_ENE           0x1524
#define PCI_ADDRESS_INIT        0x80000000
#define PCI_ENUM_REG            0x0CF8
#define PCI_ACCESS_REG          0x0CFC
#define PCI_MEM_LENGTH          0x100
#define PCI_SEARCH_MAX          0x300
#define PCI_DEVID_O2_MICRO_6933 0x6933
#define PCI_DEVID_O2_MICRO_6872 0x6872
#define PCI_DEVID_O2_MICRO_6832 0x6832
#define PCI_DEVID_O2_MICRO_6972 0x6972
#define PCI_DEVID_O2_ENE_1410   0x1410
#define PCI_DEVID_O2_ENE_1421   0x1421
#define O2MICRO_WRT_BURST_EN    0x8
#define O2_6872_WRT_BRST_ADDR   0xd4
#define O2_6933_6832_WRT_BRST_ADDR    0x94

// Function Return Code List
#define PCI_SUCCESSFUL          0x00
#define FUNC_NOT_SUPPORTED      0x81
#define BAD_VENDOR_ID           0x83
#define BAD_REGISTER_NUMBER     0x87

// PCI BIOS Calls
#define PCI_BIOS_INTERRUPT      0x1A        // PCI BIOS Int 1Ah Function Call
#define PCI_PRESENT_CODE        0x20494350  // Hex Equivalent Of 'PCI '

#define PCI_SERVICE_IDENTIFIER  0x49435024  // ASCII Codes for 'ICP$'

//-------------------------------------------------------------------------
// PCI Cards found - returns hardware info after scanning for devices
//-------------------------------------------------------------------------
typedef struct PciCardInfo {
    A_UINT32    BaseIo;
    A_UINT16    VendorID;
    A_UINT16    DeviceID;
    A_UINT16    SubVendorID;
    A_UINT16    SubVendorDeviceID;
    A_UINT16    SlotNumber;         // NDIS slot number
    A_UINT32    MemPhysAddress;     // NIC physical address
    A_UINT32    MemLength;          // NIC footprint
    A_UCHAR     ChipRevision;
    A_UCHAR     Irq;
} PCI_CARD_INFO;

typedef struct PciCardsFoundStruc {
    A_UINT16        NumFound;
    PCI_CARD_INFO   PciSlotInfo[16];
} PCI_CARDS_FOUND_STRUC, *PPCI_CARDS_FOUND_STRUC;

#ifdef __cplusplus
}
#endif

#endif /* _PCI_H */

