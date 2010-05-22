/* cal_gen3.h - gen3 calibration header definitions */

/* Copyright (c) 2000 Atheros Communications, Inc., All Rights Reserved */

#ifndef __INCcalgen3h
#define __INCcalgen3h
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define IQ_CAL_RATEMASK		 0x01
#define IQ_CAL_RATE				6
#define IQ_CAL_NUMPACKETS    100


void dutCalibration_gen3(A_UINT32 devNum, A_UINT32 mode);
A_BOOL setup_datasets_for_cal_gen3(A_UINT32 devNum, A_UINT32 mode);
void measure_all_channels_gen3(A_UINT32 devNum, A_UINT32 debug, A_UINT32 mode);
void copy_11g_cal_to_11b_gen3(RAW_DATA_STRUCT_GEN3 *pRawDataSrc, RAW_DATA_STRUCT_GEN3 *pRawDataDest);

void dump_raw_data_to_file_gen3( RAW_DATA_STRUCT_GEN3 *pRawData, char *filename);
void raw_to_eeprom_dataset_gen3(RAW_DATA_STRUCT_GEN3 *pRawDataset, EEPROM_DATA_STRUCT_GEN3 *pCalDataset) ;
void fill_words_for_eeprom_gen3(A_UINT32 *word, A_UINT16 numWords, A_UINT16 *fbin, 
							 A_UINT16 dbmmask, A_UINT16 pcdmask, A_UINT16 freqmask);
void read_dataset_from_file_gen3(RAW_DATA_STRUCT_GEN3 *pDataSet, char *filename);

void golden_iq_cal(A_UINT32 devNum, A_UINT32 mode, A_UINT32 channel);
A_UINT16 dut_iq_cal(A_UINT32 devNum, A_UINT32 mode, A_UINT32 channel);
void get_cal_info_for_mode_gen3(A_UINT32 *word, A_UINT16 numWords, A_UINT16 *fbin, 
					            A_UINT16 dbmmask, A_UINT16 pcdmask, A_UINT16 freqmask, A_UINT32 mode);

#ifdef __cplusplus
}
#endif

#endif //__INCmauicalh



