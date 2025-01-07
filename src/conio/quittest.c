#include "ciolib.h"
#include "genwrap.h"

int main(int argc, char **argv)
{
	int mode = CIOLIB_MODE_AUTO;
	for (int i = 1; i < argc; i++) {
		switch(argv[i][0]) {
			case 'G':
				mode = CIOLIB_MODE_GDI_FULLSCREEN;
				break;
			case 'g':
				mode = CIOLIB_MODE_GDI;
				break;
			case 'S':
				mode = CIOLIB_MODE_SDL_FULLSCREEN;
				break;
			case 's':
				mode = CIOLIB_MODE_SDL;
				break;
			case 'X':
				mode = CIOLIB_MODE_X_FULLSCREEN;
				break;
			case 'x':
				mode = CIOLIB_MODE_X;
				break;
			default:
				puts("GgSsXx");
				exit(0);
		}
	}

	initciolib(mode);

	for (;;) {
		if(!kbhit())
			SLEEP(100);
		int k = getch();
		if (k == 0 || k == 0xe0)
			k = k | (getch() << 8);
		switch(k) {
			case 'q':
				exit(1);
			case CIO_KEY_QUIT:
				cputs("Got quit\n");
				break;
			default:
				cprintf("Key: %d (q to quit)\n", k);
				break;
		}
	}
}
