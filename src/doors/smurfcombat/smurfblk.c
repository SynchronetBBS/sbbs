/***************************************************************************/
/* */
/* sss                       fff     ccc                 b               */
/* s   s                     f   f   c   c                b               */
/* s                         f       c                    b               */
/* sss  mmm mmm  u   u r rr  fff    c      ooo  mmm mmm   bbb  aaa  ttt  */
/* s m  m   m u   u rr   f       c     o   o m  m   m b   b a  a  t   */
/* s   s m  m   m u  uu r    f       c   c o   o m  m   m b   b aaaa  t   */
/* sss  m  m   m  uu u r    f        ccc   ooo  m  m   m  bbb  a  a  t   */
/* */
/* SYSOP Maintenance Code: !-S-Y-S-O-P-!                         BLOCKADE  */
/* REMOTE Maintenance Code: !-SIX-NINE-|                         BLOCKADE  */
/* */
/***************************************************************************/

extern char    *get_blk_err_msg(int num);





char           *
get_blk_err_msg(int num)
{
    char           *sp;
    static char    *blk_err_msgs[] = {
	"No error",		       /* 0  */
	"No memory for buffer",	       /* 1  */
	"Error opening file",	       /* 2  */
	"No memory for file buffering",
	"File buffering failure",      /* 4  */
	"Fseek error",
	"Error reading data block",    /* 6  */
	"Invalid data block info",
	"Error reading file",	       /* 8  */
	"Error reading file",	       /* 9  */
	"Error reading file",	       /* 10  */
	"Check Value error",	       /* 11  */
	"File size error",	       /* 12  */
    "Invalid error number"};	       /* 13  */
    if (num == -1)
	num = 11;
    if (num == -2)
	num = 12;
    if ((num < 0) || (num > 12))
	num = 13;		       /* out of range */
    sp = blk_err_msgs[num];
    return (sp);
}
