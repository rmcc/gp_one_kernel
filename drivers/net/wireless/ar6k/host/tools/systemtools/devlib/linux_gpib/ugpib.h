/*
 * User GPIB Include File
 *
 * Copyright (c) 1994 National Instruments Corp.
 * All rights reserved.
 */

#ifndef NI_UGPIB_H
#define NI_UGPIB_H

#ifdef __cplusplus
extern "C" {
#endif

/* Address type for 488.2 calls */
typedef short	Addr4882_t;	

#ifndef __GPIBDRIVER__
/* Global variables */
extern int		ibsta;
extern int		iberr;
extern unsigned int	ibcnt;
extern long 		ibcntl;
#endif

/* EOS mode bits */
#define REOS	0x0400		/* Terminate read on eos */
#define XEOS	0x0800		/* Assert EOI with eos byte */
#define BIN	0x1000		/* Eight-bit compare */

/* GPIB commands */
#define DCL	0x14		/* GPIB device clear			      */
#define GET	0x08		/* GPIB group execute trigger		      */
#define GTL	0x01		/* GPIB go to local			      */
#define LAD	0x20		/* Listen address (mask)		      */
#define LLO	0x11		/* GPIB local lock out			      */
#define PPC	0x05		/* GPIB parallel poll configure		      */
#define PPD	0x70		/* GPIB parallel poll disable		      */
#define PPE	0x60		/* GPIB parallel poll enable (mask)	      */
#define PPU	0x15		/* GPIB parallel poll unconfigure	      */
#define SDC	0x04		/* GPIB selected device clear		      */
#define SPD	0x19		/* GPIB serial poll disable		      */
#define SPE	0x18		/* GPIB serial poll enable		      */
#define TAD	0x40		/* Talk address (mask)			      */
#define TCT	0x09		/* GPIB take control			      */
#define UNL	0x3f		/* GPIB unlisten command		      */
#define UNT	0x5f		/* GPIB untalk command			      */

/* GPIB status bit vector */
#define ERR	(1<<15)		/* Error detected			      */
#define TIMO	(1<<14)		/* Timeout				      */
#define END	(1<<13)		/* EOI or EOS detected			      */
#define SRQI	(1<<12)		/* SRQ detected by CIC			      */
#define RQS	(1<<11)		/* Device requires service		      */
#define SPOLL	(1<<10)		/* Board has been serially polled	      */
#define EVENT	(1<<9)		/* An event has occurred		      */
#define CMPL	(1<<8)		/* DMA completed (SH/AH synch'd)	      */
#define LOK	(1<<7)		/* Local lockout state			      */
#define REM	(1<<6)		/* Remote state				      */
#define CIC	(1<<5)		/* Controller-in-charge			      */
#define ATN	(1<<4)		/* Attention asserted			      */
#define TACS	(1<<3)		/* Talker active			      */
#define LACS	(1<<2)		/* Listener active			      */
#define DTAS	(1<<1)		/* Device trigger state			      */
#define DCAS	(1<<0)		/* Device clear state			      */

/* Error messages */
#define EDVR	0		/* System error				      */
#define ECIC	1		/* Not CIC (or lost CIC during command)	      */
#define ENOL	2		/* Write detected no listeners		      */
#define EADR	3		/* Board not addressed correctly	      */
#define EARG	4		/* Bad argument to function call	      */
#define ESAC	5		/* Function requires board to be SAC	      */
#define EABO	6		/* Asynchronous operation was aborted	      */
#define ENEB	7		/* Non-existent board			      */
#define EDMA	8		/* DMA hardware error detected		      */
#define EBTO	9		/* DMA hardware uP bus timeout		      */
#define EOIP	10		/* New I/O attempted with old I/O in progress */
#define ECAP	11		/* No capability for intended opeation	      */
#define EFSO	12		/* File system operation error		      */
#define EOWN 	13		/* Shareable board exclusively owned	      */
#define EBUS	14		/* Bus error				      */
#define ESTB	15		/* Serial poll queue overflow		      */
#define ESRQ	16		/* SRQ line 'stuck' on			      */
#define ETAB	20		/* The return buffer is full		      */
#define ELCK	21		/* Board or address is locked		      */

/* Timeout values and meanings */
#define TNONE	0		/* Infinite timeout (disabled)		      */
#define T10us	1		/* Timeout of 10 us (ideal)		      */
#define T30us	2		/* Timeout of 30 us (ideal)		      */
#define T100us	3		/* Timeout of 100 us (ideal)		      */
#define T300us	4		/* Timeout of 300 us (ideal)		      */
#define T1ms	5		/* Timeout of 1 ms (ideal)		      */
#define T3ms	6		/* Timeout of 3 ms (ideal)		      */
#define T10ms	7		/* Timeout of 10 ms (ideal)		      */
#define T30ms	8		/* Timeout of 30 ms (ideal)		      */
#define T100ms	9		/* Timeout of 100 ms (ideal)		      */
#define T300ms	10		/* Timeout of 300 ms (ideal)		      */
#define T1s	11		/* Timeout of 1 s (ideal)		      */
#define T3s	12		/* Timeout of 3 s (ideal)		      */
#define T10s	13		/* Timeout of 10 s (ideal)		      */
#define T30s	14		/* Timeout of 30 s (ideal)		      */
#define T100s	15		/* Timeout of 100 s (ideal)		      */
#define T300s	16		/* Timeout of 300 s (ideal)		      */
#define T1000s	17		/* Timeout of 1000 s (maximum)		      */

/* GPIB Bus Control Lines bit vector */
#define ValidDAV  ((short)0x0001)
#define ValidNDAC ((short)0x0002)
#define ValidNRFD ((short)0x0004)
#define ValidIFC  ((short)0x0008)
#define ValidREN  ((short)0x0010)
#define ValidSRQ  ((short)0x0020)
#define ValidATN  ((short)0x0040)
#define ValidEOI  ((short)0x0080)
#define BusDAV    ((short)0x0100) /* DAV line status bit		      */
#define BusNDAC   ((short)0x0200) /* NDAC line status bit		      */
#define BusNRFD   ((short)0x0400) /* NRFD line status bit		      */
#define BusIFC    ((short)0x0800) /* IFC line status bit		      */
#define BusREN    ((short)0x1000) /* REN line status bit		      */
#define BusSRQ    ((short)0x2000) /* SRQ line status bit		      */
#define BusATN    ((short)0x4000) /* ATN line status bit		      */
#define BusEOI    ((short)0x8000) /* EOI line status bit		      */


/* Old obsolete codes provided for compatibility -- Do NOT use these codes */
#define BUS_DAV         BusDAV
#define BUS_NDAC        BusNDAC
#define BUS_NRFD        BusNRFD
#define BUS_IFC         BusIFC
#define BUS_REN         BusREN
#define BUS_SRQ         BusSRQ
#define BUS_ATN         BusATN
#define BUS_EOI         BusEOI


/* The following constants are used for the second parameter of the 
  ibconfig function.  They are the "option" selection codes. */

#define IbcPAD		0x0001	/* Primary Address			      */
#define IbcSAD		0x0002	/* Secondary Address			      */
#define IbcTMO		0x0003	/* Timeout Value			      */
#define IbcEOT		0x0004	/* Send EOI with last data byte?	      */
#define IbcPPC		0x0005	/* Parallel Poll Configure		      */
#define IbcREADDR	0x0006	/* Repeat Addressing			      */
#define IbcAUTOPOLL	0x0007	/* Enable/Disable Auto Serial Polling	      */
#define IbcCICPROT	0x0008	/* Use the CIC Protocol?		      */
#define IbcIRQ		0x0009	/* Enable/Disable Hardware Interrupts	      */
#define IbcSC		0x000A	/* Board is System Controller?		      */
#define IbcSRE		0x000B	/* Assert SRE on device calls?		      */
#define IbcEOSrd	0x000C	/* Terminate reads on EOS		      */
#define IbcEOSwrt	0x000D	/* Send EOI with EOS character		      */
#define IbcEOScmp	0x000E	/* Use 7 or 8-bit EOS compare		      */
#define IbcEOSchar	0x000F	/* The EOS character.			      */
#define IbcPP2		0x0010	/* Use Parallel Poll Mode 2.		      */
#define IbcTIMING	0x0011	/* NORMAL, HIGH, or VERY_HIGH timing.	      */
#define IbcDMA		0x0012	/* Use DMA for I/O			      */
#define IbcReadAdjust	0x0013	/* Swap bytes during an ibrd.		      */
#define IbcWriteAdjust	0x0014	/* Swap bytes during an ibwrt.		      */
#define IbcEventQueue	0x0015	/* Enable/disable the event queue.	      */
#define IbcSPollBit	0x0016	/* Enable/disable the visibility of SPOLL.    */
#define IbcSpollBit	0x0016	/* Enable/disable the visibility of SPOLL.    */
#define IbcSendLLO	0x0017	/* Enable/disable the sending of LLO.	      */
#define IbcSPollTime	0x0018	/* Set the timeout value for serial polls.    */
#define IbcPPollTime	0x0019	/* Set the parallel poll length period.	      */
#define IbcNoEndBitOnEOS 0x01A	/* Remove EOS from END bit of IBSTA.	      */
#define IbcEndBitIsNormal 0x1A	/* Remove EOS from END bit of IBSTA.	      */
#define IbcUnAddr	0x001B	/* Enable/disable device unaddressing.	      */
#define IbcSignalNumber	0x001C	/* Set the signal to send on ibsgnl events.   */
#define IbcHSCableLength 0x01F	/* Set the cable length for HS488 transfers.  */
#define IbcLON		0x0022


/* Constants that can be used (in addition to the ibconfig constants)
  when calling the ibask() function.  */

#define IbaPAD		  IbcPAD            
#define IbaSAD		  IbcSAD            
#define IbaTMO		  IbcTMO            
#define IbaEOT		  IbcEOT            
#define IbaPPC		  IbcPPC            
#define IbaREADDR	  IbcREADDR         
#define IbaAUTOPOLL	  IbcAUTOPOLL       
#define IbaCICPROT	  IbcCICPROT        
#define IbaIRQ		  IbcIRQ            
#define IbaSC		  IbcSC             
#define IbaSRE		  IbcSRE            
#define IbaEOSrd	  IbcEOSrd          
#define IbaEOSwrt	  IbcEOSwrt         
#define IbaEOScmp	  IbcEOScmp         
#define IbaEOSchar	  IbcEOSchar        
#define IbaPP2		  IbcPP2            
#define IbaTIMING	  IbcTIMING         
#define IbaDMA		  IbcDMA            
#define IbaReadAdjust	  IbcReadAdjust     
#define IbaWriteAdjust	  IbcWriteAdjust    
#define IbaEventQueue	  IbcEventQueue     
#define IbaSPollBit	  IbcSPollBit       
#define IbaSpollBit	  IbcSpollBit       
#define IbaSendLLO	  IbcSendLLO        
#define IbaSPollTime	  IbcSPollTime      
#define IbaPPollTime	  IbcPPollTime      
#define IbaNoEndBitOnEOS  IbcNoEndBitOnEOS
#define IbaEndBitIsNormal IbcNoEndBitOnEOS
#define IbaUnAddr	  IbcUnAddr         
#define IbaSignalNumber	  IbcSignalNumber
#define IbaHSCableLength  IbcHSCableLength
#define IbaLON		  IbcLON

#define IbaBNA		  0x200	/* A device's access board.                   */
#define IbaBaseAddr	  0x201	/* A GPIB board's base I/O address.           */
#define IbaDmaChannel	  0x202	/* A GPIB board's DMA channel.                */
#define IbaIrqLevel	  0x203	/* A GPIB board's IRQ level.                  */
#define IbaBaud		  0x204	/* Baud rate used to communicate to CT box.   */
#define IbaParity	  0x205	/* Parity setting for CT box.                 */
#define IbaStopBits	  0x206	/* Stop bits used for communicating to CT.    */
#define IbaDataBits	  0x207	/* Data bits used for communicating to CT.    */
#define IbaComPort	  0x208	/* System COM port used for CT box.           */
#define IbaComIrqLevel	  0x209	/* System COM port's interrupt level.         */
#define IbaComPortBase	  0x20A	/* System COM port's base I/O address.        */
#define IbaSingleCycleDma 0x20B	/* DMA xfer mode (0:demand, 1:single-cycle)   */

/* Secondary address constant used by ibln() */
#define NO_SAD		 0	/* No secondary address			      */
#define ALL_SAD		-1	/* Send all secondary addresses		      */

/* Values used by the 488.2 Send command */
#define NULLend		0x00	/* Do nothing at the end of a transfer.	      */
#define NLend		0x01	/* Send NL with EOI after a transfer.	      */
#define DABend		0x02	/* Send EOI with the last data byte.	      */

/* Value used by the 488.2 Receive command. */
#define STOPend		0x0100

/*
 * This macro can be used to easily create an entry in address list that
 * is required by many of the 488.2 functions.  The primary address goes
 * goes in the lower 8-bits and the secondary address goes in the upper
 * 8-bits.
 */
#define MakeAddr(pad, sad)	((Addr4882_t)(((pad) & 0xFF) | ((sad) << 8)))

/*
 * This value is used to terminate an address list.  It should be
 * assigned to the last entry.
 */
#define NOADDR		((Addr4882_t)0xFFFF)

/*
 * The following two macros are used to "break apart" an address list
 * entry.  They take an unsigned integer and return either the primary
 * or secondary address stored in the integer.
 */
#define GetPAD(val)	((val) & 0xFF)
#define GetSAD(val)	(((val) >> 8) & 0xFF)

/*
 * This structure defines driver information returned from ibdiag()
 */
struct fixed_info {
    unsigned char	handler_type;
    unsigned char	board_type;
    unsigned char	handler_version[4];
    unsigned int	handler_segment;	/* DOS drivers only */
    unsigned int	handler_size;		/* DOS drivers only */
};

typedef struct fixed_info ib_fixed_info_t;


#ifndef __GPIBDRIVER__
/* Function prototypes */
#ifdef __STDC__
extern int ibask(int handle, int option, int *retval);
extern int ibbna(int handle, char *bdname);
extern int ibcac(int handle, int v);
extern int ibclr(int handle);
extern int ibcmd(int handle, void *buffer, long cnt);
extern int ibcmda(int handle, void *buffer, long cnt);
extern int ibconfig(int handle, int option, int value);
extern int ibdev(int boardID, int pad, int sad, int tmo, int eot, int eos);
extern int ibdiag(int handle, void *buffer, long cnt);
extern int ibdma(int handle, int v);
extern int ibeos(int handle, int v);
extern int ibeot(int handle, int v);
extern int ibevent(int handle, short *event);
extern int ibfind(char *bdname);
extern int ibgts(int handle, int v);
extern int ibist(int handle, int v);
extern int iblines(int handle, short *lines);
extern int ibllo(int handle);
extern int ibln(int handle, int padval, int sadval, short *listenflag);
extern int ibloc(int handle);
extern int ibonl(int handle, int v);
extern int ibpad(int handle, int v);
extern int ibpct(int handle);
extern int ibpoke(int handle, int option, int value);
extern int ibppc(int handle, int v);
extern int ibrd(int handle, void *buffer, long cnt);
extern int ibrda(int handle, void *buffer, long cnt);
extern int ibrdf(int handle, char *flname);
extern int ibrdkey(int handle, void *buffer, int cnt);
extern int ibrpp(int handle, char *ppr);
extern int ibrsc(int handle, int v);
extern int ibrsp(int handle, char *spr);
extern int ibrsv(int handle, int v);
extern int ibsad(int handle, int v);
extern int ibsgnl(int handle, int v);
extern int ibsic(int handle);
extern int ibsre(int handle, int v);
extern int ibsrq(void (*func)(void));
extern int ibstop(int handle);
extern int ibtmo(int handle, int v);
extern int ibtrap(int  mask, int mode);
extern int ibtrg(int handle);
extern int ibwait(int handle, int mask);
extern int ibwrt(int handle, void *buffer, long cnt);
extern int ibwrta(int handle, void *buffer, long cnt);
extern int ibwrtf(int handle, char *flname);
extern int ibwrtkey(int handle, void *buffer, int cnt);
extern int ibxtrc(int handle, void *buffer, long cnt);

extern void AllSpoll(int boardID, Addr4882_t *addrlist, short *resultlist);
extern void DevClear(int boardID, Addr4882_t address);
extern void DevClearList(int boardID, Addr4882_t *addrlist);
extern void EnableLocal(int boardID, Addr4882_t *addrlist);
extern void EnableRemote(int boardID, Addr4882_t *addrlist);
extern void FindLstn(int boardID, Addr4882_t *padlist, Addr4882_t *resultlist, int limit);
extern void FindRQS(int boardID, Addr4882_t *addrlist, short *result);
extern void PPoll(int boardID, short *result);
extern void PPollConfig(int boardID, Addr4882_t address, int dataLine, int lineSense);
extern void PPollUnconfig(int boardID, Addr4882_t *addrlist);
extern void PassControl(int boardID, Addr4882_t address);
extern void RcvRespMsg(int boardID, void *buffer, long cnt, int termination);
extern void ReadStatusByte(int boardID, Addr4882_t address, short *result);
extern void Receive(int boardID, Addr4882_t address, void *buffer, long cnt, int termination);
extern void ReceiveSetup(int boardID, Addr4882_t address);
extern void ResetSys(int boardID, Addr4882_t *addrlist);
extern void Send(int boardID, Addr4882_t address, void *buffer, long datacnt, int eotmode);
extern void SendCmds(int boardID, void *buffer, long cnt);
extern void SendDataBytes(int boardID, void *buffer, long cnt, int eotmode);
extern void SendIFC(int boardID);
extern void SendLLO(int boardID);
extern void SendList(int boardID, Addr4882_t *addrlist, void *buffer, long datacnt, int eotmode);
extern void SendSetup(int boardID, Addr4882_t *addrlist);
extern void SetRWLS(int boardID, Addr4882_t *addrlist);
extern void TestSRQ(int boardID, short *result);
extern void TestSys(int boardID, Addr4882_t *addrlist, short *resultlist);
extern void Trigger(int boardID, Addr4882_t address);
extern void TriggerList(int boardID, Addr4882_t *addrlist);
extern void WaitSRQ(int boardID, short *result);

#else /* __STDC__ */
extern int ibask();
extern int ibbna();
extern int ibcac();
extern int ibclr();
extern int ibcmd();
extern int ibcmda();
extern int ibconfig();
extern int ibdev();
extern int ibdiag();
extern int ibdma();
extern int ibeos();
extern int ibeot();
extern int ibevent();
extern int ibfind();
extern int ibgts();
extern int ibist();
extern int iblines();
extern int ibllo();
extern int ibln();
extern int ibloc();
extern int ibonl();
extern int ibpad();
extern int ibpct();
extern int ibpoke();
extern int ibppc();
extern int ibrd();
extern int ibrda();
extern int ibrdf();
extern int ibrdkey();
extern int ibrpp();
extern int ibrsc();
extern int ibrsp();
extern int ibrsv();
extern int ibsad();
extern int ibsgnl();
extern int ibsic();
extern int ibsre();
extern int ibsrq();
extern int ibstop();
extern int ibtmo();
extern int ibtrap();
extern int ibtrg();
extern int ibwait();
extern int ibwrt();
extern int ibwrta();
extern int ibwrtf();
extern int ibwrtkey();
extern int ibxtrc();

extern void AllSpoll();
extern void DevClear();
extern void DevClearList();
extern void EnableLocal();
extern void EnableRemote();
extern void FindLstn();
extern void FindRQS();
extern void PPoll();
extern void PPollConfig();
extern void PPollUnconfig();
extern void PassControl();
extern void RcvRespMsg();
extern void ReadStatusByte();
extern void Receive();
extern void ReceiveSetup();
extern void ResetSys();
extern void Send();
extern void SendCmds();
extern void SendDataBytes();
extern void SendIFC();
extern void SendLLO();
extern void SendList();
extern void SendSetup();
extern void SetRWLS();
extern void TestSRQ();
extern void TestSys();
extern void Trigger();
extern void TriggerList();
extern void WaitSRQ();
#endif /* __STDC__ */
#endif /* __GPIBDRIVER__ */

#ifdef __cplusplus
}
#endif

#endif /* NI_UGPIB.H */

