#ifndef _WINDRVR_USB_H_
#define _WINDRVR_USB_H_

#if defined(__cplusplus)
extern "C" {
#endif  // __cplusplus 

typedef enum {
    PIPE_TYPE_CONTROL     = 0,
    PIPE_TYPE_ISOCHRONOUS = 1,
    PIPE_TYPE_BULK        = 2,
    PIPE_TYPE_INTERRUPT   = 3,
} USB_PIPE_TYPE;

#define IOCTL_WD_USB_RESET_PIPE         WD_CTL_CODE(0x93c)
#define IOCTL_WD_USB_RESET_DEVICE       WD_CTL_CODE(0x93f)
#define IOCTL_WD_USB_SCAN_DEVICES_V432      WD_CTL_CODE(0x940)
#define IOCTL_WD_USB_SCAN_DEVICES           WD_CTL_CODE(0x95a)
#define IOCTL_WD_USB_TRANSFER           WD_CTL_CODE(0x942)
#define IOCTL_WD_USB_DEVICE_REGISTER_V432   WD_CTL_CODE(0x944)
#define IOCTL_WD_USB_DEVICE_UNREGISTER_V432 WD_CTL_CODE(0x945)
#define IOCTL_WD_USB_GET_CONFIGURATION  WD_CTL_CODE(0x946)
#define IOCTL_WD_USB_DEVICE_REGISTER        WD_CTL_CODE(0x958)
#define IOCTL_WD_USB_DEVICE_UNREGISTER      WD_CTL_CODE(0x959)

#define WD_USB_MAX_PIPE_NUMBER_V432 16
#define WD_USB_MAX_PIPE_NUMBER 32
#define WD_USB_MAX_ENDPOINTS WD_USB_MAX_PIPE_NUMBER
#define WD_USB_MAX_INTERFACES 30

#define WD_USB_MAX_DEVICE_NUMBER_V432 20
#define WD_USB_MAX_DEVICE_NUMBER 127

typedef struct
{
    DWORD dwVendorId;
    DWORD dwProductId;
} WD_USB_ID;

typedef enum {
    USB_DIR_IN     = 1,
    USB_DIR_OUT    = 2,
    USB_DIR_IN_OUT = 3,
} USB_DIR;

typedef struct
{
    DWORD dwNumber;        // Pipe 0 is the default pipe
    DWORD dwMaximumPacketSize;
    DWORD type;            // USB_PIPE_TYPE
    DWORD direction;       // USB_DIR
                           // Isochronous, Bulk, Interrupt are either USB_DIR_IN or USB_DIR_OUT
                           // Control are USB_DIR_IN_OUT
    DWORD dwInterval;      // interval in ms relevant to Interrupt pipes
} WD_USB_PIPE_INFO;

typedef struct
{
    DWORD dwNumInterfaces;
    DWORD dwValue;
    DWORD dwAttributes;
    DWORD MaxPower;
} WD_USB_CONFIG_DESC;

typedef struct
{
    DWORD dwNumber;
    DWORD dwAlternateSetting;
    DWORD dwNumEndpoints;
    DWORD dwClass;
    DWORD dwSubClass;
    DWORD dwProtocol;
    DWORD dwIndex;
} WD_USB_INTERFACE_DESC;

typedef struct
{
    DWORD dwEndpointAddress;
    DWORD dwAttributes;
    DWORD dwMaxPacketSize;
    DWORD dwInterval;
} WD_USB_ENDPOINT_DESC;

typedef struct
{
    WD_USB_INTERFACE_DESC Interface;
    WD_USB_ENDPOINT_DESC Endpoints[WD_USB_MAX_ENDPOINTS];
} WD_USB_INTERFACE;

typedef struct 
{
    DWORD uniqueId;
    DWORD dwConfigurationIndex;
    WD_USB_CONFIG_DESC configuration;
    DWORD dwInterfaceAlternatives;
    WD_USB_INTERFACE Interface[WD_USB_MAX_INTERFACES];
} WD_USB_CONFIGURATION;


typedef struct
{
    DWORD   fBusPowered;
    DWORD   dwPorts;              // number of ports on this hub
    DWORD   dwCharacteristics;    // Hub Charateristics
    DWORD   dwPowerOnToPowerGood; // port power on till power good in 2ms
    DWORD   dwHubControlCurrent;  // max current in mA
} WD_USB_HUB_GENERAL_INFO;

typedef struct
{
    WD_USB_ID deviceId;
    DWORD dwHubNum;
    DWORD dwPortNum;
    DWORD fHub;
    DWORD fFullSpeed;
    DWORD dwConfigurationsNum;
    DWORD deviceAddress;
    WD_USB_HUB_GENERAL_INFO hubInfo;
} WD_USB_DEVICE_GENERAL_INFO;

typedef struct
{
    DWORD dwPipes;
    WD_USB_PIPE_INFO Pipe[WD_USB_MAX_PIPE_NUMBER_V432];
} WD_USB_DEVICE_INFO_V432;

typedef struct
{
    DWORD dwPipes;
    WD_USB_PIPE_INFO Pipe[WD_USB_MAX_PIPE_NUMBER];
} WD_USB_DEVICE_INFO;

// IOCTL Structures
typedef struct 
{
    WD_USB_ID searchId; // if dwVendorId==0 - scan all vendor IDs
                        // if dwProductId==0 - scan all product IDs
    DWORD dwDevices;
    DWORD uniqueId[WD_USB_MAX_DEVICE_NUMBER_V432]; // a unique id to identify the device
    WD_USB_DEVICE_GENERAL_INFO deviceGeneralInfo[WD_USB_MAX_DEVICE_NUMBER_V432];
} WD_USB_SCAN_DEVICES_V432;

typedef struct 
{
    WD_USB_ID searchId; // if dwVendorId==0 - scan all vendor IDs
                        // if dwProductId==0 - scan all product IDs
    DWORD dwDevices;
    DWORD uniqueId[WD_USB_MAX_DEVICE_NUMBER]; // a unique id to identify the device
    WD_USB_DEVICE_GENERAL_INFO deviceGeneralInfo[WD_USB_MAX_DEVICE_NUMBER];
} WD_USB_SCAN_DEVICES;

// USB TRANSFER options
enum { USB_TRANSFER_HALT = 1 };

typedef struct
{
    DWORD hDevice;      // handle of USB device to read from or write to
    DWORD dwPipe;       // pipe number on device
    DWORD fRead;
    DWORD dwOptions;    // USB_TRANSFER options:
                        //    USB_TRANSFER_HALT halts the pervious transfer
    PVOID pBuffer;      // pointer to buffer to read/write
    DWORD dwBytes;
    DWORD dwTimeout;    // timeout for the transfer in milli-seconds. 0==>no timeout.
    DWORD dwBytesTransfered;    // returns the number of bytes actually read/written
    BYTE  SetupPacket[8];       // setup packet for control pipe transfer
    DWORD fOK;          // transfer succeeded
} WD_USB_TRANSFER;

typedef struct {
    DWORD uniqueId;                 // the device unique ID
    DWORD dwConfigurationIndex;     // the index of the configuration to register
    DWORD dwInterfaceNum;           // interface to register
    DWORD dwInterfaceAlternate;
    DWORD hDevice;                  // handle of device
    WD_USB_DEVICE_INFO_V432 Device;      // description of the device
    DWORD dwOptions;                // should be zero
    CHAR  cName[32];                // name of card
    CHAR  cDescription[100];        // description
} WD_USB_DEVICE_REGISTER_V432;

typedef struct {
    DWORD uniqueId;                 // the device unique ID
    DWORD dwConfigurationIndex;     // the index of the configuration to register
    DWORD dwInterfaceNum;           // interface to register
    DWORD dwInterfaceAlternate;
    DWORD hDevice;                  // handle of device
    WD_USB_DEVICE_INFO Device;      // description of the device
    DWORD dwOptions;                // should be zero
    CHAR  cName[32];                // name of card
    CHAR  cDescription[100];        // description
} WD_USB_DEVICE_REGISTER;

typedef struct
{
    DWORD hDevice;
    DWORD dwPipe;
} WD_USB_RESET_PIPE;

#define WD_UsbScanDevice(h, pScan)\
    WD_FUNCTION(IOCTL_WD_USB_SCAN_DEVICES, h, pScan,sizeof(WD_USB_SCAN_DEVICES), FALSE)
#define WD_UsbScanDeviceV432(h, pScan)\
    WD_FUNCTION(IOCTL_WD_USB_SCAN_DEVICES_V432, h, pScan,sizeof(WD_USB_SCAN_DEVICES_V432), FALSE)
#define WD_UsbGetConfiguration(h, pConfig) \
    WD_FUNCTION(IOCTL_WD_USB_GET_CONFIGURATION, h, pConfig, sizeof(WD_USB_CONFIGURATION), FALSE)
#define WD_UsbDeviceRegister(h, pRegister)\
    WD_FUNCTION(IOCTL_WD_USB_DEVICE_REGISTER, h, pRegister, sizeof(WD_USB_DEVICE_REGISTER), FALSE)
#define WD_UsbDeviceRegisterV432(h, pRegister)\
    WD_FUNCTION(IOCTL_WD_USB_DEVICE_REGISTER_V432, h, pRegister, sizeof(WD_USB_DEVICE_REGISTER_V432), FALSE)
#define WD_UsbTransfer(h, pTrans)\
    WD_FUNCTION(IOCTL_WD_USB_TRANSFER, h, pTrans, sizeof(WD_USB_TRANSFER), TRUE)
#define WD_UsbDeviceUnregister(h, pRegister)\
    WD_FUNCTION(IOCTL_WD_USB_DEVICE_UNREGISTER, h, pRegister, sizeof(WD_USB_DEVICE_REGISTER), FALSE)
#define WD_UsbDeviceUnregisterV432(h, pRegister)\
    WD_FUNCTION(IOCTL_WD_USB_DEVICE_UNREGISTER_V432, h, pRegister, sizeof(WD_USB_DEVICE_REGISTER_V432), FALSE)
#define WD_UsbResetPipe(h, pResetPipe)\
    WD_FUNCTION(IOCTL_WD_USB_RESET_PIPE, h, pResetPipe, sizeof(WD_USB_RESET_PIPE), FALSE)
#define WD_UsbResetDevice(h, hDevice)\
    WD_FUNCTION(IOCTL_WD_USB_RESET_DEVICE, h, &hDevice, sizeof(DWORD), FALSE)

#ifdef __cplusplus
}
#endif  // __cplusplus 

#endif // _WINDRVR_USB_H_
