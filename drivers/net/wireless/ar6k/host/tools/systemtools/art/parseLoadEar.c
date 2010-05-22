#ifdef __ATH_DJGPPDOS__
#include <unistd.h>
#ifndef EILSEQ  
    #define EILSEQ EIO
#endif	// EILSEQ

 #define __int64	long long
 #define HANDLE long
 typedef unsigned long DWORD;
 #define Sleep	delay
 #include <bios.h>
 #include <dir.h>
#endif	// #ifdef __ATH_DJGPPDOS__

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#if defined(LINUX) || defined(__linux__)
#include "linuxdrv.h"
#else
#include "ntdrv.h"
#endif
#include "common_hw.h"


#include "wlantype.h"
#include "athreg.h"
#include "ear_defs.h"
#include "ear_externs.h"

static char delimiters[]   = " \t\n:\r\f";
#define RESERVED_MODE_BIT_MASK	0x120

int parseEarCfgFile(char *earcfg_filename, EAR_CFG **ear_cfg) {

	FILE *fileStream;
	char lineBuffer[MAX_FILE_WIDTH];
	char *token;
	int select;
	A_UINT16 iIndex;
	REGISTER_CFG *temp_reg_cfg=NULL;
	REGISTER_WRITE_TYPE0 *temp_reg_write_type0=NULL;
	REGISTER_WRITE_TYPE1 *temp_reg_write_type1=NULL;
	REGISTER_WRITE_TYPE2 *temp_reg_write_type2=NULL;
	REGISTER_WRITE_TYPE3 *temp_reg_write_type3=NULL;
	A_UINT16 temp_value, temp_end_bit, temp_num_bits, temp_type1_value;
	A_UINT16 version_mask;
	A_UINT16 version_mask_given=0;

	fileStream = (FILE *) fopen(earcfg_filename, "r");
	if (fileStream == NULL) {
		printf("Unable to open ear cfg file %s\n", earcfg_filename);
		return -1;
	}

	while( fgets(lineBuffer, MAX_FILE_WIDTH, fileStream) != NULL ) {
		//printf("lineBuffer = %s\n", lineBuffer);
		token = (char *) strtok(lineBuffer, delimiters);
		if (token == NULL) continue;
		if (token[0] == '#') {
			continue;
		}
		select = (int) atoi(token);
		//printf("select = %d\n", select);
		switch(select) {
			case REGISTER_HEADER_TYPE: // register header
				ASSERT(version_mask_given != 0);
				if ((*ear_cfg)->reg_cfg == NULL) {
					(*ear_cfg)->reg_cfg = (REGISTER_CFG *) malloc(sizeof(REGISTER_CFG));
					temp_reg_cfg = (*ear_cfg)->reg_cfg;
				}
				else {
					temp_reg_cfg->next = (REGISTER_CFG *) malloc(sizeof(REGISTER_CFG));
					temp_reg_cfg = temp_reg_cfg->next;
					temp_reg_cfg->write_type.reg_write_type1 = NULL;
				}

				ASSERT(temp_reg_cfg != NULL);
				temp_reg_cfg->next = NULL;
				temp_reg_cfg->write_type.reg_write_type0 = NULL;
				
				temp_reg_cfg->version_mask = version_mask; // copy the version mask, if u have new set
				token = (char *) strtok(NULL, delimiters);
				ASSERT(token != NULL);
				temp_reg_cfg->reg_hdr.field0.pack0.type = atoi(token);	

				token = (char *) strtok(NULL, delimiters);
				ASSERT(token != NULL);
				sscanf(token, "%x", &temp_value);
//    				if ( temp_value & RESERVED_MODE_BIT_MASK) {
//	    				printf("WARNING:*** Reserved mode bits set, thus clearing the reserved bits  *** \n");
//					temp_value = temp_value & ~RESERVED_MODE_BIT_MASK;
//    				}
				temp_reg_cfg->reg_hdr.field0.pack0.reg_modality_mask = temp_value;
		    

				token = (char *) strtok(NULL, delimiters);
				ASSERT(token != NULL);
				temp_reg_cfg->reg_hdr.field0.pack0.stage = atoi(token);	

				token = (char *) strtok(NULL, delimiters);
				ASSERT(token != NULL);
				sscanf(token, "%x", &temp_value);
				temp_reg_cfg->reg_hdr.field1.channel_modifier = temp_value;
				temp_reg_cfg->reg_hdr.field0.pack0.channel_modifier_present = (temp_reg_cfg->reg_hdr.field1.channel_modifier?1:0);

				token = (char *) strtok(NULL, delimiters);
				ASSERT(token != NULL);
				sscanf(token, "%x", &temp_value);
				temp_reg_cfg->reg_hdr.field2.disabler_mask = temp_value;
				temp_reg_cfg->reg_hdr.field0.pack0.disabler_present =  (temp_reg_cfg->reg_hdr.field2.disabler_mask?1:0);

				token = (char *) strtok(NULL, delimiters);
				ASSERT(token != NULL);
				sscanf(token, "%x", &temp_value);
				temp_reg_cfg->reg_hdr.pll_value = temp_value;

				temp_reg_cfg->reg_hdr.field0.pack0.bit15 = 0;

				break;
			case REGISTER_WRITES_TYPE: // register data
				switch(temp_reg_cfg->reg_hdr.field0.pack0.type) {
					case WRITE_TYPE0:
						if (temp_reg_cfg->write_type.reg_write_type0 == NULL) {
							temp_reg_cfg->write_type.reg_write_type0 = (REGISTER_WRITE_TYPE0 *) malloc(sizeof(REGISTER_WRITE_TYPE0));
							temp_reg_write_type0 = temp_reg_cfg->write_type.reg_write_type0;
						}
						else {
							temp_reg_write_type0->next = (REGISTER_WRITE_TYPE0 *) malloc(sizeof(REGISTER_WRITE_TYPE0));
							temp_reg_write_type0 = temp_reg_write_type0->next;
						}
						ASSERT(temp_reg_write_type0 != NULL);
						temp_reg_write_type0->next = NULL;

						token = (char *) strtok(NULL, delimiters);
						ASSERT(token != NULL);
						temp_reg_write_type0->field0.pack.tag = atoi(token);

						token = (char *) strtok(NULL, delimiters);
						ASSERT(token != NULL);
						sscanf(token, "%x", &temp_value);
						temp_reg_write_type0->field0.pack.address = temp_value>>2;
						//printf("TYPE0:Address parsed = %x\n", temp_reg_write_type0->field0.pack.address<<2);

						token = (char *) strtok(NULL, delimiters);
						ASSERT(token != NULL);
						sscanf(token, "%x", &temp_reg_write_type0->msw);

						if (temp_reg_write_type0->field0.pack.tag == 0 || temp_reg_write_type0->field0.pack.tag == 3) {
						    token = (char *) strtok(NULL, delimiters);
						    ASSERT(token != NULL);
						    sscanf(token, "%x", &temp_reg_write_type0->lsw);
						}
						break;
					case WRITE_TYPE1:
						if (temp_reg_cfg->write_type.reg_write_type1 == NULL) {
							temp_reg_cfg->write_type.reg_write_type1 = (REGISTER_WRITE_TYPE1 *) malloc(sizeof(REGISTER_WRITE_TYPE1));
							temp_reg_write_type1 = temp_reg_cfg->write_type.reg_write_type1;
						}
						else {
							temp_reg_write_type1->next = (REGISTER_WRITE_TYPE1 *) malloc(sizeof(REGISTER_WRITE_TYPE1));
							temp_reg_write_type1 = temp_reg_write_type1->next;
						}

						ASSERT(temp_reg_write_type1 != NULL);
						temp_reg_write_type1->next = NULL;
						token = (char *) strtok(NULL, delimiters);
						ASSERT(token != NULL);
						temp_value = atoi(token);
						ASSERT(temp_value != 0);
						temp_reg_write_type1->field0.pack.num = temp_value - 1;

						token = (char *) strtok(NULL, delimiters);
						ASSERT(token != NULL);
						sscanf(token, "%x", &temp_value);
						temp_reg_write_type1->field0.pack.address = temp_value>>2;

						for(iIndex=0; iIndex<=temp_reg_write_type1->field0.pack.num; iIndex++) {

						   token = (char *) strtok(NULL, delimiters);
						   ASSERT(token != NULL);
						   sscanf(token, "%x", &temp_type1_value);
						   temp_reg_write_type1->data_msw[iIndex] = temp_type1_value;

						   token = (char *) strtok(NULL, delimiters);
						   ASSERT(token != NULL);
						   sscanf(token, "%x", &temp_type1_value);
						   temp_reg_write_type1->data_lsw[iIndex] = temp_type1_value;

						}
						break;
					case WRITE_TYPE2:
						if (temp_reg_cfg->write_type.reg_write_type2 == NULL) {
							temp_reg_cfg->write_type.reg_write_type2 = (REGISTER_WRITE_TYPE2 *) malloc(sizeof(REGISTER_WRITE_TYPE2));
							temp_reg_write_type2 = temp_reg_cfg->write_type.reg_write_type2;
						}
						else {
							temp_reg_write_type2->next = (REGISTER_WRITE_TYPE2 *) malloc(sizeof(REGISTER_WRITE_TYPE2));
							temp_reg_write_type2 = temp_reg_write_type2->next;
						}
						ASSERT(temp_reg_write_type2 != NULL);
						temp_reg_write_type2->next = NULL;

						token = (char *) strtok(NULL, delimiters);
						ASSERT(token != NULL);
						temp_reg_write_type2->field0.pack0.last = atoi(token);

						token = (char *) strtok(NULL, delimiters);
						ASSERT(token != NULL);
						temp_reg_write_type2->field0.pack0.analog_bank = atoi(token);

						token = (char *) strtok(NULL, delimiters);
						ASSERT(token != NULL);
						temp_reg_write_type2->field0.pack0.column = atoi(token);

						token = (char *) strtok(NULL, delimiters);
						ASSERT(token != NULL);
						temp_end_bit = atoi(token);

						token = (char *) strtok(NULL, delimiters);
						ASSERT(token != NULL);
						temp_reg_write_type2->field0.pack0.start_bit = atoi(token);

						temp_num_bits = temp_end_bit - temp_reg_write_type2->field0.pack0.start_bit + 1;

						if (temp_num_bits > 12) {
							temp_reg_write_type2->num_bits = temp_num_bits;
							temp_reg_write_type2->field0.pack0.extended = 1;
							temp_reg_write_type2->num_data = A_DIV_UP(temp_num_bits, 16);
							temp_reg_write_type2->data = (A_UINT16 *) malloc(temp_num_bits * sizeof(A_UINT16));
							iIndex=0;
							while(iIndex<temp_reg_write_type2->num_data) {
								token = (char *) strtok(NULL, delimiters);
								ASSERT(token != NULL);
								sscanf(token, "%x", &temp_value);
								temp_reg_write_type2->data[iIndex++] = temp_value;
							}
						}
						else {
							temp_reg_write_type2->field1.pack1.num_bits = temp_num_bits;
							temp_reg_write_type2->field0.pack0.extended = 0;

							token = (char *) strtok(NULL, delimiters);
							ASSERT(token != NULL);
							sscanf(token, "%x", &temp_value);
							temp_reg_write_type2->field1.pack1.data = temp_value;
						}

						break;
					case WRITE_TYPE3:
						if (temp_reg_cfg->write_type.reg_write_type3 == NULL) {
							temp_reg_cfg->write_type.reg_write_type3 = (REGISTER_WRITE_TYPE3 *) malloc(sizeof(REGISTER_WRITE_TYPE3));
							temp_reg_write_type3 = temp_reg_cfg->write_type.reg_write_type3;
						}
						else {
							temp_reg_write_type3->next = (REGISTER_WRITE_TYPE3 *) malloc(sizeof(REGISTER_WRITE_TYPE3));
							temp_reg_write_type3 = temp_reg_write_type3->next;
						}
						ASSERT(temp_reg_write_type3 != NULL);
						temp_reg_write_type3->next = NULL;

						token = (char *) strtok(NULL, delimiters);
						ASSERT(token != NULL);
						temp_reg_write_type3->field0.pack.last = atoi(token);

						token = (char *) strtok(NULL, delimiters);
						ASSERT(token != NULL);
						temp_reg_write_type3->field0.pack.opcode = atoi(token);
						//printf("Opcode read = %x\n", temp_reg_write_type3->field0.pack.opcode);
						//printf("header value = %x\n", temp_reg_write_type3->field0.value);
						temp_reg_write_type3->field0.pack.bit13 = 0;
						temp_reg_write_type3->field0.pack.bit14 = 0;

						token = (char *) strtok(NULL, delimiters);
						ASSERT(token != NULL);
						temp_end_bit = atoi(token);

						token = (char *) strtok(NULL, delimiters);
						ASSERT(token != NULL);
						temp_reg_write_type3->field0.pack.start_bit = atoi(token);
						temp_reg_write_type3->field0.pack.num_bits = temp_end_bit - temp_reg_write_type3->field0.pack.start_bit + 1;
//FJC						printf("header value = %x\n", temp_reg_write_type3->field0.value);

						token = (char *) strtok(NULL, delimiters);
						ASSERT(token != NULL);
						sscanf(token, "%x", &temp_reg_write_type3->address);

						token = (char *) strtok(NULL, delimiters);
						ASSERT(token != NULL);
						sscanf(token, "%x", &temp_reg_write_type3->data_msw);

						if (temp_reg_write_type3->field0.pack.num_bits > 16) {
						    token = (char *) strtok(NULL, delimiters);
						    ASSERT(token != NULL);
						    sscanf(token, "%x", &temp_reg_write_type3->data_lsw);
						}

						break;

				}
				break;
			case VERSION_MASK_TYPE: // version mask
				token = (char *) strtok(NULL, delimiters);
				sscanf(token, "%x", &version_mask);
				version_mask |= (1<<15);
				version_mask_given = 1;
				break;
			case VERSION_ID_TYPE: // Versiod id
				token = (char *) strtok(NULL, delimiters);
				//sscanf(token, "%x", &(*ear_cfg)->version_id);
				sscanf(token, "%x", *ear_cfg);
				break;
		}
	}

	fclose(fileStream);
	return 0;

}

