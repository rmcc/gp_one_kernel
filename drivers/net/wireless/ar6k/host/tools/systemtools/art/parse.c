/* parse.c - parsing of eep file */
/* Copyright (c) 2002 Atheros Communications, Inc., All Rights Reserved */

#ifdef __ATH_DJGPPDOS__
	#define __int64	long long
 	#define HANDLE long
 	typedef unsigned long DWORD;
 	#define Sleep	delay
 	#include <bios.h>
#endif	// #ifdef __ATH_DJGPPDOS__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "wlantype.h"
#include "wlanproto.h"
#include "athreg.h"
#include "manlib.h"
#include "dynArray.h"
#include "test.h"
#ifdef JUNGO
#include "mld.h"
#endif
#include "common_hw.h"
#ifdef __ATH_DJGPPDOS__
#include "mlibif_dos.h"
#endif
#include "art_if.h"
#ifndef _MLD 
#ifndef __ATH_DJGPPDOS__
#include "diag.i"
#endif
#endif

// non-ansi functions are mapped to some equivalent functions
#if defined(LINUX) || defined(__linux__)
#include "linux_ansi.h"
#endif

static A_BOOL parseEepFile (char *filename, DYNAMIC_ARRAY *pCommonInfo, DYNAMIC_ARRAY *pModeInfo,
							A_UINT32      *pPerforceVersion);

static A_UINT32 getPerforceVersion(A_CHAR *Buffer);

static char delimiters[]   = " \t\n\r";
extern A_UINT32 swDeviceID;


