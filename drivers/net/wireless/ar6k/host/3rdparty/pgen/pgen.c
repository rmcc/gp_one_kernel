//  
//  May-2005 
//    Greg Chesson           greg@atheros.com
//    Stephen [kiwin] PALM   wmm@kiwin.com
//////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2005  Greg Chesson,  Stephen [kiwin] PALM
//
// License is granted to Wi-Fi Alliance members and designated
// contractors for exclusive use in testing of Wi-Fi equipment.
// This license is not transferable and does not extend to non-Wi-Fi
// applications.
//
// Derivative works are authorized and are limited by the
// same restrictions.
//
// Derivatives in binary form must reproduce the
// above copyright notice, the name of the authors "Greg Chesson" and
// "Stephen [kiwin] Palm",
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// The name of the authors may not be used to endorse or promote
// products derived from this software without specific prior
// written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
// IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
// IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
#//////////////////////////////////////////////////////////////////////

#include <ctype.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>

#include <net/if.h>
#include <sys/ioctl.h>

#ifdef __CYGWIN__
#include <rpc.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#else
#include <rpc/rpc.h>
#include <linux/ip.h>
#endif

#include <netdb.h>
#include <errno.h>
#include <string.h>

//   #include <arpa/inet.h>


/*
* Modes of operation
*/
int which_program;
#define	STAUT	1		// Station Under Test
#define	APTS	2		// APSD Test System
#define	AP	3		// AP proxy
#define	PHONE	4		// Phone emulator
#define	UPSD	5		// UPSD phone
#define	PGEN	6		// Generate packets
#define	PGET	7		// Get packets

int nsta=0;                          // Number of stations
int sd;				    // socket descriptor
int msgsize = 256;                  // tx msg size
int msgno =0;			    // tx msg number
int msgno_ul= -1;                   // uplink message number
int rmiss;			    // number of missed messages
int sta_test;                       // the test the STA was commanded to perform
int my_sta_id;			    // integer id
int my_cookie;			    // value to echo in load test
int my_group_cookie;		    // value to echo in load test from broadcast frame
unsigned long txmsg[512];	    // tx msg buffer
unsigned long rmsg[512];           // rx msg buffer
// txmsg[0]  msgno              send_txmsg
// txmsg[1]  dscp               send_txmsg
// txmsg[2]  my_group_cookie    send_txmsg
// txmsg[3]  my_cookie          send_txmsg
// txmsg[4]  my_sta_id          send_txmsg
// txmsg[5] 
// txmsg[6]  param0  nexchang   create_apts_msg
// txmsg[7]  param1  nup        create_apts_msg
// txmsg[8]  param2  ndown      create_apts_msg
// txmsg[9]  param3             create_apts_msg
// txmsg[10] cmd                send_txmsg
// txmsg[11] cmd-text
// txmsg[12] cmd-text
// txmsg[13] msgno ascii        send_txmsg
// txmsg[14] 

// rmsg[0] nrecv (downlink) 
// rmsg[1] 
// rmsg[2] 
// rmsg[3]    sta->cookie      do_apts          nsent B_H  do_apts
// rmsg[4]    
// rmsg[5] 
// rmsg[6] 
// rmsg[7] 
// rmsg[8] 
// rmsg[9]    id   (confirm)   do_apts
// rmsg[10]   
// rmsg[11] 
// rmsg[12] 
// rmsg[13] 
// rmsg[14]  nsent%10   do_apts

struct sockaddr dst, rcv;           // sock declarations
struct sockaddr_in target;
struct sockaddr_in from;
struct sockaddr_in local;
int sockflags;			    // socket call flags
unsigned int fromlen;		    // sizeof socket struct
char *period;                       // codec period (if specified)
char *count;			    // output frame count
int nsent=0;                        // Number sent 
int nrecv=0;                        // Number received 
int ngrecv=0;                       // Number received group/broadcast
int nerr=0;                         // Number of errors
int ngerr=0;                        // Number of errors group/broadcast
int nend;                           // Number of exchanges
int ndown;                          // Number of Downlink frames per exchange
int nup;                            // Number of Uplink frames per exchange

FILE *bsc_file;

double buzzinterval;
double buzzrate;
int buzzcount;
int buzzperiod;
void buzz();

#define	NTARG	32		    // number of target names
int ntarg;
char *targetnames[NTARG];
char *targetname;                   // dst system name or ip address
char *bcstIpAddress;                // broadcast Ip Address
int bcstIpAddressDefined = 0;  // broadcast ip address defined by command line?
/*
* APTS messages/tests
*/
#define	APTS_DEFAULT	1		// message codes
#define	APTS_HELLO	2
#define	APTS_HELLO_RESP	3
#define	APTS_CONFIRM	4
#define	APTS_STOP	5
#define APTS_CK_BE      6
#define APTS_CK_BK      7
#define APTS_CK_VI      8
#define APTS_CK_VO      9
#define	APTS_TESTS	10		// test codes begin after APTS_TESTS
#define B_D		11
#define B_2		12
#define	B_H		13
#define	B_4		14
#define	B_5		15
#define	B_6		16
#define B_B             17
#define B_E             18
#define B_G             19
#define B_M             20
#define M_D             21
#define M_G             22
#define M_I             23
#define B_Z             24
#define M_Y             25
#define	L_1		26
#define	DLOAD		27
#define	ULOAD		28
#define	APTS_PASS	29
#define	APTS_FAIL	30
#define	A_Y		31		// active mode version of M_Y
#define	B_W		32		// 
#define	A_J		33		// Active test of sending 4 down
#define M_V             34
#define M_U             35
#define A_U             36
#define M_L             37
#define B_K             38
#define M_B             39
#define M_K             40
#define M_W             41
#define	APTS_LAST	42		// reminder: update APTS_LAST when adding tests
#define APTS_BCST       99

/*
* internal table
*/
struct apts_msg {			// 
	char *name;			// name of test
	int cmd;			// msg num
	int param0;			// number of packet exchanges
	int param1;			// number of uplink frames 
	int param2;			// number of downlink frames
	int param3;
};


struct apts_msg apts_msgs[] ={
	{0, -1, 0, 0, 0, },
	{"APTS TX         ", APTS_DEFAULT, 0, 0, 0, },
	{"APTS Hello      ", APTS_HELLO, 0, 0, 0, 0},
	{"APTS Hello Resp ", APTS_HELLO_RESP, 0, 0, 0, 0},
	{"APTS Confirm    ", APTS_CONFIRM, 0, 0, 0, 0},
	{"APTS STOP       ", APTS_STOP, 0, 0, 0, 0},
	{"APTS CK BE      ", APTS_CK_BE, 0, 0, 0, 0, },
	{"APTS CK BK      ", APTS_CK_BK, 0, 0, 0, 0, },
	{"APTS CK VI      ", APTS_CK_VI, 0, 0, 0, 0, },
	{"APTS CK VO      ", APTS_CK_VO, 0, 0, 0, 0, },
	{0, 10, 0, 0, 0, },		// APTS_TESTS
	{"B.D", B_D, 4, 1, 1, 0},	// 4 single packet exchanges
	{"B.2", B_2, -1, 1, 1, 0},	// continuous single packet exchanges
	{"B.H", B_H, 4, 1, 2, 0},	// 4 exchanges: 1 uplink, 2 downlink frames
	{"B.4", B_4, 4, 2, 1, 0},	// 4 exchanges: 2 uplink (trigger 2nd), 1 downlink frames
	{ 0,    B_5, 4, 2, 1, 0},	// placeholder
	{ 0,    B_6, 4, 2, 1, 0},	// placeholder
	{"B.B", B_B, 4, 1, 0, 0},	// 4 exchanges: 1 uplink, 0 downlink
	{"B.E", B_E, 4, 2, 0, 0},	// 4 exchanges: 2 uplink, 0 downlink
	{"B.G", B_G, 4, 2, 1, 0},	// 4 exchanges: 2 uplink, 1 downlink
	{"B.M", B_M, 4, 2, 1, 0},	// 
	{"M.D", M_D, 4, 1, 1, 0},	// 4 single packet exchanges (mixed legacy/U-APSD)
	{"M.G", M_G, 4, 2, 1, 0},	// 4 exchanges: 2 uplink, 1 downlink (mixed legacy/U-APSD)
	{"M.I", M_I, 4, 2, 2, 0},	// 4 exchanges: 2 uplink, 2 downlink (mixed legacy/U-APSD)
	{"B.Z", B_Z, 1, 1, 1, 0},	// 1 special exchange for Broadcast testing
	{"M.Y", M_Y, 4, 1, 1, 0},	// special exchange for Broadcast testing
	{"L.1", L_1, 3000, 3000, 3000, 20},	// bidirectional voip-like 60-second load test
	{"Downlink Load", DLOAD, 0, 0, 0, 0},	// label for downlink frames during load test
	{"Uplink Load", ULOAD, 0, 0, 0, 0},	// label for uplink frames during load test
	{"APTS PASS", APTS_PASS, 0, 0, 0, 0},
	{"APTS FAIL", APTS_FAIL, 0, 0, 0, 0},
	{"A.Y", A_Y, 5, 1, 1, 0},	// A_Y like M_Y, but staut retrieves frames in Active Mode
	{"B.W", B_W, 3, 1, 1, 0},	// 3 special exchange for Broadcast testing
	{"A.J", A_J, 1, 1, 4, 0},	// 1 exchanges: 1 uplink, 4 downlink frames
	{"M.V", M_V, 4, 1, 1, 0},	// 3 special exchange (but use to capture last one)
	{"M.U", M_U, 4, 1, 1, 0},	// 3 special exchange (but use to capture last one)
    {"A.U", A_U, 6, 1, 1, 0},	// 3 special exchange (but use to capture last one)
	{"M.L", M_L, 4, 1, 1, 0},	// 4 single packet exchanges
	{"B.K", B_K, 4, 1, 1, 0},	// 4 exchanges: 1 uplink, 0 downlink
	{"M.B", M_B, 4, 1, 0, 0},	// 4 exchanges: 1 uplink, 0 downlink
	{"M.K", M_K, 4, 1, 1, 0},	// 4 exchanges: 1 uplink, 0 downlink
	{"M.W", M_W, 3, 1, 1, 0},	// 3 special exchange for Broadcast testing
	{0, 0, 0, 0, 0, }		// APTS_LAST
};

