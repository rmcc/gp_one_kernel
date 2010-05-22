
#ifndef	__INCphs_manlibh
#define	__INCphs_manlibh
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "wlantype.h"
#include "athreg.h"
#include "manlib.h"
#ifndef SWIG

MANLIB_API void cont_rx_test(A_UINT32 devNum, int chan, int num_bytes, A_UINT32 addr, int sys_num, int buf_num, int dma_chan, int scramble, int encryption, int data_type);
MANLIB_API void cont_rx_stop(A_UINT32 devNum, A_UINT32 buf_num);

MANLIB_API void cont_tx_test(A_UINT32 devNum, int tx_mode, int chan, int sys_num, int buf_num, int slot_num, int scramble, int encryption, int link_type, int data_type);
MANLIB_API void cont_tx_stop(A_UINT32 devNum);
MANLIB_API void rssi_test(A_UINT32 devNum, int chan, int num_meas, A_UINT32 addr, int sys_num, int buf_num, int scramble, int encryption);
MANLIB_API void audio_loopback_test(A_UINT32 devNum, int rcvr_gain, int mic_gain, int dma_chan);
MANLIB_API void audio_speaker_test(A_UINT32 devNum, A_UINT32 addr, int num_bytes, int spkr_gain, int dma_chan);
MANLIB_API void phs_cal_setup(A_UINT32 devNum, int chan);
MANLIB_API void rx_cal_setup(A_UINT32 devNum);
MANLIB_API void tx_cal_setup(A_UINT32 devNum);
MANLIB_API void end_cal(A_UINT32 devNum);
MANLIB_API int run_tx_leak_cal(A_UINT32 devNum);
MANLIB_API int run_tx_dc_power_cal(A_UINT32 devNum, double pwr, int iter, double cable_loss, int *max_dac_val);
MANLIB_API int run_tx_burst_power_cal(A_UINT32 devNum, double pwr, int iter, double cable_loss, int *max_dac_val);
MANLIB_API int run_tx_iq_mismatch_cal(A_UINT32 devNum);
MANLIB_API int run_rx_iq_mismatch_cal(A_UINT32 devNum);
MANLIB_API int run_rx_gain_cal(A_UINT32 devNum, double signal_level, double cable_loss);
MANLIB_API int run_rx_filter_cal(A_UINT32 devNum);
MANLIB_API int run_rx_peak_cal(A_UINT32 devNum);
MANLIB_API int run_rx_dc_offset_cal(A_UINT32 devNum);
MANLIB_API int run_xtal_cal(A_UINT32 devNum);
MANLIB_API int run_predistort_cal(A_UINT32 devNum, int max_dac_val);
MANLIB_API int run_chip_temp_cal(A_UINT32 devNum, double temp);
MANLIB_API int run_batt_volt_cal(A_UINT32 devNum);
MANLIB_API int run_batt_temp_cal(A_UINT32 devNum);
MANLIB_API int run_ref_volt_cal(A_UINT32 devNum, int voltage, int iter);

#endif // #ifndef SWIG

#ifdef __cplusplus
}
#endif

#endif // #define __INCmanlibh