/**************************************************************************
* parseEepFile - Parse the Eep file register section
*
* Returns: TRUE if file parsed successfully, FALSE otherwise.
*/
A_BOOL
parseEepFile
(
 char *filename,
 DYNAMIC_ARRAY *pCommonInfo,
 DYNAMIC_ARRAY *pModeInfo,
 A_UINT32      *pPerforceVersion
)
{
    FILE        *fileStream;
    A_CHAR      lineBuffer[MAX_FILE_WIDTH];
    A_CHAR      *token;
    A_UINT32    currentLineNumber = 0;
    A_BOOL      returnValue = 1;
    A_BOOL      fileError = 0;
    A_UINT16    i;
    PARSE_FIELD_INFO    fieldInfoIn;
    PARSE_FIELD_INFO    specialFieldInfo;
    PARSE_MODE_INFO    modeFieldInfoIn;
//    MODE_INFO    *pModeFieldDetails = NULL;
    A_BOOL		modeSpecific = 0;
    A_UINT32	cfgVersion = 0;
    A_BOOL		configStart = 0;
    A_BOOL		ignoreField = FALSE;
    A_UINT32    perforceVersion;


    specialFieldInfo.fieldName[0] = '\0';
    //open file
    fileStream = fopen(filename, "r");

    if (NULL == fileStream) {
        uiPrintf("Unable to open atheros text file - %s\n", filename);
        return 0;
    }

    memset(&fieldInfoIn, 0, sizeof(PARSE_FIELD_INFO));
    memset(&modeFieldInfoIn, 0, sizeof(PARSE_MODE_INFO));

    
    //start reading lines and parsing info
    while (!fileError) {
        ignoreField = FALSE;
        if (fgets( lineBuffer, MAX_FILE_WIDTH, fileStream ) == NULL) {
            break;
        }

        currentLineNumber++;
        //check for this being a comment line
        if (lineBuffer[0] == '#') {
            //comment line

	    //look for the perforce version information
	    if(strncmp("#$Id", lineBuffer, strlen("#$Id")) == 0) {
                perforceVersion = getPerforceVersion(lineBuffer);
	        if(perforceVersion == 0) {
		    uiPrintf("Unable to get the perforce version number from EEP file\n");
		    fclose(fileStream);
		    return 0;
		}
		*pPerforceVersion = perforceVersion;
            }
            continue;
        }

        //extract value from the line
        token = strtok( lineBuffer, delimiters );
        if (NULL == token) {
            //blank line
            continue;
        }

        //process command lines
        if(token[0] == '@') {
            if(strnicmp("@MODE:", token, strlen("@MODE:")) == 0) {
	        token = strtok( NULL, delimiters ); //get MODE type
	        if(NULL == token) {
		    uiPrintf("Bad @MODE syntax at line %d\n", currentLineNumber);
		    fclose(fileStream);
		    return 0;
		}
		        
		/*
		* for now only care about finding the mode specific section, 
		* ignore everything else
		*/
		if(strnicmp("MODE_SPECIFIC", token, strlen("MODE_SPECIFIC")) == 0) {
		    modeSpecific = 1;
		}
				
            }

	    else if(strnicmp("@config_section_begin", token, strlen("@config_section_begin")) == 0) {
	        configStart = 1;
	    }

	    else if(strnicmp("@config_section_end", token, strlen("@config_section_end")) == 0) {
	        configStart = 0;
	    }

	    //continue and get next line
	    continue;
			
	}

	if (!configStart) {
	    continue;
	}

        for(i = 0; (i < 2) && !fileError && !modeSpecific; i++) {

            switch(i) {
            case 0:
                if(strlen(token) > MAX_NAME_LENGTH) {
                    uiPrintf("field name on line is more than %d characters\n", 
                                    currentLineNumber, MAX_NAME_LENGTH);
                    fileError = 1;
                    break;
                }
 
                strcpy(fieldInfoIn.fieldName, token);
                break;

            case 1:
                if(strlen(token) > MAX_VALUE_LENGTH) {
                    uiPrintf("Register value on line is more than %d characters\n", 
                                    currentLineNumber, MAX_VALUE_LENGTH);
                    fileError = 1;
                    break;
                }

                //pass the value down as a string, need the signed and field size info
		//to be able to parse it correctly
		strcpy(fieldInfoIn.valueString, token);
                break;
            }

            //get the next token
            if (!fileError && (i != 1)) {
                token = strtok( NULL, delimiters );
                if (NULL == token) {
                    //bad line give an error and get out
                    uiPrintf("Incomplete line at line number %d\n", currentLineNumber);
                    fileError = 1;
                }
            }
        }

			
        if (modeSpecific) {
	    //parse mode specific section
	    for(i = FIELD_NAME; (i <= TURBO_11G_VALUE) && !fileError; i++) {
                switch(i) {
                case FIELD_NAME:
		    if(strlen(token) > MAX_NAME_LENGTH) {
		        uiPrintf("field name on line is more than %d characters\n", currentLineNumber, MAX_NAME_LENGTH);
			fileError = 1;
			break;
	            }

		    strcpy(modeFieldInfoIn.fieldName, token);
		    if((swDeviceID & 0xff) == 0x0014) {
		        if(strcmp(token, "rf_pdgain_lo") == 0) {
		            //for derby 1, this is just xpd_gain
			    strcpy(modeFieldInfoIn.fieldName, "rf_xpd_gain");
			}

			if(strcmp(token, "rf_pdgain_hi") == 0) {
			    //want to ignore this field for derby 1.0
			    ignoreField = TRUE;
			}
                    }
		    break;

		case BASE_11A_VALUE:
		    if(strlen(token) > MAX_VALUE_LENGTH) {
		        uiPrintf("11a Register value on line is more than %d characters\n", currentLineNumber, MAX_VALUE_LENGTH);
	                fileError = 1;
		        break;
		    }

		    //pass the value down as a string, need the signed and field size info
		    //to be able to parse it correctly
		    strcpy(modeFieldInfoIn.value11aStr, token);
		    break;

		case TURBO_11A_VALUE:
		    if(strlen(token) > MAX_VALUE_LENGTH) {
                        uiPrintf("11a turbo Register value on line is more than %d characters\n", 
			currentLineNumber, MAX_VALUE_LENGTH);
			fileError = 1;
			break;
                    }

		    //pass the value down as a string, need the signed and field size info
		    //to be able to parse it correctly
		    strcpy(modeFieldInfoIn.value11aTurboStr, token);
		    break;

		case BASE_11B_VALUE:
                    if(strlen(token) > MAX_VALUE_LENGTH) {
		        uiPrintf("11b Register value on line is more than %d characters\n", currentLineNumber, MAX_VALUE_LENGTH);
                        fileError = 1;
		        break;
		    }

		    //pass the value down as a string, need the signed and field size info
		    //to be able to parse it correctly
		    strcpy(modeFieldInfoIn.value11bStr, token);
		    break;

		case BASE_11G_VALUE:
		    if(strlen(token) > MAX_VALUE_LENGTH) {
		        uiPrintf("11g Register value on line is more than %d characters\n", currentLineNumber, MAX_VALUE_LENGTH);
                        fileError = 1;
		        break;
		    }

		    //pass the value down as a string, need the signed and field size info
		    //to be able to parse it correctly
		    strcpy(modeFieldInfoIn.value11gStr, token);
		    break;

		case TURBO_11G_VALUE:
		    if (NULL == token) {
		        modeFieldInfoIn.value11gTurboStr[0] = '\0';
		    }
		    else {
		        if(strlen(token) > MAX_VALUE_LENGTH) {
			    uiPrintf("11g Turbo Register value on line is more than %d characters\n", currentLineNumber, MAX_VALUE_LENGTH);
                            fileError = 1;
			    break;
			}

			//pass the value down as a string, need the signed and field size info
			//to be able to parse it correctly
			strcpy(modeFieldInfoIn.value11gTurboStr, token);
                    }
		    break;
		} //end switch

		//get the next token
		if (!fileError && (i != TURBO_11G_VALUE)) {
		    token = strtok( NULL, delimiters );
		    if ((NULL == token) && (i != BASE_11G_VALUE)) {  //make the 11g turbo value optional for now
		        //bad line give an error and get out
		        uiPrintf("Incomplete line at line number %d, in mode specific section\n", currentLineNumber);
		        fileError = 1;
		    }
		}
	    } //end for
        }
				
        if((!fileError) && (!ignoreField)){
            if(!modeSpecific) {
	        //add to common array to pass to library
		if(!addElement(pCommonInfo, (void *)&fieldInfoIn)) {
		    uiPrintf("Unable to add a new element to common info array\n");
		    return 0;
		}
	    }
	    else {
	        //add to mode specific array to pass to library
	        if(!addElement(pModeInfo, (void *)&modeFieldInfoIn)) {
	            uiPrintf("Unable to add a new element to mode info array\n");
		    return 0;
		}

	    }
        }
    } //end while !fileError

    if (fileError) {
        returnValue = 0;
    }
    fclose(fileStream);
    return(returnValue);
}

