/* cal_gen5.h - gen5 calibration header definitions */

/* Copyright (c) 2004 Atheros Communications, Inc., All Rights Reserved */

#ifndef __INCcalgen5h
#define __INCcalgen5h
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define IQ_CAL_RATEMASK		 0x01
#define IQ_CAL_RATE				6
#define IQ_CAL_NUMPACKETS    100
#define NUM_DUMMY_EEP_MAP1_LOCATIONS 10


A_UINT16 dutCalibration_gen5(A_UINT32 devNum, A_UINT32 mode);
A_BOOL setup_datasets_for_cal_gen5(A_UINT32 devNum, A_UINT32 mode);
void measure_all_channels_gen5(A_UINT32 devNum, A_UINT32 debug, A_UINT32 mode);
A_UINT16 measure_all_channels_generalized_gen5(A_UINT32 devNum, A_UINT32 debug, A_UINT32 mode);
void dump_raw_data_to_file_gen5( RAW_DATA_STRUCT_GEN5 *pRawData, char *filename);
void raw_to_eeprom_dataset_gen5(RAW_DATA_STRUCT_GEN5 *pRawDataset, EEPROM_DATA_STRUCT_GEN5 *pCalDataset) ;
void fill_words_for_eeprom_gen5(A_UINT32 *word, A_UINT16 numWords, A_UINT16 *fbin, 
							 A_UINT16 dbmmask, A_UINT16 pcdmask, A_UINT16 freqmask);

A_UINT16 fill_eeprom_words_for_curr_ch_gen5(A_UINT32 *word, EEPROM_DATA_PER_CHANNEL_GEN5 *pCalCh);


void copy_11g_cal_to_11b_gen5(RAW_DATA_STRUCT_GEN5 *pRawDataSrc, RAW_DATA_STRUCT_GEN5 *pRawDataDest);
/*
void read_dataset_from_file_gen5(RAW_DATA_STRUCT_GEN5 *pDataSet, char *filename);

void golden_iq_cal(A_UINT32 devNum, A_UINT32 mode, A_UINT32 channel);
void dut_iq_cal(A_UINT32 devNum, A_UINT32 mode, A_UINT32 channel);
void get_cal_info_for_mode_gen5(A_UINT32 *word, A_UINT16 numWords, A_UINT16 *fbin, 
					            A_UINT16 dbmmask, A_UINT16 pcdmask, A_UINT16 freqmask, A_UINT32 mode);
*/
#ifdef __cplusplus
}
#endif

#endif //__INCmauicalh



