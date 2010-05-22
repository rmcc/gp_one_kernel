#ifndef	__INCphsinitfunctionsh
#define	__INCphsinitfunctionsh

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
void setup_agc(A_UINT32 devNum);
void setup_modem(A_UINT32 devNum);
void setup_ls_matrix(A_UINT32 devNum);
void setup_tdma(A_UINT32 devNum);
void radio_init(A_UINT32 devNum);
void poll_dma_compare(A_UINT32 devNum, A_UINT32 buf_num, A_UINT32 addr, A_UINT32 num_bytes, A_UINT32 data_type);
void enable_update_slot_num(A_UINT32 devNum, A_UINT32 sys_num);
#ifdef __cplusplus
}
#endif

#endif // __INCphslibincludesh