A_BOOL
processEepFile
(
 A_UINT32 devNum,
 char *filename,
 A_UINT32 *pPerforceVersion
)
{
	DYNAMIC_ARRAY *pCommonInfo=NULL;
	DYNAMIC_ARRAY *pModeInfo=NULL;


	//create the arrays
	pCommonInfo = createArray(sizeof(PARSE_FIELD_INFO));

	if(!pCommonInfo) {
		uiPrintf("Unable to create array to hold common register values\n");
		return FALSE;
	}

	pModeInfo = createArray(sizeof(PARSE_MODE_INFO));

	if(!pModeInfo) {
		uiPrintf("Unable to create array to hold mode specific register values\n");
		freeArray(pCommonInfo);
		return FALSE;
	}

	if(!parseEepFile(filename, pCommonInfo, pModeInfo, pPerforceVersion)) {
		uiPrintf("Unable to parse eep file\n");
		freeArray(pCommonInfo);
		freeArray(pModeInfo);
		return FALSE;
	}

	// call into the library to set the register values
#if defined (_MLD) || defined (__ATH_DJGPPDOS__)	
	art_changeMultipleFields(devNum, (PARSE_FIELD_INFO *)(pCommonInfo->pElements), pCommonInfo->numElements);
	art_changeMultipleFieldsAllModes(devNum, (PARSE_MODE_INFO *)(pModeInfo->pElements), pModeInfo->numElements);
#else
	m_changeMultipleFields(devNum, (PARSE_FIELD_INFO *)(pCommonInfo->pElements), pCommonInfo->numElements);
	m_changeMultipleFieldsAllModes(devNum, (PARSE_MODE_INFO *)(pModeInfo->pElements), pModeInfo->numElements);
#endif

#ifdef _IQV
		freeArray(pCommonInfo);
		freeArray(pModeInfo);
#endif	// _IQV

	return TRUE;
}