void printModeString(A_UINT16 modes) {

    A_CHAR *pModesArray[9] = {"11B ", "11G ", "T2 ", "11GXR ", "11A ", "NA ", "T5 ", "11AXR ", "NA "};
    int i;
    int newModes;

    if (modes == ALL_MODES) {
        newModes = 0xDF;
    } else {
        newModes = modes;
    }

    for(i = 0; i < 9; i++) {
        if (newModes & (1 << i)) {
            printf("%s", pModesArray[i]);
        }
    }
    printf("\n");
}

/* returns number of ear locations created 
 * returns -1 upon failure */

A_INT32 createEAR( EAR_CFG *ear_cfg, A_UINT32 ear[]) {
	A_UINT32 iIndex, numIndex=0;
	A_UINT16 prev_version_mask;
	REGISTER_CFG *temp_reg_cfg=NULL;
	REGISTER_WRITE_TYPE0 *temp_reg_write_type0=NULL;
	REGISTER_WRITE_TYPE1 *temp_reg_write_type1=NULL;
	REGISTER_WRITE_TYPE2 *temp_reg_write_type2=NULL;
	REGISTER_WRITE_TYPE3 *temp_reg_write_type3=NULL;

	ASSERT(ear_cfg != NULL);
	ASSERT(ear_cfg->reg_cfg != NULL);

	
	ear[numIndex++] =  ear_cfg->version_id;
	temp_reg_cfg = ear_cfg->reg_cfg;
	ear[numIndex++] = temp_reg_cfg->version_mask;
	prev_version_mask=temp_reg_cfg->version_mask;

	while(temp_reg_cfg) {
		//Copy version mask
		if (prev_version_mask != temp_reg_cfg->version_mask) {
			ear[numIndex++] = temp_reg_cfg->version_mask;
			prev_version_mask=temp_reg_cfg->version_mask;
		}
		// Copy register header
		// copy individual bit to respective positions so as to solve endian problem due to union
		ear[numIndex++] = (temp_reg_cfg->reg_hdr.field0.pack0.bit15 << 15)  \
			| (temp_reg_cfg->reg_hdr.field0.pack0.disabler_present << 14) 	\
			| (temp_reg_cfg->reg_hdr.field0.pack0.channel_modifier_present << 13) 	\
			| (temp_reg_cfg->reg_hdr.field0.pack0.stage << 11) 	\
			| (temp_reg_cfg->reg_hdr.field0.pack0.type << 9) 	\
			| (temp_reg_cfg->reg_hdr.field0.pack0.reg_modality_mask); 

//		printf("reg_hdr:%x:ear=%x:\n", temp_reg_cfg->reg_hdr.field0.value, ear[numIndex-1]);
		if (temp_reg_cfg->reg_hdr.field0.pack0.channel_modifier_present) {
			ear[numIndex++] = (temp_reg_cfg->reg_hdr.field1.pack1.bit15 << 15) \
				| temp_reg_cfg->reg_hdr.field1.pack1.bit0_14;
		}
		if (temp_reg_cfg->reg_hdr.field0.pack0.disabler_present) {
			ear[numIndex++] = temp_reg_cfg->reg_hdr.field2.disabler_mask;
			if (temp_reg_cfg->reg_hdr.field2.pack2.pll) {
				ear[numIndex++] = temp_reg_cfg->reg_hdr.pll_value;
			}
		}
		// Copy register writes
		switch(temp_reg_cfg->reg_hdr.field0.pack0.type) {
			case WRITE_TYPE0:
				temp_reg_write_type0 = temp_reg_cfg->write_type.reg_write_type0;
				while(temp_reg_write_type0) {
					ear[numIndex++] = ((temp_reg_write_type0->field0.pack.address << 2) | (temp_reg_write_type0->field0.pack.tag));
					ear[numIndex++] = temp_reg_write_type0->msw;
					if (temp_reg_write_type0->field0.pack.tag == 0 || temp_reg_write_type0->field0.pack.tag == 3) {
						ear[numIndex++] = temp_reg_write_type0->lsw;
					}
					temp_reg_write_type0 = temp_reg_write_type0->next;
				}
				break;
			case WRITE_TYPE1:
				temp_reg_write_type1 = temp_reg_cfg->write_type.reg_write_type1;
				while(temp_reg_write_type1) {
					ear[numIndex++] = ((temp_reg_write_type1->field0.pack.address << 2) | (temp_reg_write_type1->field0.pack.num));
					for(iIndex=0;iIndex<=temp_reg_write_type1->field0.pack.num; iIndex++) {
						ear[numIndex++] = temp_reg_write_type1->data_msw[iIndex];
						ear[numIndex++] = temp_reg_write_type1->data_lsw[iIndex];
					}
					temp_reg_write_type1 = temp_reg_write_type1->next;
				}
				break;
			case WRITE_TYPE2:
				temp_reg_write_type2 = temp_reg_cfg->write_type.reg_write_type2;
				while(temp_reg_write_type2) {
					ear[numIndex++] = ((temp_reg_write_type2->field0.pack0.analog_bank << 13) \
								| (temp_reg_write_type2->field0.pack0.last << 12) \
								| (temp_reg_write_type2->field0.pack0.column << 10) \
								| (temp_reg_write_type2->field0.pack0.extended << 9) \
								| (temp_reg_write_type2->field0.pack0.start_bit));
					if (temp_reg_write_type2->field0.pack0.extended) {
						ear[numIndex++] = temp_reg_write_type2->num_bits;
						for(iIndex=0; iIndex<temp_reg_write_type2->num_data; iIndex++) {
							ear[numIndex++] = temp_reg_write_type2->data[iIndex];
						}
					}
					else {
						ear[numIndex++] = ((temp_reg_write_type2->field1.pack1.num_bits << 12) | (temp_reg_write_type2->field1.pack1.data));
					}
					temp_reg_write_type2 = temp_reg_write_type2->next;
				}
				break;
			case WRITE_TYPE3:
				temp_reg_write_type3 = temp_reg_cfg->write_type.reg_write_type3;
				while(temp_reg_write_type3) {
					ear[numIndex++] = ((temp_reg_write_type3->field0.pack.last << 15) \
								| (temp_reg_write_type3->field0.pack.bit14 << 14) \
								| (temp_reg_write_type3->field0.pack.bit13 << 13) \
								| (temp_reg_write_type3->field0.pack.opcode << 10) \
								| (temp_reg_write_type3->field0.pack.start_bit << 5) \
								| (temp_reg_write_type3->field0.pack.num_bits));
					ear[numIndex++] = temp_reg_write_type3->address;
					ear[numIndex++] = temp_reg_write_type3->data_msw;
					if (temp_reg_write_type3->field0.pack.num_bits>16) {
						ear[numIndex++] = temp_reg_write_type3->data_lsw;
					}
					temp_reg_write_type3 = temp_reg_write_type3->next;
				}
				break;
		}
		temp_reg_cfg = temp_reg_cfg -> next;
	}

	ear[numIndex++] = 0;
	return numIndex;

}

