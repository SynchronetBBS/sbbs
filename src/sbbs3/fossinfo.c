/* Builds with Borland C++ for DOS, v3.1 */

#include <dos.h>
#include <stdio.h>
#include <memory.h>
#include <string.h>		/* _fstrncpy */
#include "fossdefs.h"

void main(int argc, char** argv)
{
	BYTE far*	bp;
	fossil_info_t info;
	WORD	ax,bx;
	char id_string[128];
	union REGS regs;
	struct SREGS sregs;

	printf("\nChecking FOSSIL interrupt vector (interrupt 0x%02X)\n", FOSSIL_INTERRUPT);
	bp=(void far*)_dos_getvect(FOSSIL_INTERRUPT);

	printf("FOSSIL interrupt vector: 0x%08lX\n",bp);
	printf("Signature: 0x%04X (should be 0x%04X)\n", *(WORD far*)(bp+6), FOSSIL_SIGNATURE);
	printf("Highest function supported: 0x%02X\n", *(BYTE far*)(bp+8));


	printf("\nInitializing FOSSIL\n");
	regs.h.ah = FOSSIL_FUNC_INIT;
	int86(FOSSIL_INTERRUPT, &regs, &regs);
	printf("AX=0x%04X (should be 0x%04X)\n", regs.x.ax, FOSSIL_SIGNATURE);
	printf("BX=0x%04X (FOSSIL rev %u, highest function supported: 0x%02X)\n"
		,regs.x.bx, regs.h.bh, regs.h.bl);

	printf("\nGetting FOSSIL Information\n");
	printf("sizeof(info)=%u\n",sizeof(info));
	memset(&info,0,sizeof(info));

/**
 AH = 1Bh    Return information about the driver

           Parameters:
               Entry:  CX = Size of user info buffer in bytes
                       DX = Port number
                       ES = Segment of user info buffer
                       DI = Offset into ES of user info buffer
               Exit:   AX = Number of bytes actually transferred

**/

	bp = (BYTE far*)&info;
	regs.h.ah = FOSSIL_FUNC_GET_INFO;
	regs.x.cx = sizeof(info);
	regs.x.dx = 0;
	sregs.es = FP_SEG(bp);
	regs.x.di = FP_OFF(bp);

	int86x(FOSSIL_INTERRUPT, &regs, &regs, &sregs);
	printf("AX=0x%04X (%u)\n",regs.x.ax,regs.x.ax);
	printf("Information structure size: %u\n", info.info_size);
    printf("FOSSIL specific revision: %u\n", info.curr_fossil);
    printf("FOSSIL driver revision: %u\n", info.curr_rev);
    printf("ID string: 0x%08lX\n",info.id_string);
	if(info.id_string) {
		_fstrncpy(id_string,(char far *)info.id_string,sizeof(id_string));
		printf("ID string: %s\n", id_string);
	}
    printf("Receive buffer size: %u\n", info.inbuf_size);
    printf("Receive buffer space: %u\n",info.inbuf_free);
    printf("Transmit buffer size: %u\n", info.outbuf_size);
    printf("Transmit buffer space: %u\n",info.outbuf_free);
    printf("Screen width: %u\n",info.screen_width);
    printf("Screen height: %u\n", info.screen_height);
    printf("Baud rate: 0x%02X (%u %c-%u-%u)\n"
		,info.baud_rate
		,fossil_baud_rate[(info.baud_rate&FOSSIL_BAUD_RATE_MASK)>>FOSSIL_BAUD_RATE_SHIFT]
		,fossil_parity[(info.baud_rate&FOSSIL_PARITY_MASK)>>FOSSIL_PARITY_SHIFT]
		,fossil_data_bits[(info.baud_rate&FOSSIL_DATA_BITS_MASK)>>FOSSIL_DATA_BITS_SHIFT]
		,fossil_stop_bits[(info.baud_rate&FOSSIL_STOP_BITS_MASK)>>FOSSIL_STOP_BITS_SHIFT]
		);

	if(argc>1 && stricmp(argv[1],"pause")==0) {
		printf("\nHit enter to continue...");
		getchar();
	}
}	
