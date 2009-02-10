extern int getcrtchar (int x, int y);
extern void printc (int c, int x, int y, int color);

unsigned char const mkline_mat[]=""
	"\xff\xff\xffÄ\xff\xff\xffÍ"
	"\xffÚ¿Â\xffÕ¸Ñ"
	"\xffÀÙÁ\xffÔ¾Ï"
	"³Ã´Å³ÆµØ"
	"\xff\xff\xffÄ\xff\xff\xffÍ"
	"\xffÖ·Ò\xffÉ»Ë"
	"\xffÓ½Ğ\xffÈ¼Ê"
	"ºÇ¶×ºÌ¹Î";
//these characters are used in automatic frame lines intersection replacement
//algorithm by mkline_aux and mkline.

void mkline_aux (int cnt, int var, unsigned int mode, int pos, int color)
	//auxiliary function for mkline.
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
		mult=0x08;
	 }
	c=getcrtchar(x,y);
	for (c0=0;c0<0x40 && c!=mkline_mat[c0];c0++);
	if(c0==0x40)
		c0=0;
	if (c0==0)
		pos=3;
	c0=(c0|(pos*mult));
	if((mode&0x02)==0)
		c0=(c0&(-4*mult-1));
	 else
		c0=(c0|(4*mult));
	printc(mkline_mat[c0],x,y,color);
 }

void mkline (int cnt, int bgn, int end, int color, unsigned int mode)
//draws lines in menu boxs (text frames) with automatic character replacement
//when another frame character is found at position where is going to be
//printed the line.
 {
	unsigned c0;
	mkline_aux(cnt,bgn,mode,1,color);
	bgn++;
	for (c0=bgn;c0<end;c0++)
		mkline_aux(cnt,c0,mode,3,color);
	mkline_aux(cnt,end,mode,2,color);
 }
