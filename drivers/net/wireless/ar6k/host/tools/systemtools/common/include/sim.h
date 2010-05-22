#ifndef __SIM_H__
#define __SIM_H__

A_INT32 PCIsimInitCmd(A_INT32);
A_INT32 PCIsimGetIdsel(A_UINT32 *, A_UINT32 *);
A_INT32 PCIsimConfigReadB(A_UINT32 id_select,A_UINT32 address, A_UINT8 *data);
A_INT32 PCIsimConfigReadW(A_UINT32 id_select,A_UINT32 address, A_UINT16 *data);
A_INT32 PCIsimConfigReadDW(A_UINT32 id_select,A_UINT32 address, A_UINT32 *data);
A_INT32 PCIsimConfigWriteB(A_UINT32 id_select, A_UINT32 address, A_UINT8 data);
A_INT32 PCIsimConfigWriteW(A_UINT32 id_select, A_UINT32 address, A_UINT16 data);
A_INT32 PCIsimConfigWriteDW(A_UINT32 id_select, A_UINT32 address, A_UINT32 data);
A_INT32 PCIsimMemReadB(A_UINT32 address, A_UINT8 *data);
A_INT32 PCIsimMemReadW(A_UINT32 address, A_UINT16 *data);
A_INT32 PCIsimMemReadDW(A_UINT32 address, A_UINT32 *data);
A_INT32 PCIsimMemWriteB(A_UINT32 address, A_UINT8 data);
A_INT32 PCIsimMemWriteW(A_UINT32 address, A_UINT16 data);
A_INT32 PCIsimMemWriteDW(A_UINT32 address, A_UINT32 data);
A_INT32 PCIsimIOReadB(A_UINT32 address,A_UINT8 * data);
A_INT32 PCIsimIOWriteB(A_UINT32 address,A_UINT8 data);
A_INT32 PCIsimGetTime(A_UINT32 *time_high, A_UINT32 *time_low);
A_INT32 PCIsimTimeWait(A_UINT32 delay);
A_INT32 PCIsimCreateEvent(A_UINT32 type, A_UINT32 persistent,A_UINT32 idselect,A_UINT32 evt_handle, A_UINT32 *event_data);
A_INT32 PCIsimGetNextEvent(A_UINT32 *type, A_UINT32 *persistent,A_UINT32 idselect,A_UINT32 *evt_handle, A_UINT32 *event_data);
A_INT32 PCIsimQuit();

#endif
