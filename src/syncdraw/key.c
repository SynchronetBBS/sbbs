#include <ciolib.h>
#include <keys.h>

const char *altkeys="QWERTYUIOP!!!!ASDFGHJKL!!!!!ZXCVBNM";

int 
newgetch(void)
{
	int             ch;

	ch = getch();
	if(ch==0 || ch==0xff)
		ch|=getch()<<8;
	if (ch == 10)
		ch = 13;
	if (ch == 17)
		return CIO_KEY_ALT_F(1);
	if (ch == 23)
		return CIO_KEY_ALT_F(2);
	if (ch == 5)
		return CIO_KEY_ALT_F(3);
	if (ch == 18)
		return CIO_KEY_ALT_F(4);
	if (ch == 20)
		return CIO_KEY_ALT_F(5);
	if (ch == 25)
		return CIO_KEY_ALT_F(6);
	if (ch == 21)
		return CIO_KEY_ALT_F(7);
	if (ch == 9)
		return CIO_KEY_ALT_F(8);
	if (ch == 15)
		return CIO_KEY_ALT_F(9);
	if (ch == 16)
		return CIO_KEY_ALT_F(10);
	if (ch>=0x1000 && ch<=0x3200 && altkeys[(ch>>8)-16]!='!') {
		ungetch(altkeys[(ch>>8)-16]);
		return 27;
	}
	return ch;
}
