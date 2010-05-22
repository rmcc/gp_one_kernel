/*============================================================================
//
// Copyright(c) 2006 Intel Corporation. All rights reserved.
//   All rights reserved.
// 
//   Redistribution and use in source and binary forms, with or without 
//   modification, are permitted provided that the following conditions 
//   are met:
// 
//     * Redistributions of source code must retain the above copyright 
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright 
//       notice, this list of conditions and the following disclaimer in 
//       the documentation and/or other materials provided with the 
//       distribution.
//     * Neither the name of Intel Corporation nor the names of its 
//       contributors may be used to endorse or promote products derived 
//       from this software without specific prior written permission.
// 
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
//   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
//   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
//   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
//   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
//   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
//   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
//   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
//   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
//   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
//   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//  File: tutrace.h
//  Description: This file provides debug message printing support
//
//  Example Usage: 
//  1) TUTRACE((TUTRACE_INFO, "Successfully created TimerThread"
//                   " Handle %X\n", timerThreadHandle));
//  2) TUTRACE((TUTRACE_ERR, "OpenEvent failed.\n"));
//
//===========================================================================*/

#ifndef _TUTRACE_H
#define _TUTRACE_H

#ifdef __cplusplus
extern "C" {
#endif


// include this preprocessor directive in your project/make file
#ifdef _TUDEBUGTRACE

// Set Debug Trace level here
#define TUTRACELEVEL    (TUINFO | TUERR)
//#define TUTRACELEVEL    (TUERR)
//#define TUTRACELEVEL    (0)

// trace levels
#define TUINFO  0x0001  
#define TUERR   0x0010      
     
#define TUTRACE_ERR        TUERR, __FILE__, __LINE__
#define TUTRACE_INFO       TUINFO, __FILE__, __LINE__

#define TUTRACE(VARGLST)   PrintTraceMsg VARGLST

void PrintTraceMsg(int level, char *lpszFile,
                   int nLine, char *lpszFormat, ...);

#else //_TUDEBUGTRACE

#define TUTRACE(VARGLST)    ((void)0)

#endif //_TUDEBUGTRACE

#ifdef __cplusplus
}
#endif


#endif // _TUTRACE_H
