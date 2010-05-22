/* ui.c - some generic user interface functions for DK feedback */

/* Copyright (c) 2000 Atheros Communications, Inc., All Rights Reserved */

#ident  "ACI $Id: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/common/ui.c#1 $, $Header: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/common/ui.c#1 $"
static  char *rcsid =  "ACI $Id: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/common/ui.c#1 $, $Header: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/common/ui.c#1 $";

/* 
modification history
--------------------
00a    28apr00    fjc    Created.
*/

/*
DESCRIPTION
This contians some generic user interface functions that will be used for 
display within the perl interpreter.
*/

#include <stdio.h>
#include <stdarg.h>
#include "dk_common.h"

static FILE     *logFile;  /* file handle of logfile */    
static A_BOOL   logging=0;   /* set to 1 if a log file open */
static A_UINT16 quietMode; /* set to 1 for quiet mode, 0 for not */

#if defined(SERIAL_COMN)
A_INT32 uiPrintf
(
	const char *format,
	...
) {

}


A_INT32 q_uiPrintf
(
	const char *format,
	...
) {
}

A_INT16 statsPrintf
(
    	FILE *pFile,
	const char *format,
    	...
) {
}

#else  

/**************************************************************************
* uiPrintf - print to the perl console
*
* This routine is the equivalent of printf.  It is used such that logging
* capabilities can be added.
*
* RETURNS: same as printf.  Number of characters printed
*/
A_INT32 uiPrintf
    (
    const char * format,
    ...
    )
{
    va_list argList;
    int     retval = 0;
    char    buffer[256];

    /*if have logging turned on then can also write to a file if needed */

    /* get the arguement list */
    va_start(argList, format);

    /* using vprintf to perform the printing it is the same is printf, only
     * it takes a va_list or arguments
     */
    retval = vprintf(format, argList);
    fflush(stdout);

    if (logging) {
        vsprintf(buffer, format, argList);
        fputs(buffer, logFile);
    }

    va_end(argList);    /* cleanup arg list */

    return(retval);
}

A_INT32 q_uiPrintf
    (
    const char * format,
    ...
    )
{
    va_list argList;
    int     retval = 0;
    char    buffer[256];

    if ( !quietMode ) {
        va_start(argList, format);

        retval = vprintf(format, argList);
        fflush(stdout);

        if ( logging ) {
            vsprintf(buffer, format, argList);
            fputs(buffer, logFile);
        }

        va_end(argList);    // clean up arg list
    }

    return(retval);
}


/**************************************************************************
* statsPrintf - print to the perl console and to stats file
*
* This routine is the equivalent of printf.  In addition it will write
* to the file in stats format (ie with , delimits).  If the pointer to 
* the file is null, just print to console
*
* RETURNS: same as printf.  Number of characters printed
*/
A_INT16 statsPrintf
    (
    FILE *pFile,
	const char * format,
    ...
    )
{
    va_list argList;
    int     retval = 0;
    char    buffer[256];

    /*if have logging turned on then can also write to a file if needed */

    /* get the arguement list */
    va_start(argList, format);

    /* using vprintf to perform the printing it is the same is printf, only
     * it takes a va_list or arguments
     */
    retval = vprintf(format, argList);
    fflush(stdout);

    if (logging) {
        vsprintf(buffer, format, argList);
        fputs(buffer, logFile);
    }

    if (pFile)
	{
        vsprintf(buffer, format, argList);
        fputs(buffer, pFile);	
		fputc(',', pFile);
	    fflush(pFile);
	}

	va_end(argList);    /* cleanup arg list */

    return(retval);
}

#endif  

/**************************************************************************
* uilog - turn on logging
*
* A user interface command which turns on logging to a fill, all of the
* information printed on screen
*
* RETURNS: 1 if file opened, 0 if not
*/
A_UINT16 uilog
    (
    char *filename        /* name of file to log to */
    )
{
    /* open file for writing */
    logFile = fopen(filename, "w");
    if (logFile == NULL) {
        uiPrintf("Unable to open file %s\n", filename);
        return(0);
    }

    /* set flag to say logging enabled */
    logging = 1;
    return(1);
}

/**************************************************************************
* uilogClose - close the logging file
*
* A user interface command which closes an already open log file
*
* RETURNS: 1 if file opened, 0 if not
*/
void uilogClose(void)
{
    if ( logging ) {
        fclose(logFile);
        logging = 0;
    }

    return;
}

/**************************************************************************
* quiet - set quiet mode on or off
*
* A user interface command which turns quiet mode on or off
*
* RETURNS: N/A
*/
void dk_quiet
    (
    A_UINT16 Mode        // 0 for off, 1 for on
    )
{
    quietMode = Mode;
    return;
}
