#ifndef __INCear_defs_h
#define __INCear_defs_h


#define REGISTER_HEADER_TYPE 	0 
#define REGISTER_WRITES_TYPE 	1 
#define VERSION_MASK_TYPE 	2 
#define VERSION_ID_TYPE 	3
#define WRITE_TYPE0	0
#define WRITE_TYPE1	1
#define WRITE_TYPE2	2
#define WRITE_TYPE3	3
#define ALL_MODES	0xDF


#define ASSERT(x) assert(x)
#define A_DIV_UP(x, y) (((x) + (y) - 1) / (y))

typedef struct register_header REGISTER_HEADER;
typedef struct register_write_type0 REGISTER_WRITE_TYPE0;
typedef struct register_write_type1 REGISTER_WRITE_TYPE1;
typedef struct register_write_type2 REGISTER_WRITE_TYPE2;
typedef struct register_write_type3 REGISTER_WRITE_TYPE3;
typedef struct register_cfg  REGISTER_CFG;
typedef struct earCfg EAR_CFG;

struct register_header {
	union {
	A_UINT16  value;
	struct header_bits {
	   unsigned reg_modality_mask:9;
           unsigned type:2;
	   unsigned stage:2;
	   unsigned channel_modifier_present:1;
	   unsigned disabler_present:1;
	   unsigned bit15:1;	
	}pack0;
	}field0;
	union {
		A_UINT16  channel_modifier;
		struct channel_modifier_bits {
			unsigned bit0_14:15;
			unsigned bit15:1;	
		} pack1;
	} field1;
	union {
		A_UINT16 disabler_mask;
		struct disabler_mask_bits {
			unsigned bank0:1;
			unsigned bank1:1;
			unsigned bank2:1;
			unsigned bank3:1;
			unsigned bank4:1;
			unsigned bank5:1;
			unsigned bank6:1;
			unsigned bank7:1;
			unsigned c0:1;
			unsigned c1:1;
			unsigned c2:1;
			unsigned c3:1;
			unsigned c4:1;
			unsigned c5:1;
			unsigned bit14:1;
			unsigned pll:1;
		}pack2;
	} field2;
	A_UINT16  pll_value;
} ;

struct register_write_type0 {
	union {
		A_UINT16 value;
		struct type0_bits {
		   unsigned tag:2;
		   unsigned address:14;
		}pack;
	}field0;
	A_UINT16  msw;
	A_UINT16  lsw;
	REGISTER_WRITE_TYPE0 *next;
} ;

struct register_write_type1 {
	union {
		A_UINT16 value;
		struct type1_bits {
		   unsigned num:2;
		   unsigned address:14;
		} pack;
	}field0;
	A_UINT16 data_msw[4];
	A_UINT16 data_lsw[4];
	REGISTER_WRITE_TYPE1 *next;
} ;

struct register_write_type2 {
	union {
		A_UINT16 value;
		struct type2_bits {
		   unsigned start_bit:9;
		   unsigned extended:1;
		   unsigned column:2;
		   unsigned last:1;
		   unsigned analog_bank:3;
		} pack0;
	}field0;
	union {
		A_UINT16 value;
		struct type2_bits1 {
		   unsigned data:12;	
		   unsigned num_bits:4;
		} pack1;
	}field1;
	A_UINT16 num_bits;
	A_UINT16 num_data;
	A_UINT16 *data;
	REGISTER_WRITE_TYPE2 *next;
};

struct register_write_type3 {
	union {
		A_UINT16 value;
		struct type3_bits {
		   unsigned num_bits:5;
		   unsigned start_bit:5;
		   unsigned opcode:3;
		   unsigned bit13:1;
		   unsigned bit14:1;
		   unsigned last:1;
		}pack;
	}field0;
	A_UINT16  address;
	A_UINT16  data_msw;
	A_UINT16  data_lsw;
	REGISTER_WRITE_TYPE3 *next;
};

struct register_cfg {
	A_UINT16 version_mask;
	REGISTER_HEADER reg_hdr;
	union {
		REGISTER_WRITE_TYPE0 *reg_write_type0;
		REGISTER_WRITE_TYPE1 *reg_write_type1;
		REGISTER_WRITE_TYPE2 *reg_write_type2;
		REGISTER_WRITE_TYPE3 *reg_write_type3;
	} write_type;
	REGISTER_CFG *next;
} ;

struct earCfg {
	A_UINT16 version_id;
	REGISTER_CFG *reg_cfg;
} ;





#endif