void freeup_earcfg(EAR_CFG *ear_cfg) {
    REGISTER_CFG *temp_reg_cfg = NULL;
    REGISTER_WRITE_TYPE0 *temp_reg_write_type0=NULL;
    REGISTER_WRITE_TYPE1 *temp_reg_write_type1=NULL;
    REGISTER_WRITE_TYPE2 *temp_reg_write_type2=NULL;
    REGISTER_WRITE_TYPE3 *temp_reg_write_type3=NULL;
    REGISTER_CFG *temp = NULL;

    temp_reg_cfg = ear_cfg->reg_cfg;

    while(temp_reg_cfg) {
	switch(temp_reg_cfg->reg_hdr.field0.pack0.type) {
		case 0:
			temp_reg_write_type0 = temp_reg_cfg->write_type.reg_write_type0;
			while(temp_reg_write_type0) {
				REGISTER_WRITE_TYPE0 *tempp;
				tempp = (REGISTER_WRITE_TYPE0 *) temp_reg_write_type0;
				temp_reg_write_type0 = temp_reg_write_type0 -> next;
                if (tempp != NULL) {
				    A_FREE(tempp);
					tempp = NULL;
				}

			}
			break;
		case 1:
			temp_reg_write_type1 = temp_reg_cfg->write_type.reg_write_type1;
			while(temp_reg_write_type1) {
				REGISTER_WRITE_TYPE1 *tempp;
				tempp = (REGISTER_WRITE_TYPE1 *) temp_reg_write_type1;
				temp_reg_write_type1 = temp_reg_write_type1 -> next;
                if (tempp != NULL) {
    				A_FREE(tempp);
					tempp = NULL;
				}
			}
			break;
		case 2:
			temp_reg_write_type2 = temp_reg_cfg->write_type.reg_write_type2;
			while(temp_reg_write_type2) {
				REGISTER_WRITE_TYPE2 *tempp;
				tempp = (REGISTER_WRITE_TYPE2 *) temp_reg_write_type2;
				temp_reg_write_type2 = temp_reg_write_type2 -> next;
                if (tempp != NULL) {
				    A_FREE(tempp);
					tempp = NULL;
				}
			}
			break;
		case 3:
			temp_reg_write_type3 = temp_reg_cfg->write_type.reg_write_type3;
			while(temp_reg_write_type3) {
				REGISTER_WRITE_TYPE3 *tempp;
				tempp = (REGISTER_WRITE_TYPE3 *) temp_reg_write_type3;
				temp_reg_write_type3 = temp_reg_write_type3 -> next;
                if (tempp != NULL) {
				    A_FREE(tempp);
					tempp = NULL;
				}
			}
			break;
	}
	temp = (REGISTER_CFG *) temp_reg_cfg;
        temp_reg_cfg = temp_reg_cfg->next;
		if (temp != NULL) {
	A_FREE(temp);
			temp = NULL;
		}
    }

    if (ear_cfg != NULL) {
    A_FREE(ear_cfg);
		ear_cfg = NULL;
	}

}

