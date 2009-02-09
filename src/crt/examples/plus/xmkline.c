//mkline_mat4 is the enhanced version of mkline_mat
//mkline_aux4 is the enhanced version of mkline_aux
//mkline4 is the enhanced version of mkline

char const mkline_mat4[]="\
ÿ€Äÿ‚ƒÍÿˆ‰Šÿ\1\2\3\
„Ú¿Â„Õ¸Ñ„‹Œ„\4\5\6\
…ÀÙÁ…Ô¾Ï……\a\b\t\
³Ã´Å³ÆµØ³‘’“³\n\13\14\
ÿ€Äÿ‚ƒÍÿˆ‰Šÿ\1\2\3\
†Ö·Ò†É»Ë†”•–†\15\16\17\
‡Ó½Ğ‡È¼Ê‡—˜™‡\20\21\22\
ºÇ¶×ºÌ¹Îºš›œº\23\24\25\
ÿ€Äÿ‚ƒÍÿˆ‰Šÿ\1\2\3\
Ÿ ©ª«ãäå\26\27\30\
¡¢£¤¡¬­®¡æçè¡\31\32\33\
¥¦§¨¥àáâ¥éêë¥\34\35\36\
ÿ€Äÿ‚ƒÍÿˆ‰Šÿ\1\2\3\
\37!\"#\37,-.\37?@[\37bcd\
$%&'$/:;$\\]^$efg\
()*+(<=>(_`a(hij\
";
//these characters are used in automatic frame lines intersection replacement
//algorithm by mkline_aux3 and mkline3.

void mkline_aux4 (int cnt, int var, unsigned int mode, int pos, int color)
	//auxiliary function for mkline3.
 {
	unsigned int c0;
	unsigned int x,y;
	char mult;
	unsigned char c;
	if ((mode&0x01)==0)
	 {
		x=var;
		y=cnt;
		mult=0x01;
	 }
	 else
	 {
		x=cnt;
		y=var;
		mult=0x10;
	 }
	c=getcrtchar(x,y);
	for (c0=0;c0<0x100 && c!=mkline_mat4[c0];c0++);
	if(c0==0x100)
		c0=0;
	if (c0==0)
		pos=3;
	c0=(c0|(pos*mult));

	c0=(c0&(-12*mult-1));
	if((mode&0x06)==2)
		c0=(c0|(4*mult));
	else if ((mode&0x06)==4)
		c0=(c0|(8*mult));
	else if ((mode&0x06)==6)
		c0=(c0|(12*mult));

	printc(mkline_mat4[c0],x,y,color);
 }

void mkline4 (int cnt, int bgn, int end, int color, unsigned int mode)
//draws lines in menu boxes (text frames) with automatic character replacement
//when another frame character is found at position where is going to be
//printed the line.
 {
	unsigned c0;
	mkline_aux4(cnt,bgn,mode,1,color);
	bgn++;
	for (c0=bgn;c0<end;c0++)
		mkline_aux4(cnt,c0,mode,3,color);
	mkline_aux4(cnt,end,mode,2,color);
 }