#define PORT    12345               // port for sending/receiving
int port        = PORT;

#define EQ(a,b)     (strcmp(a,b)==0)

struct itimerval waitval_ap;        // codec and ap wait intervals
struct itimerval waitval_codec;
struct itimerval waitval_cancel;
struct timeval time_ul;		    // uplink timestamp
struct timeval time_ap;		    // downlink timestamp
struct timeval time_delta;	    // time difference
struct timeval time_start, time_stop;

int codec_sec = 0;                      // default values for codec period
int codec_usec = 10000;                 // 10 ms
int rcv_sleep_time = 10;


/*
* Wait/Timer states
*/
int wstate;                             // what event are we currently timing
typedef enum {
	WAIT_NEXT_CODEC,			
	WAIT_FOR_AP_RESPONSE,
	WAIT_STUAT_00,
	WAIT_STUAT_01,
	WAIT_STUAT_02,
	WAIT_STUAT_03,
	WAIT_STUAT_04,
	WAIT_STUAT_0E,
	WAIT_STUAT_VOLOAD,
	WAIT_STUAT_SEQ,
} WAIT_MODE;

/*
* power management
*/
#define	P_ON	1
#define	P_OFF	0

/*
* global flags and variables
*/
unsigned char dscp, ldscp;		// new/last dscp output values
char phoneflag = 0;                     // operate as the phone
char apflag = 0;                        // operate as ap, echo every frame back to phone
char stautflag= 0;                      // operate as staut, wait for command packet
char staloopflag = 0;                   // STA loops instead of exiting upon STOP
int sta_state = 0;
char aptsflag = 0;                      // operate as AP Test software (TS) mode
int apts_state = 0;
char upsdflag = 0;                      // same as phone mode
char upsdstartflag;
unsigned char upsdval = 0xb8;		// dscp value for AC_VO
char pgenflag;                          // operate as packet generator only
char pgetflag;				// operate as packet reader only
char histflag;				// keep running histogram
char wmeloadflag;                       // generate packets for all 4 ACCs
char floodflag;				// flood/saturate the network
char broadcastflag;			// generate broadcast packets
char qdataflag;                         // generate QOS_DATA frames at AP
char qdisflag;
char tspecflag;				// generate a tspec frame
char iphdrflag;                         // print ip header
char traceflag;				// enable debug packet tracing
char prcvflag = 1;			// enable rcv packet printing in do_staut
char brcmflag = 0;			// is BRCM device
char athrflag = 0;			// is ATHR device
char cnxtflag = 0;			// is CNXT device
char intelflag = 0;			// is Intel device
char mrvlflag = 0;			// is MRVL device
char ralinkflag = 0;			// is Ralink device
char winbflag = 0;			// is Winbond device
unsigned char qdenable = 0xbc;		// special dscp values for driver debugging only
char companyProduct = 0;    // each company could have multiple products.
unsigned char qdisable = 0xad;
unsigned char tsenable = 0xbd;

/*
* wme
*/

#define	TOS_VO7	    0xE0		// 111 0  0000 (7)  AC_VO tos/dscp values
#define	TOS_VO	    0xD0		// 110 0  0000 (6)  AC_VO tos/dscp values

#define	TOS_VI	    0xA0                // 101 0  0000 (5)  AC_VI
#define	TOS_VI4	    0x80                // 100 0  0000 (4)  AC_VI

#define	TOS_BE	    0x00                // 000 0  0000 (0)  AC_BE
#define	TOS_EE	    0x60                // 011 0  0000 (3)  AC_BE

#define	TOS_BK	    0x20                // 001 0  0000 (1)  AC_BK
#define	TOS_LE	    0x40                // 010 0  0000 (2)  AC_BK

unsigned char dscpval[] ={ 0x18, 0x18, 0x18, 0x18, 0x08, 0x08, 0x08, 0x08,  0x30, 0x30, 0x30, 0x30, 0x28, 0x28, 0x28, 0x28 };
unsigned int dscpx[sizeof(dscpval)];

//unsigned char ac_sequence[] = {TOS_VI, TOS_VO, TOS_BE, TOS_BE, 0};       // sequence for tests M.Y, A.Y

int ac_seq[APTS_LAST][6] ={
	{0,      0,      0,      0,      0},
	{0},
	{0},
	{0},
	{0},
	{0},
	{0},
	{0},
	{0},
	{0},
	{0},		// APTS_TESTS
	{0}, // B.D
	{0}, // B.2
	{0}, // B.H
	{0}, // B.4
	{0}, // B_5
	{0,      0,      0,      0,      0}, // B_6
	{TOS_VO, TOS_VI, TOS_BE, TOS_BK, 0}, // B.B B_B - 4 exchanges: 1 uplink, 0 downlink
	{0}, // B.E
	{0}, // B.G
	{0}, // B.I
	{0}, // M.D
	{0}, // M.G
	{0}, // M.I
	{0}, // B.Z  1, 1, 1, 0},	// 1 special exchange for Broadcast testing
	{TOS_VI, TOS_VO, TOS_BE, TOS_BE, 0}, //  M.Y  M_Y 2 special exchange for Broadcast testing
	{0}, // L.1
	{0}, // DLOAD
	{0}, // ULOAD
	{0}, // "APTS PASS"
	{0}, // "APTS FAIL"
	//{TOS_VI, TOS_VO, TOS_BE, TOS_BE, 0}, //  A.Y A_Y special exchange for Broadcast testing
	{TOS_VI, TOS_VO, TOS_BE, TOS_BE, TOS_BE}, //  A.Y A_Y special exchange for Broadcast testing
	{0}, //  B.W  2 special exchange for Broadcast testing
	{0}, //  A.J
	{TOS_VI, TOS_BE, TOS_VI, TOS_VI, TOS_VI}, //  M.V M_V
	{TOS_VI, TOS_BE, TOS_VO, TOS_VO, TOS_VO}, //  M.U M_U
	//{TOS_VI, TOS_BE, TOS_VO, TOS_VO, TOS_VO},  //  A.U A_U
	{TOS_VI, TOS_BE, TOS_BE, TOS_BE, TOS_VO, TOS_VO},  //  A.U A_U
	{0}, //  M.L M_L
	{TOS_VI, TOS_BE, TOS_VI, TOS_VI, 0}, // B.K B_K
	{TOS_VO, TOS_VI, TOS_BE, TOS_BK, 0}, // M.B M_B - 4 exchanges: 1 uplink, 0 downlink
	{TOS_VI, TOS_BE, TOS_VI, TOS_VI, 0}, // M.K M_K
	{TOS_VI, TOS_BE, TOS_VI, TOS_VI, 0} //  M.W M_W   special exchange for Broadcast testing
};


__inline__ 
int dscp_to_acc(int dscpval) 
{
	int id;

	switch(dscpval) 
	{
	case TOS_LE:            // BK/LE  2
	case TOS_BK:            // BK/LE  1
		id = 0; break;
	case TOS_EE:            // BE 3
	case TOS_BE:            // BE 0
		id = 1; break;
	case TOS_VI:            // VI 5
	case TOS_VI4:           // VI 4
		id = 2; break;
	case TOS_VO:            // VO 6
	case TOS_VO7:           // VO 7
		id = 3; break;
	default:
		id = 4;
	}
	return(id);
}

#define	NSTA	20		    // number of trackable stations (id==0 held in reserve)
struct station 
{		    
	// keep state for individual stations
	int cookie;			    // cookie value exchanged with each staut
	struct timeval tstart;	    // start time for this station
	struct timeval tstop;	    // start time for this station
	unsigned long s_addr;	    // station address
	int nsent, nrecv, nend, nerr;   // counters
        int alreadyCleared; // ignore this station if this value is 1
  char ipaddress[20];
};
struct station stations[NSTA];

struct station *
	lookup_sta(unsigned long rmsg[])
{
  /*int id;
	struct station *sta;

	//sta = &stations[rmsg[4]];
	
	id = 0;
	while ( id<NSTA) 
	{
	  sta = &stations[id];
		// was this supposed to be ==?
		//sta->s_addr = from.sin_addr.s_addr;
		if (sta->s_addr == rmsg[4])
		  return(sta);
	}

  **	id = rmsg[4];
	while (id>0 && id<NSTA) 
	{
		sta = &stations[id];
		// was this supposed to be ==?
		//sta->s_addr = from.sin_addr.s_addr;
		if (sta->s_addr == from.sin_addr.s_addr)
		  return(sta);
	}
  **
	printf("lookup failed rmsg[4] %d\n", id);
	//return(&station[0]);
	return NULL;
*/


	  return (&stations[rmsg[4]]);
}

int 
assign_sta_id(unsigned long addr)
{
  printf("Assign station id for %d\n", addr);
	int id;
	for(id=0; id<NSTA; id++) 
	{
	  printf("search for address, id=%d, station addr is: %d\n", id, stations[id].s_addr);
		if (stations[id].s_addr == 0) 
		{
			stations[id].s_addr = addr;
			return(id);
		}
		  if(stations[id].s_addr == addr)
		    return id;
	}
	return(0);
}

void
clear_sta_id(struct station *sta)
{

	//bzero(sta, sizeof (*sta));
  sta->alreadyCleared = 1;
	nsta--;
	printf( "Clear STA=%d\n", nsta );
}

/*
* timediff
* return diff between two timevals
*/
struct timeval
	timediff(t2, t1)
struct timeval *t2, *t1;
{
	struct timeval del;

	del.tv_sec = t2->tv_sec - t1->tv_sec;
	del.tv_usec = t2->tv_usec - t1->tv_usec;
	if (del.tv_usec<0) 
	{
		del.tv_sec--;
		del.tv_usec += 1000000;
	}
	return(del);
}

float
ftimediff(t2, t1)
struct timeval *t2, *t1;
{
	struct timeval time_delta;
	float time;

	time_delta = timediff(t2, t1);
	time = time_delta.tv_sec;
	if (time_delta.tv_usec) 
	{
		time += time_delta.tv_usec * 1e-6;
	}
	return(time);
}