void displayEar(A_UINT32 ear[MAX_EAR_LOCATIONS], A_UINT32 numLocations) {
	A_UINT32 iIndex=0, jIndex;
	EAR_CFG  *ear_cfg = NULL;
	REGISTER_CFG *temp_reg_cfg = NULL;
	REGISTER_WRITE_TYPE0 *temp_reg_write_type0=NULL;
	REGISTER_WRITE_TYPE1 *temp_reg_write_type1=NULL;
	REGISTER_WRITE_TYPE2 *temp_reg_write_type2=NULL;
	REGISTER_WRITE_TYPE3 *temp_reg_write_type3=NULL;
	A_UINT16 prev_version_mask;

    /* for(jIndex=0; jIndex<numLocations; jIndex++) {
            printf(":%x:", ear[jIndex]);
            if (jIndex % 32 == 0) {printf("\n");}
    }
    */

	ear_cfg = (EAR_CFG *) malloc(sizeof(EAR_CFG));
	ASSERT(ear_cfg != NULL);
	ear_cfg->reg_cfg = NULL;
	ear_cfg->version_id = (A_UINT16) ear[iIndex++];

	uiPrintf(" Version Id = %x\n",ear_cfg->version_id);
	

	if((ear_cfg->version_id == 0x0000 ) || (ear_cfg->version_id == 0xffff))
	{
		uiPrintf("\n There is no Ear Programmed into EEPROM\n");
		return;
	}

	prev_version_mask = (A_UINT16) ear[iIndex]; //First byte after version id should be version mask

	uiPrintf(" prev_version_mask = %x\n",prev_version_mask);

	for(; iIndex<numLocations; ) {
		//printf("iIndex=%d:ear[iIndex]=%x:\n", iIndex, ear[iIndex]);
		if (ear_cfg->reg_cfg == NULL) {
			ear_cfg->reg_cfg = (REGISTER_CFG *) malloc(sizeof(REGISTER_CFG));
			temp_reg_cfg = ear_cfg->reg_cfg;
		}
		else {
			temp_reg_cfg->next = (REGISTER_CFG *) malloc(sizeof(REGISTER_CFG));
			temp_reg_cfg = temp_reg_cfg->next;
		}
		ASSERT(temp_reg_cfg != NULL);
		temp_reg_cfg->version_mask = prev_version_mask;
		temp_reg_cfg->next = NULL;
		temp_reg_cfg->write_type.reg_write_type1 = NULL;
		if (ear[iIndex] & 0x8000) { // its version mask, else register header
		   temp_reg_cfg->version_mask = (A_UINT16) ear[iIndex++];
		}
		if (prev_version_mask != temp_reg_cfg->version_mask) {
			prev_version_mask = temp_reg_cfg->version_mask;
		}
		//Should be register header
		temp_reg_cfg->reg_hdr.field0.value = (A_UINT16) ear[iIndex++];
        //printf("ear[%d]=%x:temp_reg_cfg->reg_hdr.field0.value=%x:\n", (iIndex-1), ear[iIndex-1], temp_reg_cfg->reg_hdr.field0.value);
        //printf("CM=%d:D=%d:pll=%d:\n", temp_reg_cfg->reg_hdr.field0.pack0.channel_modifier_present, temp_reg_cfg->reg_hdr.field0.pack0.disabler_present, temp_reg_cfg->reg_hdr.field2.pack2.pll);
		ASSERT(temp_reg_cfg->reg_hdr.field0.pack0.bit15 == 0);
		if (temp_reg_cfg->reg_hdr.field0.pack0.channel_modifier_present) {
			temp_reg_cfg->reg_hdr.field1.channel_modifier = (A_UINT16) ear[iIndex++];
		}
		if (temp_reg_cfg->reg_hdr.field0.pack0.disabler_present) {
			temp_reg_cfg->reg_hdr.field2.disabler_mask = (A_UINT16) ear[iIndex++];
		}
        else {
			temp_reg_cfg->reg_hdr.field2.disabler_mask = 0;
        }
		if (temp_reg_cfg->reg_hdr.field2.pack2.pll) {
			temp_reg_cfg->reg_hdr.pll_value = (A_UINT16) ear[iIndex++];
		}

		switch(temp_reg_cfg->reg_hdr.field0.pack0.type) {
		case WRITE_TYPE0:
			do {
			//printf("TYPE0:iIndex=%d\n", iIndex);
              		  if (temp_reg_cfg->write_type.reg_write_type0 == NULL) {
				temp_reg_cfg->write_type.reg_write_type0 = (REGISTER_WRITE_TYPE0 *) malloc(sizeof(REGISTER_WRITE_TYPE0));
				temp_reg_write_type0 = temp_reg_cfg->write_type.reg_write_type0;
			  }
			  else {
				temp_reg_write_type0->next = (REGISTER_WRITE_TYPE0 *) malloc(sizeof(REGISTER_WRITE_TYPE0));
				temp_reg_write_type0 = temp_reg_write_type0->next;
			  }
			  ASSERT(temp_reg_write_type0 != NULL);
			  temp_reg_write_type0->next = NULL;

			  temp_reg_write_type0->field0.value = (A_UINT16) ear[iIndex++];

			  switch(temp_reg_write_type0->field0.pack.tag){
			  case 0:
			  case 3:
				  temp_reg_write_type0->msw = (A_UINT16) ear[iIndex++];
				  temp_reg_write_type0->lsw = (A_UINT16) ear[iIndex++];
				  break;
			  case 1:
			  case 2:
				  temp_reg_write_type0->msw = (A_UINT16) ear[iIndex++];
				  break;
			  }
			  ASSERT(iIndex<numLocations);
			} while(temp_reg_write_type0->field0.pack.tag != 3);
			break;
		case WRITE_TYPE1:
			//printf("TYPE1:iIndex=%d\n", iIndex);
              		if (temp_reg_cfg->write_type.reg_write_type1 == NULL) {
				temp_reg_cfg->write_type.reg_write_type1 = (REGISTER_WRITE_TYPE1 *) malloc(sizeof(REGISTER_WRITE_TYPE1));
				temp_reg_write_type1 = temp_reg_cfg->write_type.reg_write_type1;
			  	temp_reg_write_type1->next = NULL;
			  }
			  ASSERT(temp_reg_write_type1 != NULL);
			  temp_reg_write_type1->field0.value = (A_UINT16) ear[iIndex++];
			  for(jIndex=0; jIndex<=temp_reg_write_type1->field0.pack.num; jIndex++) {
			  	ASSERT(iIndex<numLocations);
				temp_reg_write_type1->data_msw[jIndex] = (A_UINT16) ear[iIndex++];
				temp_reg_write_type1->data_lsw[jIndex] = (A_UINT16) ear[iIndex++];
			  }
			break;
		case WRITE_TYPE2:
			//printf("TYPE2:iIndex=%d\n", iIndex);
			do {
              		  if (temp_reg_cfg->write_type.reg_write_type2 == NULL) {
				temp_reg_cfg->write_type.reg_write_type2 = (REGISTER_WRITE_TYPE2 *) malloc(sizeof(REGISTER_WRITE_TYPE2));
				temp_reg_write_type2 = temp_reg_cfg->write_type.reg_write_type2;
			  }
			  else {
					temp_reg_write_type2->next = (REGISTER_WRITE_TYPE2 *) malloc(sizeof(REGISTER_WRITE_TYPE2));
					temp_reg_write_type2 = temp_reg_write_type2->next;
			  }
			  ASSERT(temp_reg_write_type2 != NULL);
			  temp_reg_write_type2->next = NULL;

 			  temp_reg_write_type2->field0.value = (A_UINT16) ear[iIndex++];
  
			  switch(temp_reg_write_type2->field0.pack0.extended){
			    case 0:
			      temp_reg_write_type2->field1.value = (A_UINT16) ear[iIndex++];	  
				break;
			    case 1:
				  temp_reg_write_type2->num_bits = (A_UINT16) ear[iIndex++];
				  temp_reg_write_type2->num_data = A_DIV_UP(temp_reg_write_type2->num_bits, 16);
				  temp_reg_write_type2->data = (A_UINT16 *) malloc(sizeof (A_UINT16) * temp_reg_write_type2->num_data);
				  for(jIndex=0; jIndex<temp_reg_write_type2->num_data; jIndex++) {
			  		  ASSERT(iIndex<numLocations);
					  temp_reg_write_type2->data[jIndex] = (A_UINT16)ear[iIndex++];
				  }
				break;
			  }
			  ASSERT(iIndex<numLocations);
			}while(!temp_reg_write_type2->field0.pack0.last);
			break;
		case WRITE_TYPE3:
			//printf("TYPE3:iIndex=%d\n", iIndex);
			do {
              		  if (temp_reg_cfg->write_type.reg_write_type3 == NULL) {
				temp_reg_cfg->write_type.reg_write_type3 = (REGISTER_WRITE_TYPE3 *) malloc(sizeof(REGISTER_WRITE_TYPE3));
				temp_reg_write_type3 = temp_reg_cfg->write_type.reg_write_type3;
			  }
			  else {
					temp_reg_write_type3->next = (REGISTER_WRITE_TYPE3 *) malloc(sizeof(REGISTER_WRITE_TYPE3));
					temp_reg_write_type3 = temp_reg_write_type3->next;
			  }
			  ASSERT(temp_reg_write_type3 != NULL);
			  temp_reg_write_type3->next = NULL;

 			  temp_reg_write_type3->field0.value = (A_UINT16) ear[iIndex++];
  
			  temp_reg_write_type3->address = (A_UINT16) ear[iIndex++];
			  temp_reg_write_type3->data_msw = (A_UINT16) ear[iIndex++];
			  if (temp_reg_write_type3->field0.pack.num_bits > 16) {
				  temp_reg_write_type3->data_lsw = (A_UINT16) ear[iIndex++];
			  }
			  ASSERT(iIndex<numLocations);
			}while(!temp_reg_write_type3->field0.pack.last);
			break;
		}
		if (!ear[iIndex]) break;
	}
	printParsedFormattedData(ear_cfg);
	freeup_earcfg(ear_cfg);
	
}

