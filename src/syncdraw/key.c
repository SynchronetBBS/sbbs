#include <ciolib.h>

int 
newgetch(void)
{
	int             ch;

	ch = getch();
	if(ch==0 || ch==0xe0)
		ch|=getch()<<8;
	/* Input translation */
	switch(ch) {
	case 10:
		return(13);
	/* CTRL-x to ALT-Fx for ncurses terminals */
	case 17:
		return CIO_KEY_ALT_F(1);
	case 23:
		return CIO_KEY_ALT_F(2);
	case 5:
		return CIO_KEY_ALT_F(3);
	case 18:
		return CIO_KEY_ALT_F(4);
	case 20:
		return CIO_KEY_ALT_F(5);
	case 25:
		return CIO_KEY_ALT_F(6);
	case 21:
		return CIO_KEY_ALT_F(7);
	case 9:
		return CIO_KEY_ALT_F(8);
	case 15:
		return CIO_KEY_ALT_F(9);
	case 16:
		return CIO_KEY_ALT_F(10);
	}
	return ch;
}