/*
* Histograms
*/
#define NSAMPLES    1000
#define	NBUCKETS    10
struct histogram {
	int buckets[NBUCKETS+1];
	float avg, sum;
	float min, max;
	int n;
	int nsamples;
};

float h_limits[NBUCKETS] = {10., 20., 30., 40., 50., 100., 500., 1000., 2000., 5000.,};

void
dohist(struct histogram *h, float val)
{
	static nout =0;
	int i;

	if (h->n==h->nsamples) 
	{
		h->avg = h->sum/h->n;
		h->n = 0;
		h->sum = 0.0;
		printf("avg %f\tmin(%f)\tmax(%f)\n", h->avg, h->min, h->max);
		if (nout==0) 
		{
			for (i=0; i<=NBUCKETS; i++) 
			{
				printf("%6.0f ", h_limits[i]);
			}
			printf("\n");
		}
		for (i=0; i<=NBUCKETS; i++) 
		{
			printf("%5d ", h->buckets[i]);
			h->buckets[i] = 0;
		}
		printf("\n");
		h->max = 0.0;
		h->min = 10000000.0;
		nout++;
	} 
	else 
	{
		h->sum += val;
		h->n++;
		if (nout==10)
			nout = 0;
	}
	if ((val < h->min) && val>0.0)
		h->min = val;
	if (val > h->max)
		h->max = val;
	if (val>0.0) 
	{
		for(i=0; i<NBUCKETS; i++) 
		{
			if (val < h_limits[i]) 
			{
				h->buckets[i]++;
				goto hdone;
			}
		}
		h->buckets[NBUCKETS]++;
	}
hdone:
	return;
}

int
set_dscp(int new_dscp)
{
	int r;
	if (new_dscp == ldscp)
		return;

	if ((r=setsockopt(sd, SOL_IP, IP_TOS, &new_dscp, sizeof(dscp)))<0) 
	{
		perror("can't set dscp/tos field");
		exit(-1);
	}
	dscp = ldscp = new_dscp;
	rmsg[1] = dscp;
	return(new_dscp);
}

void
send_txmsg(int new_dscp)
{
	int r, n;
	static int totalsent;


	if (new_dscp > -1) 
	{
		set_dscp(new_dscp);
	}

	msgno_ul = txmsg[0] = msgno++;
	txmsg[1] = dscp;
	txmsg[2] = my_group_cookie;
	txmsg[3] = my_cookie;
	txmsg[4] = my_sta_id;

	if ( (which_program == STAUT) && (txmsg[10] == APTS_DEFAULT )) 
	{ 
		txmsg[13] = (msgno%10) + 0x20202030; 
	}

	if (wmeloadflag || upsdflag ) 
	{
		n = msgno % sizeof(dscp);
		if (qdataflag) 
		{		// send special dscp value for debugging
			dscp = qdenable;
			qdataflag = 0;
		} else
			if (qdisflag) 
			{			
				// send special dscp value for debugging
				dscp = qdisable;
				qdisflag = 0;
			} 
			else if (upsdstartflag) 
			{
				dscp = upsdval;
				upsdstartflag = 0;
			} 
			else if (upsdflag) 
			{
				dscp = upsdval;
			} 
			else 
			{
				dscp = dscpval[n];
			}

			txmsg[1] = set_dscp(dscp);
			txmsg[2] = dscpx[dscp_to_acc(dscp)]++;
	}

	/* record start time */
	if (time_start.tv_sec==0) 
	{
		gettimeofday(&time_start, 0);
		nsent = 0;
	}

resend:
	r = sendto(sd, txmsg, msgsize+(msgno%200), sockflags, &dst, sizeof(dst));
	gettimeofday(&time_ul, 0);

	/* 
	* When we're flooding output queues with back-to-back packets,
	* (a saturation/overflow test)
	* wait for 1 ms and then resend, keeping output queue backlogged.
	*/
	if (r<0) 
	{
		buzz(100);
		goto resend;
	}
	nsent++;

	/* print each msg only if slow mode */
	if (waitval_codec.it_value.tv_sec && floodflag==0) 
	{
		printf("send_txmsg: size(%d) msgno(%3d) cmd(%2d) dscp(%2X)\n", msgsize+(msgno%200), msgno, txmsg[10], dscp);
		//pb("sendto", &dst, sizeof(dst));
		//mptimeval("ul", &time_ul);
	}

	if (count && nsent>=nend) 
	{
		float rate, time;

		gettimeofday(&time_stop, 0);
		time = ftimediff(&time_stop, &time_start);
		if (time) 
		{
			rate = nsent/time;
		} 
		else 
		{
			rate = 0.0;
		}
		printf("msgs sent %d\t", nsent);
		printf("elapsed time %f\t", time);
		printf("rate %.0f/s\n", rate);
		exit(0);
	}
}



void
create_apts_msg(int msg, unsigned long txbuf[])
{
	struct apts_msg *t;

	t = &apts_msgs[msg];
	msgno_ul = txbuf[0] = msgno++;
	txbuf[ 1] = 0;
	txbuf[ 2] = 0;
	txbuf[ 3] = 0;
	txbuf[ 4] = 0;
	txbuf[ 5] = 0; 
	txbuf[ 6] = t->param0; 
	txbuf[ 7] = t->param1; 
	txbuf[ 8] = t->param2; 
	txbuf[ 9] = t->param3; 
	txbuf[ 10] = t->cmd; 
	strcpy((char *)&txbuf[11], t->name);
	if (traceflag) printf("create_apts_msg (%s)\n", t->name);
}



/*
* use a buzz loop when interpacket time is smaller
* than resolution provided by real-time interval timer.
*/
void
buzz(int delay)
{
	struct timeval now, stop;
	float diff;
#ifdef TESTBUZZ
	struct timeval start;
	static first;
#endif

	gettimeofday(&stop, 0);
#ifdef TESTBUZZ
	start = stop;
#endif
	stop.tv_usec += delay;
	if (stop.tv_usec > 1000000) 
	{
		stop.tv_usec -= 1000000;
		stop.tv_sec += 1;
	}
	do 
	{
		gettimeofday(&now, 0);
		diff = ftimediff(&stop, &now);
	} while (diff>0.0);
#ifdef TESTBUZZ
	if (first==0) 
	{
		mptimeval("start\t", &start);
		mptimeval("stop \t", &now);
		printf("diff %f\n", ftimediff(&now, &start));
	}
	first = 1;
#endif
}

void
timeout()
{
	int r, i, n, t;
	int flags =0;

	if (traceflag)   printf( "STAUT timeout wstate: %d sec %d usec %d\n", 
		wstate, waitval_codec.it_value.tv_sec, waitval_codec.it_value.tv_usec );

	switch(wstate) 
	{
	case WAIT_NEXT_CODEC:
		send_txmsg(-1);
		wstate = WAIT_NEXT_CODEC;
		if ( setitimer(ITIMER_REAL, &waitval_codec, NULL) < 0 ) 
		{
			perror( "setitimer: WAIT_NEXT_CODEC: " );
			exit( -1 );
		}
		return;
	case WAIT_FOR_AP_RESPONSE:
		wstate = WAIT_NEXT_CODEC;
		gettimeofday(&time_ap, 0);
		time_delta = timediff(&time_ap, &time_ul);
		mptimeval("timeout ", &time_delta);
		if ( setitimer(ITIMER_REAL, &waitval_codec, NULL) < 0 ) 
		{
			perror( "setitimer: WAIT_FOR_AP_RESPONSE: " );
			exit( -1 );
		}
		return;
	case WAIT_STUAT_00:
		waitval_codec.it_value.tv_sec = 1;		// 1 sec codec sample period
		waitval_codec.it_value.tv_usec = 0;
		if (traceflag)   printf( "STAUT timeout sec %d usec %d period: %s\n", 
			waitval_codec.it_value.tv_sec, waitval_codec.it_value.tv_usec, period );
		if ( setitimer(ITIMER_REAL, &waitval_codec, NULL) < 0 ) 
		{
			perror( "setitimer: WAIT_STUAT_00: " );
			exit( -1 );
		}
		r = sendto(sd, txmsg, msgsize, sockflags, &dst, sizeof(dst));
		return;
	case WAIT_STUAT_02:
		send_txmsg(TOS_VO);
		if ( setitimer(ITIMER_REAL, &waitval_codec, NULL) < 0 ) 
		{
			perror( "setitimer: WAIT_STUAT_02: " );
			exit( -1 );
		}
		return;
	case WAIT_STUAT_04:
		send_txmsg(TOS_BE);		// send a best effort frame
		send_txmsg(TOS_VO);		// send a VO (trigger) frame
		if ( setitimer(ITIMER_REAL, &waitval_codec, NULL) < 0 ) 
		{
			perror( "setitimer: WAIT_STUAT_04: " );
			exit( -1 );
		}
		return;
	case WAIT_STUAT_0E:
		send_txmsg(TOS_VO);		// send a VO (trigger) frame
		send_txmsg(TOS_VO);		// send a VO (trigger) frame
		if ( setitimer(ITIMER_REAL, &waitval_codec, NULL) < 0 ) 
		{
			perror( "setitimer: WAIT_STUAT_0E: " );
			exit( -1 );
		}
		return;
	case WAIT_STUAT_VOLOAD:
		send_txmsg(TOS_VO);		// send a VO (trigger) frame
		if ( setitimer(ITIMER_REAL, &waitval_codec, NULL) < 0 ) 
		{
			perror( "setitimer: WAIT_STUAT_VOLOAD: " );
			exit( -1 );
		}
		return;
	case WAIT_STUAT_SEQ:
		if (nsent<0 || nsent>sizeof(ac_seq[sta_test]) )
			nsent = 0;
		printf("SEQ (%d) sending (%d)\n", sta_test, nsent);
		if ((sta_test==A_Y && nsent==3) 
			|| (sta_test==A_U && nsent==1) 
			|| (sta_test==A_U && nsent==3) 
			|| (sta_test==A_U && nsent==5) )
		{
			set_pwr_mgmt("STAUT-SEQ", P_OFF);
			usleep(100000);
		}
		if ((sta_test==A_Y && nsent==4) 
			|| (sta_test==A_U && nsent==2)
			|| (sta_test==A_U && nsent==4) )
		{
			set_pwr_mgmt("STAUT-SEQ", P_ON);
			usleep(100000);
		}
		send_txmsg(ac_seq[sta_test][nsent]);

		// comment out the following by JP, why do we want to reset them?
		//waitval_codec.it_value.tv_sec = 1;		// 1 sec codec sample period
		//waitval_codec.it_value.tv_usec = 0;
	
		if (traceflag)   printf( "STAUT timeout sec %d usec %d period: %s\n", 
			waitval_codec.it_value.tv_sec, waitval_codec.it_value.tv_usec, period );
		if ( setitimer(ITIMER_REAL, &waitval_codec, NULL) < 0 ) 
		{
			perror( "setitimer: WAIT_STUAT_SEQ: " );
			exit( -1 );
		}
		return;
	default:
		return;
	}
}