void printParsedFormattedData(EAR_CFG *ear_cfg) {
    REGISTER_CFG *temp_reg_cfg = NULL;
    REGISTER_WRITE_TYPE0 *temp_reg_write_type0=NULL;
    REGISTER_WRITE_TYPE1 *temp_reg_write_type1=NULL;
    REGISTER_WRITE_TYPE2 *temp_reg_write_type2=NULL;
    REGISTER_WRITE_TYPE3 *temp_reg_write_type3=NULL;
    A_UINT32 iIndex;
    A_UINT16 prev_version_mask;
    char *tag_str[] = {"32 bit data", "lower 16 bit data", "upper 16 bit data", "32 bit data and last write in the block"};


    temp_reg_cfg = ear_cfg->reg_cfg;

    uiPrintf("******  Version Id = %x  ****** \n", ear_cfg->version_id);	
    uiPrintf("******  Version Mask = %x  ******  \n", temp_reg_cfg->version_mask);	
    prev_version_mask = temp_reg_cfg->version_mask;
    while(temp_reg_cfg) {
	if (prev_version_mask != temp_reg_cfg->version_mask) {
    		uiPrintf("******  Version Mask = %x  ******  \n", temp_reg_cfg->version_mask);	
    		prev_version_mask = temp_reg_cfg->version_mask;
	}
	uiPrintf("Hdr value = %x::", temp_reg_cfg->reg_hdr.field0.value);
	printModeString( (A_UINT16) temp_reg_cfg->reg_hdr.field0.pack0.reg_modality_mask);
	if (temp_reg_cfg->reg_hdr.field0.pack0.channel_modifier_present) {
		uiPrintf("Channel modifier = ");
		if (temp_reg_cfg->reg_hdr.field1.pack1.bit15) {
			uiPrintf("Single channel %0d MHz\n", temp_reg_cfg->reg_hdr.field1.pack1.bit0_14);
		}
		else
		{
			uiPrintf("%x::", temp_reg_cfg->reg_hdr.field1.channel_modifier);
		}
	}
	if (temp_reg_cfg->reg_hdr.field0.pack0.disabler_present) {
		uiPrintf("Disabler = %x -> ", temp_reg_cfg->reg_hdr.field2.disabler_mask);
		if (temp_reg_cfg->reg_hdr.field2.pack2.pll) {
			uiPrintf("PLL  %x\n", temp_reg_cfg->reg_hdr.pll_value);
		}
	}
	switch(temp_reg_cfg->reg_hdr.field0.pack0.type) {
		case 0:
			uiPrintf("TYPE 0\n");
			temp_reg_write_type0 = temp_reg_cfg->write_type.reg_write_type0;
			while(temp_reg_write_type0) {
				uiPrintf("Address = %x::", temp_reg_write_type0->field0.pack.address<<2);
				uiPrintf("%s\n", tag_str[temp_reg_write_type0->field0.pack.tag]);
				uiPrintf("=== >	%x\n", temp_reg_write_type0->msw);
				if (temp_reg_write_type0->field0.pack.tag == 0 || temp_reg_write_type0->field0.pack.tag == 3) {
				   uiPrintf("=== >	%x ", temp_reg_write_type0->lsw);
				}
				uiPrintf("\n");
				temp_reg_write_type0 = temp_reg_write_type0 -> next;
			}
			break;
		case 1:
			uiPrintf("TYPE 1, Group addresses\n");
			temp_reg_write_type1 = temp_reg_cfg->write_type.reg_write_type1;
			while(temp_reg_write_type1) {
				uiPrintf("Number of data = %x\n", temp_reg_write_type1->field0.pack.num);
				uiPrintf("Address = %x\n", temp_reg_write_type1->field0.pack.address<<2);
				for(iIndex=0; iIndex<=temp_reg_write_type1->field0.pack.num; iIndex++) {
				   uiPrintf("Data MSW[%d] = %x \n", iIndex, temp_reg_write_type1->data_msw[iIndex]);
				   uiPrintf("Data LSW[%d] = %x \n", iIndex, temp_reg_write_type1->data_lsw[iIndex]);
				}
				temp_reg_write_type1 = temp_reg_write_type1 -> next;
			}
			break;
		case 2:
			uiPrintf("TYPE 2, Analog registers = ");
			temp_reg_write_type2 = temp_reg_cfg->write_type.reg_write_type2;
			while(temp_reg_write_type2) {
				uiPrintf("%x\n", temp_reg_write_type2->field0.value);
				if (temp_reg_write_type2->field0.pack0.extended) {
				   for(iIndex=0; iIndex<temp_reg_write_type2->num_data; iIndex++) {
				      uiPrintf("=== >	%x\n", temp_reg_write_type2->data[iIndex]);
				   }
				}
				else {
				   uiPrintf("%x\n", temp_reg_write_type2->field1.value);
				}
				temp_reg_write_type2 = temp_reg_write_type2 -> next;
			}
			break;
		case 3:
			uiPrintf("TYPE 3 \n");
			temp_reg_write_type3 = temp_reg_cfg->write_type.reg_write_type3;
			while(temp_reg_write_type3) {
				uiPrintf("Field 0 === >	%x::", temp_reg_write_type3->field0.value);
				uiPrintf("Opcode = %d\n", temp_reg_write_type3->field0.pack.opcode);
				uiPrintf("Address = %x\n", temp_reg_write_type3->address);
				if (temp_reg_write_type3->field0.pack.num_bits > 16) {
					uiPrintf("Data MSW %x\n", temp_reg_write_type3->data_msw);
					uiPrintf("Data LSW %x\n", temp_reg_write_type3->data_lsw);
				}
				else {
					uiPrintf("Data %x\n", temp_reg_write_type3->data_msw);
				}
				temp_reg_write_type3 = temp_reg_write_type3 -> next;
			}
			break;
	}
        temp_reg_cfg = temp_reg_cfg->next;
    }

}