/**************************************************************************
* getPerforceVersion - Parse a perforce string to get the version number
*
* Returns: 0 if not successful, otherwise perforce version.
*/
A_UINT32
getPerforceVersion
(
 A_CHAR *buffer
)
{
	A_UINT32 perforceVersion = 0;
	A_CHAR *token;
	A_UINT32 i;

	//get part the first few chars
	buffer+= sizeof("#$Id:");

	for(i = 0; i < strlen(buffer); i++) {
		if(buffer[i] == '#') {
			//perforce version is right after this.
			break;
		}
	}

	if (i == strlen(buffer)) {
		return 0;
	}

    token = strtok( &buffer[i + 1], delimiters );
	perforceVersion = atoi(token);
	return(perforceVersion);
}

/**************************************************************************
* getEarPerforceVersion - Get the perforce version number from ear file
*
* Returns: 0 if not successful, otherwise perforce version.
*/
A_UINT32
getEarPerforceVersion
(
 char *earFile
)
{
	FILE *fileStream;
	A_UINT32 perforceVersion = 0;
    A_CHAR      lineBuffer[MAX_FILE_WIDTH];

	fileStream = (FILE *)fopen(earFile, "r");

	if(fileStream == NULL) {
		uiPrintf("Unable to open ear file %s\n", earFile);
		return 0;
	}

    
	while(1) {
		if (fgets( lineBuffer, MAX_FILE_WIDTH, fileStream ) == NULL) {
			uiPrintf("Unable to get perforce version number from %s\n", earFile);
			fclose(fileStream);
			return 0;
		}

		//look for the perforce version information
		if(strncmp("#$Id", lineBuffer, strlen("#$Id")) == 0) {
			perforceVersion = getPerforceVersion(lineBuffer);
			fclose(fileStream);
			return perforceVersion;
		}
	}

	return 0;
}

/**************************************************************************
* getEarFileIdentifier - Get the file itentifier from the file string
*
* Returns: -1 if not successful, the file identifier.
*/
A_INT32
getEarFileIdentifier
(
 char *filename
)
{
	A_UINT32 i;
	A_CHAR   tempFilename[MAX_FILE_LENGTH];
	A_UINT32 fileIdentifier;

	//take a working copy of the filename
	strncpy(tempFilename, filename, MAX_FILE_LENGTH);
    i = strlen(tempFilename);

    /* // Commented by Siva, as with this we cannot extract the file id with ../../config path
	for(i = 0; i < strlen(tempFilename); i++) {
		//look for the '.' in the filename
		if(tempFilename[i] == '.') {
			break;
		}
	}
	if(i == strlen(tempFilename)) {
		uiPrintf("Error: unable to extract file identifier from ear filename %s (missing .)\n", tempFilename);
		return -1;
	}

	//put null terminator where ',' is
	//tempFilename[i] = '\0';
    */

	//work backwards looking for _
	for( i = i; i > 0; i--) {
		if(tempFilename[i] == '_') {
			break;
		}
	}
	
	if(i == 0) {
		uiPrintf("Error: unable to extract file identifier from ear filename %s(missing _)\n", tempFilename);
		return -1;
	}

	//extract the file identifier from the string.
	if(!sscanf(&tempFilename[i+1], "%x\n", &fileIdentifier)) {
		uiPrintf("Error: unable to extract file identifier from ear filename %s (missing identifier)\n", tempFilename);
		return -1;
	}

	if(fileIdentifier > 0xff) {
		uiPrintf("Error: File identifier is greater than 0xff (%d)\n", fileIdentifier);
		return -1;
	}
	return (A_INT32)fileIdentifier;
}