ptimeval(struct timeval *tv)                        // print routines for timevals
{
	printf("%2d:%06d\n", tv->tv_sec, tv->tv_usec);
}
mptimeval(char *s, struct timeval *tv)
{
	printf("%s\t ", s);
	ptimeval(tv);
}
pitimes(struct itimerval *itv)
{
	ptimeval(&itv->it_interval);
	ptimeval(&itv->it_value);
}
mpitimes(char *s, struct itimerval *itv)
{
	printf("%s\n", s); pitimes(itv);
}

setup_timers()
{
	int r;

	waitval_codec.it_value.tv_sec = codec_sec;		// sec codec sample period
	waitval_codec.it_value.tv_usec = codec_usec;
	waitval_ap.it_value.tv_sec = 1;			// set AP timeout value
	waitval_ap.it_value.tv_usec = 0;

	signal(SIGALRM, &timeout);                          // enable alarm signal
}

setup_period(char *period)
{
	int n;

	if (!period)
		return;
	n = atoi(period);
	printf("period %d usec\n", n);
	codec_sec = 0;
	codec_usec = n;
	if (n<10000) 
	{
		buzzperiod = n;
	}
}


paddr(unsigned char *s)
{
	printf("%d.%d.%d.%d\n", s[0], s[1], s[2], s[3]);
}

pbaddr(s, in)
char *s;
unsigned long int in;
{
	unsigned char a, b, c, d;

	a = in&0xff;
	b = (in>>8)&0xff;
	c = (in>>16)&0xff;
	d = (in>>24)&0xff;
	printf("%s %d.%d.%d.%d\n", s, a, b, c, d);
}

pb(char *s, unsigned char *b, int cc)
{
	printf("%s: ", s);
	while (cc--) 
	{
		printf("%x|%d ", *b, *b);
		b++;
	}
	printf("\n");
}

is_ipdotformat(char *s)
{
	int d;

	for(d=0; *s; s++) 
	{
		if (*s=='.')
			d++;
	}
	return(d==3);
}


int
strvec_sep(s, array, n, sep)
char *s, *array[], *sep;
int n;
{
	char *p;
	static char buf[2048];
	int i;

	strncpy(buf, s, sizeof(buf));

	p = strtok(buf, sep);

	for (i=0; p && i<n; ) 
	{
		array[i++] = p;
		if (i==n) 
		{
			i--;
			break;
		}
		p = strtok(0, sep);
	}
	array[i] = 0;
	return(i);
}



/*
* dst address
* may be host name or w.x.y.z format ip address
*/
int
setup_addr(char *name, struct sockaddr *dst)
{
	struct hostent *h;
	char *array[5];
	char c;
	char *s;
	int d, r;
	unsigned long in =0;
	unsigned char b;

#ifndef __CYGWIN__
	//printf( "setup_addr: entering\n" );
	if (broadcastflag) 
	{
	  if(bcstIpAddressDefined)
	    {
	      name = bcstIpAddress;
	      printf("BROADCAST dst %s\n", name);
	    }
	  else
	    {
	      printf("Automatically discover IP Address of eth0 interface, since no broadcast IP address has been specified by you.\n");
	  
		  int fd;
		  struct sockaddr_in *addr_ptr;
		  struct ifreq interface;
		  char IP[INET_ADDRSTRLEN];

		  /* tells ioctl which interface to query */
		  strcpy(interface.ifr_name, "eth0");  

		  /* need a socket to use ioctl */
		  fd = socket(AF_INET, SOCK_DGRAM, 0);

		  /* get interface ip address */
		  if (ioctl(fd, SIOCGIFADDR, &interface) < 0) {
			  perror("ERROR (ioctl (SIOCGIFADDR))");
			  exit(1);
		  }

		  /* kinda ugly, but this allows us to use inet_ntop */
		  addr_ptr = (struct sockaddr_in *) &interface.ifr_addr;

		  /* INET protocol family */
		  addr_ptr->sin_family = AF_INET;      

		  /* copy ip address */
		  inet_ntop(AF_INET, &addr_ptr->sin_addr, IP, INET_ADDRSTRLEN);    

		  /* print it */
		  printf("IP of eth0: %s\n", IP); 

		  printf("Assume this is a Class C IP Address, use netmask 255.255.255.0 to derive broadcast IP address.\n");

		  char ip[INET_ADDRSTRLEN];
		  int count = 0;
		  int index = 0;
		  while(count< 3)
		  {
			  ip[index] = IP[index];
			  if(ip[index] == '.')
				  count ++;
			  index++;
		  }

		  ip[index++] = '2';
		  ip[index++] = '5';
		  ip[index++] = '5';	  
		  ip[index++] = '\0';

		  name = ip;
		  printf("BROADCAST dst %s\n", name);
	  }
	}
#endif

	if ((apflag||pgetflag) || (name==NULL)) 
	{
		goto rcv_setup;
	}

	if (is_ipdotformat(name)) 
	{                 // check for dot format addr
		strvec_sep(name, array, 5, ".");
		for(d=0; d<4; d++) 
		{
			b = atoi(array[d]);
			in |= (b<<(d*8));
		}
		target.sin_addr.s_addr = in;
	} 
	else 
	{
		h = gethostbyname(name);                // try name lookup
		if (h) 
		{
			memcpy((caddr_t)&target.sin_addr.s_addr, h->h_addr, h->h_length);
			//paddr(h->h_addr_list[0]);
		} 
		else 
		{
			fprintf(stderr, "name lookup failed for (%s)\n", name);
			exit(-1);
		}
	}
	pbaddr("dst:", target.sin_addr.s_addr);
	target.sin_family = AF_INET;
	target.sin_port = port;
	memcpy((caddr_t)dst, (caddr_t)&target, sizeof(target));

	if (broadcastflag) 
	{
		int one = 1;
		r = setsockopt(sd, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one));
		if ( r<0 ) 
		{ 
			perror("multicast mode socket setup 1"); 
		}
		printf("multicast mode r %d\n", r);
	}

rcv_setup:
	from.sin_family = AF_INET;
	from.sin_port = port;
	from.sin_addr.s_addr = htonl(INADDR_ANY);
	local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	r = bind(sd, (struct sockaddr *)&from, sizeof(from));
	if (r<0) 
	{
		perror("bind call failed");
		exit(-1);
	}
	return(r);
}



void
setup_socket(char *name)
{
	int dscp = 0;
	int stype = SOCK_DGRAM;
	int sproto = 0;
	int r;

	//printf( "setup_socket: entering\n");

	if (phoneflag) 
	{
		dscp = TOS_BK;
	}
	if (upsdflag || apflag || aptsflag || stautflag) 
	{
		dscp = TOS_VO;
	}

	if (iphdrflag) 
	{
		stype = SOCK_RAW;
		sproto = IPPROTO_UDP;
	}

	if ((sd=socket(AF_INET, stype, sproto)) < 0) 
	{
		perror("socket");
		exit(-1);
	}
	set_dscp(dscp);
	if ((r=setup_addr(name, &dst))<0) 
	{
		fprintf(stderr, "can't map address (%s)\n", name);
		exit(-1);
	}
}

/*
* Determine name of running program.
* Set global variables and defaults accordingly.
*/
int
parse_program(char *s)
{
	int n;

	msgsize = 1024;                             // default msg size
#ifdef __CYGWIN__
	sockflags = 0;				// no sockflags if compiled for cygwin
#else
	sockflags = MSG_DONTWAIT;
#endif
	for(n=strlen(s); n>0; n--) 
	{		// strip any leading pathname components
		if (s[n] == '/') 
		{
			s = &s[n+1];
			break;
		}
	}

	if (strcmp(s, "staut")==0) 
	{		
		// Station Under Test
		printf("STAUT mode\n");
		msgsize = 200;
		stautflag = 1;
		codec_sec = 1;
		codec_usec = 0;
		dscp = TOS_VO;
		return(STAUT);
	} 
	if (strcmp(s, "apts")==0) 
	{			// AP Test Station
		aptsflag = 1;
		broadcastflag = 1;  // A secondary broadcast address is setup but only used in some tests
		msgsize = 256;
		return(APTS);
	}
	if (strcmp(s, "ap")==0) 
	{			
		// ap echo mode
		printf("ap mode\n");
		apflag = 1;
		return(AP);
	}
	if (strcmp(s, "phone") == 0) 
	{		// emulate phone
		printf("phone mode\n");
		phoneflag = 1;
		msgsize = 200;
		codec_sec = 0;
		codec_usec = 10000;
		return(PHONE);
	}
	if (strcmp(s, "upsd") == 0) 
	{		// emulate upsd phone
		printf("upsd phone\n");
		upsdflag = 1;
		upsdstartflag = 1;
		msgsize = 200;
		return(UPSD);
	}
	if (strcmp(s, "pgen") == 0) 
	{		
		// basic pgen
		printf("pgen mode port(%d)\n", port);
		pgenflag = 1;
		codec_sec = 2;
		codec_usec = 0;
		port++;
		return(PGEN);
	}
	if (strcmp(s, "pget" ) == 0) 
	{		// basic pget
		printf("pget mode on port(%d)\n", port);
		pgetflag = 1;
		port++;
		return(PGET);
	} 

	printf("Error: Could not match executable name (%s)\n", s);
	exit(-1);

}

//print_iphdr(struct iphdr *iph)
//{
//#ifndef __CYGWIN__
//    printf("tos %x\n", iph->tos);
//#endif
//    mpx("", iph, 32);
//}