int parseLoadEar(char *ear_cfg_filename, A_UINT32 ear[MAX_EAR_LOCATIONS], A_UINT32 *numlocations, int debug)
{

	EAR_CFG *ear_cfg = NULL;
	A_UINT32 iIndex;

	ear_cfg = (EAR_CFG *) malloc(sizeof(EAR_CFG));
	ASSERT(ear_cfg != NULL);
	ear_cfg->reg_cfg = NULL;
	if (parseEarCfgFile(ear_cfg_filename, &ear_cfg) != -1) {
	   if (debug) printf("creating EAR \n");
	   *numlocations = createEAR(ear_cfg, ear);
	   if (debug) printf("created EAR \n");
	   if (debug) {
	       printf("printing parsed file \n");
	       printf("Number of EAR locations = %d\n", *numlocations);
	       printParsedFormattedData(ear_cfg);
	       printf("EAR words are \n");
	       for(iIndex=0; iIndex<*numlocations; iIndex++) {
	   	    printf("%x\n", ear[iIndex]);
	       }
	   }

	   freeup_earcfg(ear_cfg);
	   ear_cfg=NULL;
//		 uiPrintf("\nThe Number of Ear Location = %d",*numlocations);
	   return *numlocations;
 	}	
	else {
		return -1;
	}
}
