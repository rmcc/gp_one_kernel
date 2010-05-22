
#ifdef _WINDOWS
 __declspec(dllimport) void  __cdecl Test_dragon1(void);

 __declspec(dllimport) A_BOOL __cdecl closehandle(void);
 
 __declspec(dllimport) A_BOOL  __cdecl loadDriver_ENE(A_BOOL bOnOff,A_UINT32 subSystemID);
 
 __declspec(dllimport) A_BOOL __cdecl  InitAR6000_ene(HANDLE *handle, A_UINT32 *nTargetID);

 __declspec(dllimport) int  __cdecl BMIWriteSOCRegister(HANDLE device,
                    A_UINT32 address,
                    A_UINT32 param);
 
  __declspec(dllimport) int __cdecl BMIReadSOCRegister(HANDLE device,
                   A_UINT32 address,
                   A_UINT32 *param);
  
  __declspec(dllimport) int __cdecl  DisableDragonSleep(HANDLE device);

  __declspec(dllimport) int __cdecl 
BMIReadMemory(HANDLE device,
              A_UINT32 address,
              A_UCHAR *buffer,
              A_UINT32 length);

__declspec(dllimport) int __cdecl 
BMIWriteMemory(HANDLE device,
               A_UINT32 address,
               A_UCHAR *buffer,
               A_UINT32 length);

__declspec(dllimport) int __cdecl 
BMISetAppStart(HANDLE device,
               A_UINT32 address);

__declspec(dllimport) int __cdecl 
BMIDone(HANDLE device);

__declspec(dllimport) HANDLE __cdecl 
open_device_ene(A_UINT32 device_fn, A_UINT32 devIndex, char * pipeName);

__declspec(dllimport) DWORD __cdecl DRG_Write(  HANDLE COM_Write,  PUCHAR buf,
						 ULONG length );

__declspec(dllimport) DWORD __cdecl DRG_Read( HANDLE pContext,  PUCHAR buf,
					 ULONG length,  PULONG pBytesRead);

#endif

