#ifndef	__INCphsdefsh
#define	__INCphsdefsh

#define RADIO_INIT_FN_ID 0
#define SETUP_MODEM_FN_ID 1
#define SETUP_TDMA_FN_ID 2
#define SETUP_AGC_FN_ID 3
#define SETUP_LS_MATRIX_FN_ID 4
#define POLL_DMA_COMPARE_FN_ID 5

# define M_PI       3.14159265358979323846  /* pi */
# define M_PI_2     1.57079632679489661923  /* pi/2 */
# define M_PI_4     0.78539816339744830962  /* pi/4 */
# define M_1_PI     0.31830988618379067154  /* 1/pi */


#define NO_CAL			0
#define RX_DC_OFF		1
#define RX_XCORR		2
#define RX_SELF_CORR		3

#define CAL_256			0
#define CAL_512			1
#define CAL_1024		2
#define CAL_2048		3

#define TX_DC_OFF		1
#define TX_OFF_TONE		2

#define STATUS_OK		0
#define STATUS_ERR		-1

#define NO_LOOP			0
#define LOOP_FROM_MIXER		1
#define LOOP_FROM_PA		2
#define LOOP_FROM_TX_IN		3

#define DAC_MAX_VAL		512
#define DEFAULT_CHAN		0
#define PHS_CHAN0		1895.15
#define XTAL_FREQ		38.4

#define BB_FILT_ATTEN		34
#define DESIRED_DIG_AMPL	3
#define TOTAL_RX_GAIN		42
#define REF_PHS_POWER		20

#define XTAL_DAC_1		10
#define XTAL_DAC_2		50
#define XTAL_DAC_3		90
#define XTAL_DAC_4		130
#define XTAL_DAC_5		170
#define XTAL_MULTIPLIER		384000
#define XTAL_OFFSET_FREQ	100000
#define XTAL_CAL_PTS		5
#define XTAL_TABLE_ENTRIES	61
#define XTAL_CAP_MIN		0
#define XTAL_CAP_MAX		255

#define TX_MAX_MAG		511
#define POLAR_MAG_ENTRIES	17
#define POLAR_MAG_INCR		(TX_MAX_MAG*1.0)/(POLAR_MAG_ENTRIES-1)
#define MAX_AM_AM_OUT		818.0
#define MAX_AM_AM_IN		1018.0

#define ROOM_TEMP_IN_C		25
#define CAL_BATT_VOLT		3200
#define CAL_BATT_TEMP		1800
#define REF_VOLT		1800

#define SYNTH_MULTIPLIER 262144/38.4
#define PHS_CHAN0 1895.15 /* In MHz */
#define PHS_CHAN_WIDTH 0.3 /* In MHz */
#define SIZE32 4



/* modem related constants */
#define MODEM_POWER_THRESH 10//75
#define MODEM_MAG_ALPHA 230
#define MODEM_PLL_ALPHA 50
#define MODEM_DC_ALPHA 50
#define MODEM_FREQ_OFF 81
#define MODEM_PWR_WIN 48
#define MODEM_PWR_MAT 171
#define MODEM_PWR_EXP 2
#define MODEM_BURST_MAT 128//205
#define MODEM_BURST_EXP 3//4
#define MODEM_CONV_TCH 25
#define MODEM_SYM_TCH 100
#define MODEM_CONV_CCH 52
#define MODEM_SYM_CCH 62

#define MODEM_FREQ_WIN 16
#define MODEM_BURST_WIN 14
#define MODEM_XCORR_THRESH 2500

#define CONT_RX_EARLY_START 200//1698

#define SIG_PWR_THR	226
#define RESCALE_LEN	5
#define SAT_CNT_THR	3
#define NEG_SAT_THR	25
#define POS_SAT_THR	100


#endif