void
do_pget()
{
	int r, flags, n, i, id;
	float elapsed;
#define	NCOUNTS 5
	int counts[NCOUNTS];


	n = 0;
	gettimeofday(&time_start, 0);

	while (1) 
	{
		fromlen = sizeof(from);
		r = recvfrom(sd, rmsg, sizeof(rmsg), flags, (struct sockaddr *)&from, &fromlen);
		gettimeofday(&time_stop, 0);

		if ( r < 0 ) 
		{
			continue;   // Nothing was received
		}

		id = dscp_to_acc(rmsg[1]);

		if (histflag==0) 
		{
			printf("r %d\t%x\t%d\t%d\n", rmsg[0], rmsg[1], id, rmsg[2]);
			//if (iphdrflag) {
			//print_iphdr((unsigned char *)rmsg);
			//}
			continue;
		}

		n++;

		elapsed = ftimediff(&time_stop, &time_start);

		counts[id]++;

		if (elapsed >= 2.0) 
		{
			time_start = time_stop;

			for(i=0; i<NCOUNTS; i++) 
			{
				printf("%d\t", counts[i]);
				counts[i] = 0;
			}
			printf("[%d]\n", n);
			n = 0;
		}
	}
#undef NCOUNTS
}

void
do_pgen()
{
	if (pgenflag) 
	{

		printf("PGEN: msgsize(%d) period(%s)\n", msgsize, period);
		wstate = WAIT_NEXT_CODEC;
		setup_period(period);

		while (floodflag || buzzperiod) 
		{
			send_txmsg(-1);
			buzz(buzzperiod);
		}

		setup_timers();				// enable timers
		timeout();				// stimulate first transmission
		while (1) 
		{				// repetitive sleep, interrupted by timeouts
			sleep(10);
		}
	}
}
void
do_ap()
{
	//unsigned long rmsg[2048];
	int r, flags, n, i, id;

	while (1) 
	{

		fromlen = sizeof(from);
		r = recvfrom(sd, rmsg, sizeof(rmsg), flags, (struct sockaddr *)&from, &fromlen);
		if (r<0) 
		{
			perror("rcv error:");
		}
		//if (iphdrflag) {
		//    print_iphdr((unsigned char *)rmsg);
		//}
		//pb("from", (caddr_t)&from.sin_addr.s_addr, 4);
		//pb("sock:", (caddr_t)&from, fromlen);
		//printf("port %d\n", from.sin_addr.s_port);
		if (from.sin_addr.s_addr==0 || from.sin_addr.s_addr==local.sin_addr.s_addr) 
		{
			continue;
		}
		r = sendto(sd, rmsg, r, sockflags, (struct sockaddr *)&from, sizeof(from));
		//printf("AP: snd %d\n", r);
	}
}


struct apts_msg *
	apts_lookup(char *s)
{
	struct apts_msg *t;

	for (t=&apts_msgs[APTS_TESTS]; s && t->cmd; t++) 
	{
		if (t->name && strcmp(t->name, s)==0) 
		{
			return(t);
		}
		//if ( traceflag) printf("APTS Test(%s)\n", t->name);
	}
	fprintf(stderr, "APTS Test(%s) unknown\n", s);
	fprintf(stderr, "available tests are:\n");
	for (t=&apts_msgs[APTS_TESTS]; t->cmd; t++) 
	{
		if (t->name) 
		{
			fprintf(stderr, "\t%s\n", t->name);
		}
	}
	exit(-1);
}

void
new_station(struct apts_msg *t)
{
	struct station *sta;
	int id, r;

	id = assign_sta_id(from.sin_addr.s_addr);
	sta = &stations[id];
	bzero(sta->ipaddress, 20);
char *ipp = (char *)inet_ntoa(from.sin_addr);
 strcpy( &(sta->ipaddress[0]), ipp);
 
	sta->cookie = 0;
	sta->nsent = 0;
	sta->nerr = 0;
	sta->alreadyCleared = 0;
	sta->nend = t->param2;			// number of message exchanges for this test
	t->param3 = id;				// send id to staut as param3
	create_apts_msg(t->cmd, txmsg);
	r = sendto(sd, txmsg, 190, sockflags, (struct sockaddr *)&from, sizeof(from));
	printf("new_station: size(%d) id=%02d IP address:%s\n", r, id, sta->ipaddress);
	if (traceflag) mpx("CMD send\n", txmsg, 64);
	nsta++;
	printf( "New STA = %d\n", nsta );
}


