//////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2005  Andy Patti, Kevin Yu, Greg Chesson,
//    Atheros Communications
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
// above copyright notice, the name of the authors "Andy Patti", "Kevin Yu"
// and "Greg Chesson",
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
//////////////////////////////////////////////////////////////////////
#include <pcap.h>

unsigned long long get_hw_address(u_char **packetPtr,
				  u_char *dest,
				  int *pLen)
{
    int numToCopy=6;
    u_char *ptr;

    memset(&(dest[0]),0,6);
    ptr = *packetPtr;

    if (numToCopy > *pLen) {
        numToCopy = *pLen;
    }
    memcpy(dest,ptr,numToCopy);
    *pLen -= numToCopy;
    ptr += numToCopy;
    *packetPtr = ptr;
    return(0);
}


void print_hw_address(u_char *addr)
{
    int i;
    for (i=0;i<5;i++) {
        printf ("%x:",addr[i]);
    }
    printf ("%x",addr[5]);
}

void print_IPv4_address(u_char *addr)
{
    int i;
    for (i=0;i<3;i++) {
        printf ("%d.",addr[i]);
    }
    printf ("%d",addr[3]);
}

void print_IPv6_address(u_char *addr)
{
    int i;
    for (i=0;i<15;i++) {
        printf ("%d.",addr[i]);
    }
    printf ("%d",addr[i]);
}

int get_u_short(u_char **packetPtr,u_short *buf, int *pLen)
{
    u_char *ptr;
    int error=0;

    ptr = *packetPtr;
    if (*pLen > 0) {
        *buf = (u_short) *ptr++;
        *pLen -= 1;
    } else {
        error = 1;
    }
    if (*pLen > 0) {
        *buf |= ((u_short)  *ptr++) << 8;
        *pLen -= 1;
    } else {
        error = 1;
    }
    *packetPtr = ptr;
    return(error);
}

int get_u_long(u_char **packetPtr, u_long *buf, int *pLen) 
{
    u_char *ptr;
    int i,shift,error;

    ptr = *packetPtr;
    *buf = 0;
    for (i=0,shift=0;(i<4)&&(*pLen > 0);i++,shift+=8) {
        *buf |= ((u_long) *ptr++) << shift;
        *pLen -= 1;
    }
    if (i<4) {
        error = 1;
    }
    *packetPtr = ptr;
    return(error);
}

#define IS_ASCII_CHAR(_a) ((_a) >= 32 && (_a) <= 126)

void
pr_ether_data(u_char *s, int len)
{
	int n = 0;
	u_char c0, c1;
	char asciibuf[17], *ap;

	asciibuf[16] = 0;
	ap = &asciibuf[0];

        printf( "Data Length: (%d)\n", len ); // palm
 
	while (len) {
		c0 = *s++; len--;
		if (len) {
			c1 = *s++; len--;
		} else
			c1 = 0;

		printf("%02x%02x ", c0, c1);
		*(ap++) = IS_ASCII_CHAR(c0) ? c0 : '.';
		*(ap++) = IS_ASCII_CHAR(c1) ? c1 : '.';
		
		n++;
		if (n>7) {
			printf("    %s\n", asciibuf);
			ap = &asciibuf[0];
			n = 0;
		}
	}
	if (n) {
		int i;
		for (i=0; i < (16-2*n); i++) printf("  ");
		for (i=0; i <= (7-n); i++) printf(" ");
		asciibuf[n] = 0;
		printf("    %s\n", asciibuf);
	}
}
