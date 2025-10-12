#define EXTDRIVER
#include "gac_bj.h"
#undef EXTDRIVER

char    *UserSays="`Bright Magenta`%s `Magenta`says \"`Bright Magenta`%s`Magenta`\"\r\n";
char    *UserWhispers="`Bright Magenta`%s `Magenta`whispers \"`Bright Cyan`%s`Magenta`\"\r\n";
char    *ShoeStatus="\r\n`Bright White`Shoe: %u/%u\r\n";
// uchar   symbols=1;
char    autoplay=0;

card_t newdeck[52]={
	 2,H, 2,D, 2,C, 2,S,
	 3,H, 3,D, 3,C, 3,S,
	 4,H, 4,D, 4,C, 4,S,
	 5,H, 5,D, 5,C, 5,S,
	 6,H, 6,D, 6,C, 6,S,
	 7,H, 7,D, 7,C, 7,S,
	 8,H, 8,D, 8,C, 8,S,
	 9,H, 9,D, 9,C, 9,S,
	10,H,10,D,10,C,10,S,
	 J,H, J,D, J,C, J,S,
	 Q,H, Q,D, Q,C, Q,S,
	 K,H, K,D, K,C, K,S,
	 A,H, A,D, A,C, A,S };