/*
* AP TS mode
*	Do initial handshake with STAUT:
*		recv HELLO
*		send TEST parameters
*		recv HELLO_CONFIRM
*	run APTS side of the test
*/
void
do_apts()
{
	//unsigned long rmsg[2048];
	int r, flags, n, i, id;
	struct apts_msg *t;
	struct station *sta;

	t = apts_lookup(targetnames[0]);
	flags = apts_state = 0;
	fromlen = sizeof(from);
	nsent = nrecv = 0;
	nend  = t->param0;
	nup   = t->param1;
	ndown = t->param2;
	printf("APTS(%s) Exchanges: %d  Up/exch: %d  Down/exch: %d \n", t->name, nend, nup, ndown);

	while (aptsflag) 
	{
		r = recvfrom(sd, rmsg, sizeof(rmsg), flags, (struct sockaddr *)&from, &fromlen);
		if (r<0) 
		{
			perror("rcv error:");
			exit(1);
		}
		if ( apts_state != 2 ) nrecv++;
		if (traceflag) 
		{
			printf( "APTS Received # %d   length:%d\n", nrecv, r );
			mpx("APTS RX", rmsg, 64);
		}

		// Do not process unless from remote
		if (from.sin_addr.s_addr==0 || from.sin_addr.s_addr==local.sin_addr.s_addr) 
		{
			printf( "Received 0 / local\n" );
			continue;
		}
		if (from.sin_addr.s_addr==target.sin_addr.s_addr) 
		{
			printf( "Received BROADCAST\n" );
			continue;
		}
		if (rmsg[10]==APTS_BCST) 
		{
			printf( "Received BROADCAST, skipping\n" );
			continue;
		}

		switch(apts_state) 
		{
		case 0:							
			// expecting HELLO
			if (rmsg[10] == APTS_HELLO) 
			{
			  new_station(t);
				pbaddr("HELLO from ", from.sin_addr.s_addr);
				apts_state = 1;
			}
			continue;
		case 1:							
			// expecting CONFIRM
			if (rmsg[10]==APTS_CONFIRM) 
			{
				id = rmsg[9];
				if (traceflag) printf("Confirm ID(%d)\n", id);
				if (t->cmd == L_1) 
				{ 
					apts_state = 3; 
					bsc_file = fopen("temp.bsc","w+");
					if (!bsc_file) 
					{
						printf("Could not open file temp.bsp\n");
					}
				}
				else 
				{
					apts_state = 2;
				}
			}
			continue;
		case 2:							
			// expecting AC Check Message(s) but don't echo
			if (rmsg[10]==APTS_CK_BE) { dscp = TOS_BE; }
			if (rmsg[10]==APTS_CK_BK) { dscp = TOS_BK; }
			if (rmsg[10]==APTS_CK_VI) { dscp = TOS_VI; }
			if (rmsg[10]==APTS_CK_VO) { dscp = TOS_VO; }

			if ( t->cmd == B_M ) 
			{
				create_apts_msg(APTS_STOP, txmsg);
				set_dscp(TOS_BE); 
				r = sendto(sd, txmsg, r, sockflags, (struct sockaddr *)&from, sizeof(from));
				printf("send_STOP_msg: size(%d) \n", r);
				mpx("APTS send stop\n", txmsg, 64);
				if (traceflag) mpx("APTS send stop\n", txmsg, 64);
				exit(0);
			}

			if (rmsg[10]==APTS_CK_BK) 
			{                     
				// CK_BK is the last expected startup msg: goto apts_state==3
				switch(t->cmd) 
				{
				case B_Z:
					set_dscp(TOS_VI);
					r = sendto(sd, rmsg, r, sockflags, (struct sockaddr *)&from, sizeof(from));
					printf("send_B_Z_msg: size(%d) \n", r);
					ldscp = dscp;
					apts_state = 4;
					continue;
				} // switch
				apts_state = 3;
			}
			continue;
		case 3:							
			// expecting data to be echoed
			if (traceflag) printf( "nsent %d %d  nrecv %d %d\n", nsent, nend*ndown, nrecv-3, nend*nup );

			if ( (nsent >= (nend*ndown)) && ((nrecv-3) >= (nend*nup)) && (nend>0) ) 
			{
				create_apts_msg(APTS_STOP, txmsg);
				set_dscp(TOS_BE); 
				r = sendto(sd, txmsg, r, sockflags, (struct sockaddr *)&from, sizeof(from));
				printf("send_STOP_msg: size(%d) \n", r);
				mpx("APTS send stop\n", txmsg, 64);
				if (traceflag) mpx("APTS send stop\n", txmsg, 64);
				exit(0);
			}

			switch(t->cmd) 
			{
			case L_1:						// load test
				switch(rmsg[10]) 
				{
				case APTS_HELLO:				// accept multiple stations
					pbaddr("HELLO from ", from.sin_addr.s_addr);
					new_station(t);
					continue;
				case APTS_CONFIRM:				// new sta will send a confirm
					continue;
				case APTS_CK_BE:
					dscp = TOS_BE; continue;
				case APTS_CK_BK:
					dscp = TOS_BK; continue;
				case APTS_CK_VI:
					dscp = TOS_VI; continue;
				case APTS_CK_VO:
					dscp = TOS_VO; continue;
				case ULOAD:					// fall through to the test exchange
					break;
				default:
					continue;
				}

				// find which sta sent the message
				sta = lookup_sta(rmsg);           
				sta->nend = 3000;              	
				if ( (sta->cookie == rmsg[3]) || (sta->cookie == rmsg[3]+1) || sta->nrecv==0) 
				{		
					// check if cookie was returned to us (except on first message)
					sta->nrecv++;   // bump the count for the station
					if (sta->nrecv == 1) 
					{
						pbaddr("STAUT START", from.sin_addr.s_addr);
						gettimeofday(&sta->tstart, 0);
					}
				} 
				else 
				{
					sta->nerr++;                                    // count number of incorrect cookies
				}

				if ( traceflag || !((sta->nrecv)%100) ) 
				{               // Comfort print
					printf("STA(%d) sent(%4d) recv(%4d) nend(%4d) r0 %4d \n", rmsg[4], sta->nsent, sta->nrecv, sta->nend, rmsg[0]);
				} 
				if (sta->nrecv >= sta->nend && !sta->alreadyCleared) 
				{                          
					// stop when we've received enough from each sta
					float elapsed;

					gettimeofday(&sta->tstop, 0);
					elapsed = ftimediff(&sta->tstop, &sta->tstart);
					printf("REcev: %d, NEnd: %d\n", sta->nrecv, sta->nend);
					pbaddr("STAUT STOP", from.sin_addr.s_addr);
					printf("L.1: time %.2fsec nerr(%d) nrecv(%d)\n", elapsed, sta->nerr, sta->nrecv);
					fprintf( bsc_file, "L.1: time (%.2f)sec nerr(%d) nrecv(%d) IPAddr(%s)\n", elapsed, sta->nerr, sta->nrecv, sta->ipaddress);

					create_apts_msg(APTS_STOP, txmsg);
					set_dscp(TOS_BE); 
					r = sendto(sd, txmsg, r, sockflags, (struct sockaddr *)&from, sizeof(from));
					clear_sta_id(sta);
					if ( !nsta ) 
					{
						fclose(bsc_file);
						exit( 0 );
					}
				} 
				else 
				{
					create_apts_msg(DLOAD, txmsg);                  // echo msg down to sta
					sta->cookie = txmsg[0] = sta->nrecv;            // add downlink "content" and save value in sta->cookie
					r = sendto(sd, txmsg, r, sockflags, (struct sockaddr *)&from, sizeof(from));
					sta->nsent++;
				}
				continue;
			case B_4:
				nrecv++;
				// B.4 receives 2 frames before loading next buffer frame
			case B_D:
			case M_D:
			case M_L:
			case M_I:
				nsent++;
				r = sendto(sd, rmsg, r, sockflags, (struct sockaddr *)&from, sizeof(from));
				printf("send_rxmsg: size(%d) nsent=%02d\n", r, nsent);
				if (traceflag) mpx("APTS send\n", rmsg, 64);
				continue;

			case M_V:
			case M_U:
			case A_U:
				nsent++;
				set_dscp(rmsg[1]); // palm: set dscp to received dscp value
				printf( "M_VU Received DSCP value=0x%X\n", rmsg[1] );
				r = sendto(sd, rmsg, r, sockflags, (struct sockaddr *)&from, sizeof(from));
				printf("send_rxmsg: size(%d) nsent=%02d\n", r, nsent);
				if (traceflag) mpx("APTS send\n", rmsg, 64);
				continue;

			case M_Y:
			case A_Y:
				printf("Ytest type(%x)\n", rmsg[1]);
				if (rmsg[1] == TOS_VI) 
				{  
					nsent++;
					set_dscp(rmsg[1]); // palm: set dscp to received dscp value
					r = sendto(sd, rmsg, r, sockflags, (struct sockaddr *)&from, sizeof(from));
					printf("A/M_Y echo_rxmsg: size(%d) nsent=%02d\n", r, nsent);
					if (traceflag) mpx("APTS send\n", rmsg, 64);
				}
				if (rmsg[1] == TOS_BE) 
				{  
					nsent++;
					set_dscp(rmsg[1]); // palm: set dscp to received dscp value
					r = sendto(sd, rmsg, r, sockflags, (struct sockaddr *)&from, sizeof(from));
					printf("A/M_Y echo_rxmsg: size(%d) nsent=%02d\n", r, nsent);
					if (traceflag) mpx("APTS send\n", rmsg, 64);
					if(t->cmd == M_Y)
						apts_state = 4;
				}
				continue;
			case B_G:
			case M_G:
				if ( nrecv%2 ) 
				{
					nsent++;
					r = sendto(sd, rmsg, r, sockflags, (struct sockaddr *)&from, sizeof(from));
					printf("send_rxmsg: size(%d)\n", r);
					if (traceflag) mpx("APTS send\n", txmsg, 64);
				}
				continue;
			case B_H:
				nsent++;
				rmsg[14] = (nsent%10) + 0x20202030;
				r = sendto(sd, rmsg, r, sockflags, (struct sockaddr *)&from, sizeof(from));
				printf("send_rxmsg: size(%d)\n", r);
				if (traceflag) mpx("APTS send\n ", rmsg, 64);
				/* change the received message somewhat and send it as a second packet */
				rmsg[3] = nsent++;
				rmsg[14] = (nsent%10) + 0x20202030;
				r = sendto(sd, rmsg, r, sockflags, (struct sockaddr *)&from, sizeof(from));
				printf("send_rxmsg: size(%d)\n", r);
				if (traceflag) mpx("APTS send\n ", rmsg, 64);
				// due to some APs only delivers one packet per trigger, we'll need to end this process faster than
				// needed, so that those buffered packet won't be thrown away. to do so, we'll fake the counter here.
				nsent = 99;
				nrecv = 99;
				continue;
			case A_J:
				nsent++;
				set_dscp(TOS_VO); 
				rmsg[14] = (nsent%10) + 0x20202030;
				r = sendto(sd, rmsg, r, sockflags, (struct sockaddr *)&from, sizeof(from));
				printf("send_rxmsg1: size(%d)\n", r);
				if (traceflag) mpx("APTS send 1\n ", rmsg, 64);
				/* change the received message somewhat and send it as 2nd packet */
				r++;
				rmsg[3] = nsent++;
				rmsg[14] = (nsent%10) + 0x20202030;
				set_dscp(TOS_VI); 
				r = sendto(sd, rmsg, r, sockflags, (struct sockaddr *)&from, sizeof(from));
				printf("send_rxmsg2: size(%d)\n", r);
				if (traceflag) mpx("APTS send 2\n ", rmsg, 64);
				/* change the received message somewhat and send it as 3rd packet */
				r++;
				rmsg[3] = nsent++;
				rmsg[14] = (nsent%10) + 0x20202030;
				set_dscp(TOS_BE); 
				r = sendto(sd, rmsg, r, sockflags, (struct sockaddr *)&from, sizeof(from));
				printf("send_rxmsg3: size(%d)\n", r);
				if (traceflag) mpx("APTS send 3\n ", rmsg, 64);
				/* change the received message somewhat and send it as 4th packet */
				r++;
				rmsg[3] = nsent++;
				rmsg[14] = (nsent%10) + 0x20202030;
				set_dscp(TOS_BK); 
				r = sendto(sd, rmsg, r, sockflags, (struct sockaddr *)&from, sizeof(from));
				printf("send_rxmsg4: size(%d)\n", r);
				if (traceflag) mpx("APTS send 4\n ", rmsg, 64);
				continue;
			case B_W:
			case M_W:
				// change condition since there is no cookie value in second recv'd packet
				if ( (my_cookie == rmsg[3]) || (my_cookie == rmsg[3]+1) || nrecv==0 || nrecv == 1) 
				{		
					// check if cookie was returned to us (except on first message)
					nrecv++;     // bump the count for the station (done above)
				} 
				else 
				{
					nerr++;                                    // count number of incorrect cookies
				}
				if ( (my_group_cookie == rmsg[2]) || (my_group_cookie == rmsg[2]+1) || ngrecv==0) 
				{		
					// check if cookie was returned to us (except on first message)
					ngrecv++;                                   // bump the count for the station
				} 
				else 
				{
					ngerr++;                                    // count number of incorrect cookies
				}
				if ( traceflag ) 
				{  
					printf("B_W sent(%d)  recv(%d) nend(%d)  nerr(%d) rmsg[0](%d) rmsg[3](%d)  cookie(%d) \n", 
						nsent, nrecv, nend, nerr, rmsg[0], rmsg[3], my_cookie );
					printf("B_W sent(%d) grecv(%d) nend(%d) ngerr(%d) rmsg[0](%d) rmsg[2](%d) gcookie(%d) \n", 
						nsent, ngrecv, nend, ngerr, rmsg[0], rmsg[2], my_group_cookie );
				} 

				nsent++;
				set_dscp(rmsg[1]); // palm: set dscp to received dscp value
				my_cookie =  rmsg[0] = nrecv;            
				// add downlink "content" and save value in cookie
				r = sendto(sd, rmsg, r, sockflags, (struct sockaddr *)&from, sizeof(from));
				printf("send_rxmsg: size(%d)\n", r);
				if (traceflag) mpx("APTS send\n ", rmsg, 64);
				/* change the received message somewhat and send it as a second packet */
				my_group_cookie =  rmsg[0] = ngrecv;            
				// add downlink "content" and save value in cookie
				rmsg[10] = APTS_BCST;
				r = sendto(sd, rmsg, r+10, sockflags, (struct sockaddr *)&dst, sizeof(dst));
				printf("send_bcast_msg: size(%d) BROADCAST\n", r);
				if (traceflag) mpx("APTS send BCST\n ", rmsg, 64);
				continue;
			case B_K:
			case M_K:
				// change condition since there is no cookie value in second recv'd packet
				if ( (my_cookie == rmsg[3]) || (my_cookie == rmsg[3]+1) || nrecv==0 || nrecv == 1) 
				{		
					// check if cookie was returned to us (except on first and second message)
					nrecv++;            // bump the count for the station (done above)
				} 
				else 
				{
					nerr++;                                    // count number of incorrect cookies
				}
				if ( traceflag ) 
				{  
					printf("B_K sent(%d)  recv(%d) nend(%d)  nerr(%d) rmsg[0](%d) rmsg[3](%d)  cookie(%d) \n", 
						nsent, nrecv, nend, nerr, rmsg[0], rmsg[3], my_cookie );
				} 

				nsent++;
				set_dscp(rmsg[1]); // palm: set dscp to received dscp value
				// add downlink "content" and save value in cookie-
				my_cookie =  rmsg[0] = nrecv;            
				r = sendto(sd, rmsg, r, sockflags, (struct sockaddr *)&from, sizeof(from));
				printf("send_rxmsg: size(%d)\n", r);
				if (traceflag) mpx("APTS send\n ", rmsg, 64);
				continue;
			case B_B:
			case M_B:
			case B_E:
				continue;
			}  // switch(t->cmd) 
			continue;
		case 4:							// Send Broadcast Message and STOP
			if (traceflag) printf( "nsent %d %d  nrecv %d %d\n", nsent, nend*ndown, nrecv-3, nend*nup );
			switch(t->cmd) 
			{
			case M_Y:
			case A_Y:
				if (rmsg[1] != TOS_BE) 
				{
					printf("Y test, got (%x), skipping\n", rmsg[1]);
					continue;
				}
			case B_Z:                                       // Send Broadcast
				nsent++;
				usleep( 500000) ;
				r = sendto(sd, rmsg, r, sockflags, (struct sockaddr *)&dst, sizeof(dst));
				printf("send_bcast_msg: size(%d) BROADCAST\n", r);

				usleep( 500000) ;
				create_apts_msg(APTS_STOP, txmsg);
				set_dscp(TOS_BE); 
				r = sendto(sd, txmsg, r, sockflags, (struct sockaddr *)&from, sizeof(from));
				printf("send_STOP_after_BROADCAST_msg: size(%d) \n", r);
				exit( 0 );
			}  // switch(t->cmd) 
		}
	} // while
}//APTS

