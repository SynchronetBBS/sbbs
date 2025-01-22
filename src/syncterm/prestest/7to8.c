#include <stdio.h>

int
main(int argc, char **argv)
{
	unsigned char ch;

	while (fread(&ch, 1, 1, stdin) == 1) {
		if (ch == '\x1b') {
			if (fread(&ch, 1, 1, stdin) == 1) {
				if (ch >= 0x40 && ch <= 0x5f) {
					ch &= 0x3f;
					ch |= 0x80;
					fwrite(&ch, 1, 1, stdout);
				}
				else {
					fprintf(stderr, "Invalid escape 0x%02x\n", ch);
				}
			}
			else {
				fputs("read failure\n", stderr);
			}
		}
		else {
			fwrite(&ch, 1, 1, stdout);
		}
	}
}