/*
* STAUT mode
*/
void
do_staut()
{
	//unsigned long rmsg[2048];
	int r, flags, n, i, id, msg;

sta_start:
	sta_state = 0;
	msgno = 0;
	setup_timers();
	fromlen = sizeof(from);
	set_pwr_mgmt("STAUT", P_OFF);
	create_apts_msg(APTS_HELLO, txmsg);		// send HELLO
	wstate = WAIT_STUAT_00;
	timeout();

	while (stautflag) 
	{

		while ( sta_state==0 ) 
		{ 			
			// do startup exchange with APTS

			r = recvfrom(sd, rmsg, sizeof(rmsg), sockflags, (struct sockaddr *)&from, &fromlen);
			if (r<0) 
			{
				//perror( "STUAT Waiting for HELLO or COMMAND" );
				//sleep( 1 );
				continue;                                 // Waiting for HELLO or COMMAND
			}
			signal(SIGALRM, SIG_IGN);                   // Turn off TX timer
			sta_test = rmsg[10];
			if (traceflag) mpx("STA recv\n", rmsg, 64);

			switch(sta_test) 
			{				               
				// respond with CONFIRM and start test
			case APTS_HELLO_RESP:
			case L_1:
				my_sta_id = rmsg[9];
				apts_msgs[APTS_CONFIRM].param3 = my_sta_id;	// echo back assigned sta id 
			case B_D:
			case M_D:
			case B_H:
			case B_B:
			case M_B:
			case B_K:
			case M_K:
			case M_L:
			case M_G:
			case M_I:
			case B_Z:
			case M_Y:
			case M_V:
			case M_U:
			case A_U:
			case A_Y:
			case B_W:
			case M_W:
			case A_J:
			case B_M:
				break;
			default:
				if ( traceflag ) printf( "Error sta_test=%d \n", sta_test );
				continue;
			}
			create_apts_msg(APTS_CONFIRM, txmsg);
			r = sendto(sd, txmsg, msgsize, sockflags, &dst, sizeof(dst));
			sta_state = 2;
		}


		set_pwr_mgmt("STAUT", P_ON);

		if(sta_test != B_M)
		  {
		    create_apts_msg(APTS_CK_VO, txmsg);
		    send_txmsg(TOS_VO);
		    usleep( 100000 );
		    create_apts_msg(APTS_CK_VI, txmsg);
		    send_txmsg(TOS_VI);
		    usleep( 100000 );
		    create_apts_msg(APTS_CK_BE, txmsg);
		    send_txmsg(TOS_BE);
		    usleep( 100000 );
		    create_apts_msg(APTS_CK_BK, txmsg);
		    send_txmsg(TOS_BK);
		  }
		else // B_M test
		  {
		    sleep(30);
		    create_apts_msg(APTS_CK_VI, txmsg);
		    send_txmsg(TOS_VI);

		    sleep(1);

		    // retrieve the STOP command and exit
		    create_apts_msg(APTS_CK_BE, txmsg);
		    send_txmsg(TOS_BE);

		    r = recvfrom(sd, rmsg, sizeof(rmsg), sockflags, (struct sockaddr *)&from, &fromlen);
		    if ( r < 0 ) 
		    {
			//printf( "STAUT: r < 0\n");
		    }
		    else if ( rmsg[10] == APTS_STOP ) 
		    { 
			printf("STAUT: STOP\n");
		    }
		    signal(SIGALRM, SIG_IGN);          // Turn off TX timer
		    set_pwr_mgmt("STAUT", P_OFF);
		    exit( 0 );
		  }

		sleep( 1 );

		printf("STAUT: starting test %d  %s Ex:%d Up:%d Dn:%d\n", 
			sta_test, apts_msgs[sta_test].name, apts_msgs[sta_test].param0, 
			apts_msgs[sta_test].param1, apts_msgs[sta_test].param2  );

		msg = APTS_DEFAULT;
		wstate = WAIT_STUAT_02;
		switch (sta_test) 
		{
		case L_1:				// VO load test
			period = "20000";		// set to 20ms
			prcvflag = 0;			// turn off rcv print messages
			msg = ULOAD;                    // override default msg
			wstate = WAIT_STUAT_VOLOAD;	// use VOLOAD timer
		case B_D:
		case M_D:
		case B_B:				// 
		case B_K:				// 
		case M_B:				// 
		case M_K:				// 
		case B_H:				// 
		case B_W:				// 
		case M_W:				// 
		case M_L:
		case M_G:
		case M_I:
		case B_Z:
		case M_Y:
		case M_V:
		case M_U:
		case A_U:
		case A_Y:
		case A_J:				// 
			if (sta_test == B_E) wstate=WAIT_STUAT_0E; // send two uplink frame
			if (sta_test == B_G) wstate=WAIT_STUAT_0E; // send two uplink frame
			if (sta_test == M_G) wstate=WAIT_STUAT_0E; // send two uplink frame
			if (sta_test == M_I) wstate=WAIT_STUAT_0E; // send two uplink frame
			if (sta_test == B_B) wstate=WAIT_STUAT_SEQ;
			if (sta_test == B_K) wstate=WAIT_STUAT_SEQ;
			if (sta_test == M_B) wstate=WAIT_STUAT_SEQ;
			if (sta_test == M_K) wstate=WAIT_STUAT_SEQ;
			if (sta_test == M_U) wstate=WAIT_STUAT_SEQ;
			if (sta_test == M_V) wstate=WAIT_STUAT_SEQ;
			if (sta_test == M_W) wstate=WAIT_STUAT_SEQ;
			if (sta_test == M_Y) wstate=WAIT_STUAT_SEQ;
			if (sta_test == A_U) wstate=WAIT_STUAT_SEQ;
			if (sta_test == A_Y) wstate=WAIT_STUAT_SEQ;

			create_apts_msg(msg, txmsg);
			nrecv = 0;				// start counting test messages 
			nsent = 0;
			setup_period(period);
			if (sta_test == A_J ) 
			{
				codec_sec = 5;
			}
			// time between sending frames may be reduced for M.W, hence the following
			if (sta_test == M_W ) 
			{
				codec_sec = 0;
				codec_usec = 500000; // 150 milli Seconds
			}
			if ( traceflag ) printf( "Timers: (%d) sec (%d) usec\n", codec_sec, codec_usec );
			setup_timers();                     // Setup periodic transmission
			timeout();                          // Send one right away

			if (sta_test == A_J) 
			{
				sleep( 1 );
				set_pwr_mgmt("STAUT", P_OFF);
				timeout();                          // Send one right away
			}

			fromlen = sizeof(from);

			while (1) 
			{                        
				// Loop in a test receiving frames until receive STOP
				// Transmission of frames occurs by timer interrupt

				r = recvfrom(sd, rmsg, sizeof(rmsg), sockflags, (struct sockaddr *)&from, &fromlen);
				if ( r < 0 ) 
				{
					//printf( "STAUT: r < 0\n");
					continue;
				}
				nrecv++;
				my_cookie = rmsg[0];
				if ( !(rmsg[0]%100) ) printf( "cookie: %4d\n", rmsg[0] ); 

				if (traceflag) mpx("STA recv\n", rmsg, 64);

				if (prcvflag) 
				{
					gettimeofday(&time_ap, 0);
					time_delta = timediff(&time_ap, &time_ul);
					printf("rcv(%d) msgno(%3d) cmd(%2d) ", r, rmsg[0], rmsg[10]);
					mptimeval("delta ", &time_delta);
				}


				if ( rmsg[10] == APTS_STOP ) 
				{ 
					printf("STAUT: STOP\n");
					signal(SIGALRM, SIG_IGN);          // Turn off TX timer
					if ( staloopflag ) 
						goto sta_start;
					else 
					{
						set_pwr_mgmt("STAUT", P_OFF);
						exit( 0 );
					}
				}

			}// while(1)
		}	    // switch( sta_test)
	} // while (stautflag)
}		    // do_staut

void
do_phone()
{
	//unsigned long rmsg[2048];
	int r, flags, rmsgno;
	struct histogram h;

	if (phoneflag||upsdflag) 
	{

		setup_period(period);
		setup_timers();

		if (histflag) 
		{
			h.nsamples = 1000;
		}
		wstate = WAIT_NEXT_CODEC;

		timeout();                  // kick off first uplink packet 

		while (1) 
		{

			fromlen = sizeof(from);
			r = recvfrom(sd, rmsg, sizeof(rmsg), flags, (struct sockaddr *)&from, &fromlen);

			rmsgno = rmsg[0];
			if (rmsgno != msgno_ul) 
			{
				rmiss++;
				continue;
			}

			gettimeofday(&time_ap, 0);
			time_delta = timediff(&time_ap, &time_ul);

			if (histflag) 
			{
				dohist(&h, time_delta.tv_usec);
			} 
			else 
			{
				printf("r%d msg(%d) ", r, rmsgno);
				mptimeval("delta ", &time_delta);
			}

			//setitimer(ITIMER_REAL, &waitval_cancel, NULL);
			//wstate = WAIT_NEXT_CODEC;
			//setitimer(ITIMER_REAL, &waitval_codec, NULL);
			sleep(rcv_sleep_time);
		}
	}
}


main(int argc, char **argv)
{
	int i;

	which_program = parse_program(argv[0]);	// which program is running

	for(i=1; i<argc; i++) 
	{			// parse command line options
		if (argv[i][0] != '-') 
		{
			if (EQ(argv[i], "?") || EQ(argv[i], "help") || EQ(argv[i], "-h")) 
			{
				goto helpme;
			}

			targetname = argv[i];			// gather non-option args here
			if (ntarg < NTARG) 
			{
				targetnames[ntarg++] = targetname;
			}
			continue;
		}
		if (EQ(argv[i], "-codec") || EQ(argv[i], "-c") || EQ(argv[i], "-period")) 
		{
			i++;
			period = argv[i];
			continue;
		}
		if (EQ(argv[i], "-bcstIP") ) 
		{
			i++;
			bcstIpAddress = argv[i];
			bcstIpAddressDefined = 1;
			continue;
		}
		if (EQ(argv[i], "-flood")) 
		{
			floodflag = 1;
			continue;
		}
		if (EQ(argv[i], "-count") || EQ(argv[i], "count")) 
		{
			i++;
			count = argv[i];
			nend = atoi(count);
			continue;
		}
		if (EQ(argv[i], "-size") || EQ(argv[i], "size")) 
		{
			i++;
			msgsize = atoi(argv[i]);
			continue;
		}
		if (EQ(argv[i], "-hist")) 
		{
			histflag = 1;
			continue;
		}
		if (EQ(argv[i], "-wmeload")) 
		{
			wmeloadflag = 1;
			continue;
		}
		if (EQ(argv[i], "-broadcast")) 
		{
			broadcastflag = 1;
			continue;
		}
		if (EQ(argv[i], "-qdata")) 
		{
			qdataflag = 1;
			continue;
		}
		if (EQ(argv[i], "-qdis")) 
		{
			qdisflag = 1;
			continue;
		}
		if (EQ(argv[i], "-tspec")) 
		{
			tspecflag = 1;
			continue;
		}
		if (EQ(argv[i], "-upsd")) 
		{
			upsdflag = 1;
			continue;
		}
		if (EQ(argv[i], "-ip")) 
		{
			iphdrflag = 1;
			continue;
		}
		if (EQ(argv[i], "-t") || EQ(argv[i], "-trace")) 
		{
			traceflag = 1;
			continue;
		}
		if ( EQ(argv[i], "-staloop")) 
		{
			staloopflag = 1;
			continue;
		}
		if (EQ(argv[i], "-brcm") || EQ(argv[i], "-BRCM")) 
		{
			brcmflag = 1;
			continue;
		}
		if (EQ(argv[i], "-athr") || EQ(argv[i], "-ATHR")) 
		{
			athrflag = 1;
			continue;
		}
		if (EQ(argv[i], "-cnxt") || EQ(argv[i], "-CNXT")) 
		{
			cnxtflag = 1;
			continue;
		}
		if (EQ(argv[i], "-intel") || EQ(argv[i], "-intc") || EQ(argv[i], "-INTC")) 
		{
			intelflag = 1;
			continue;
		}
		if (EQ(argv[i], "-mrvl") || EQ(argv[i], "-MRVL") || EQ(argv[i], "-marvell"))
		{
			mrvlflag = 1;
			continue;
		}
		if (EQ(argv[i], "-ralink") || EQ(argv[i], "-RALINK")) 
		{
			ralinkflag = 1;
			continue;
		}
		if (EQ(argv[i], "-winb") || EQ(argv[i], "-WINB")) 
		{
			winbflag = 1;
			continue;
		}
        if (EQ(argv[i], "-product")) {
            i++;
            companyProduct = atoi(argv[i]);
            continue;
        }
		if (EQ(argv[i], "-h")) 
		{
helpme:
			printf("-codec\n-bcstIP\n-period\n-hist\n-wmeload\n-broadcast\n-count\n-qdata\n-qdis\n-tspec\n");
			exit(0);
		}
	}

	if (ntarg==0 && !apflag && !pgetflag) 
	{	// check arg count
		if (which_program == APTS) 
		{
			apts_lookup(0);
		}
		fprintf(stderr, "need at least one target address or test name\n");
		exit(-1);
	}


	setup_socket(targetname);

	switch(which_program) 
	{
	case APTS:
		usleep( 400000 );
		do_apts(); break;
	case STAUT:
		do_staut(); break;
	case PHONE:
	case UPSD:
		do_phone(); break;
	case AP:
		do_ap(); break;
	case PGEN:
		do_pgen(); break;
	case PGET:
		do_pget(); break;
	}
}

set_pwr_mgmt(char *msg, int mode)
{
	static int last_mode = -1;

	if (last_mode == mode) 
	{
		return;
	}

	if (brcmflag) 
	{			// BRCM specific
		if (mode==P_OFF) 
		{
			if ( system("./wl.exe PM 0")  < 0 ) 
			{
				printf("%s: BRCM wl power-save call error\n", msg); return;
			} 
			else 
			{
				printf("%s: BRCM wl power-save OFF\n", msg);
			}
		}

		if (mode==P_ON) 
		{
			if ( system("./wl.exe PM 1")  < 0 ) 
			{
				printf("%s: BRCM wl power-save call error\n", msg); return;
			} 
			else 
			{
				printf("%s: BRCM wl power-save ON\n", msg);
			}
		}
	}

	else if (athrflag) 
	{			// ATHR specific
		if (mode==P_OFF) 
		{
            switch(companyProduct)
            {
            case 0:      // linux driver
			    if ( system("iwpriv ath0 sleep 0") < 0) {
				    printf("%s: ATHR powersave botch\n", msg); return;
			    } else {
				    printf("%s: ATHR power save OFF\n", msg);
			    }
                break;
                
            case 1: // dragon driver
                if ( system("wmiconfig -i eth1 --power maxperf") < 0) {
                    printf("%s: ATHR powersave botch\n", msg); return;
                } else {
                    printf("%s: ATHR power save OFF\n", msg);
                }
                break;
		    }
        }
		if (mode==P_ON) 
		{
            switch(companyProduct)
            {
            case 0:      // linux driver
			    if ( system("iwpriv ath0 sleep 1") < 0) 
			    { 
				    printf("%s: ATHR powersave botch\n", msg); return;
			    } 
			    else 
			    {
				    printf("%s: ATHR power save on\n", msg);
			    }
                break;
            case 1: //dragon driver
                if ( system("wmiconfig -i eth1 --power rec") < 0) {
                    printf("%s: ATHR powersave botch\n", msg); return;
                } else {
                    printf("%s: ATHR power save on\n", msg);
                }
                break;
		    }
        }
	}

	else if (cnxtflag) 
	{			// CNXT specific
		if (mode==P_OFF) 
		{
			if ( system("ndisoid set `ndisoid listnet|grep PRISM|cut -d' ' -f1` DOT11_OID_PSM 0") < 0) 
			{
				printf("%s: CNXT powersave botch\n", msg); return;
			} 
			else 
			{
				printf("%s: CNXT power save OFF\n", msg);
			}
		}
		if (mode==P_ON) 
		{
			if ( system("ndisoid set `ndisoid listnet|grep PRISM|cut -d' ' -f1` DOT11_OID_PSM 1") < 0) 
			{
				printf("%s: CNXT powersave botch\n", msg); return;
			} 
			else 
			{
				printf("%s: CNXT power save on\n", msg);
			}
		}
	}

	else if (intelflag) 
	{			// INTEL specific
		if (mode==P_OFF) 
		{
			if ( system("iuapsd 0") < 0) 
			{
				printf("%s: INTEL powersave botch\n", msg); return;
			} 
			else 
			{
				printf("%s: INTEL power save OFF\n", msg);
			}
		}
		if (mode==P_ON) 
		{
			if ( system("iuapsd 1") < 0) 
			{ 
				printf("%s: INTEL powersave botch\n", msg); return;
			} 
			else 
			{
				printf("%s: INTEL power save on\n", msg);
			}
		}
	}

	else if (mrvlflag) 
	{			// MRVL specific
		if (mode==P_OFF) 
		{
			if ( system("iwconfig eth1 power off") < 0) 
			{
				printf("%s: MRVL powersave botch\n", msg); return;
			} 
			else 
			{
				printf("%s: MRVL power save OFF\n", msg);
			}
		}
		if (mode==P_ON) 
		{
			if ( system("iwconfig eth1 power on") < 0) 
			{ 
				printf("%s: MRVL powersave botch\n", msg); return;
			} 
			else 
			{
				printf("%s: MRVL power save on\n", msg);
			}
		}
	}

	else if (ralinkflag) 
	{			// Ralink specific
		if (mode==P_OFF) 
		{
			if ( system("./raoid.exe pm 0") < 0) 
			{
				printf("%s: Ralink powersave botch\n", msg); return;
			} 
			else 
			{
				printf("%s: Ralink power save OFF\n", msg);
			}
		}
		if (mode==P_ON) 
		{
			if ( system("./raoid.exe pm 1") < 0) 
			{ 
				printf("%s: Ralink powersave botch\n", msg); return;
			} 
			else 
			{
				printf("%s: Ralink power save on\n", msg);
			}
		}
	}

	else if (winbflag) 
	{			// WINB specific
		if (mode==P_OFF) 
		{
			if ( system("./wb_set_ps_mode 0") < 0) 
			{
				printf("%s: Winbond powersave botch\n", msg); return;
			} 
			else 
			{
				printf("%s: Winbond power save OFF\n", msg);
			}
		}
		if (mode==P_ON) 
		{
			if ( system("./wb_set_ps_mode 1") < 0) 
			{ 
				printf("%s: Winbond powersave botch\n", msg); return;
			} 
			else 
			{
				printf("%s: Winbond power save on\n", msg);
			}
		}
	}

	else 
	{
		printf("%s: Unknown device for power save change\n", msg);
	}

	last_mode = mode;
}

