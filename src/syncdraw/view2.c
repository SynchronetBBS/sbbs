unsigned char   bitmap[64000];
unsigned char   font[256][9];
void 
viewmode(void)
{
	FILE           *fp;
	int             b = 0, ch, x, y;
	unsigned char   back, fore, chr;
	int             up, down;
	FILE           *fp;
	vga_setmode(G320x200x256);
	memset(&bitmap, 0, 64000);
	for (x = 0; x < 199; x++)
		bitmap[(x * 320) + 99] = 15;
	for (x = 0; x < 199; x++)
		bitmap[(x * 320) + 180] = 15;
	do {
		y = (b * 199) / (MaxLines - 199);
		for (x = y - 2; x <= y + 10; x++)
			if (x <= 199)
				bitmap[(x << 8) + (x << 6) + 90] = 0;
		for (x = y - 2; x <= y + 10; x++)
			if (x <= 199)
				bitmap[(x << 8) + (x << 6) + 91] = 0;
		bitmap[(y << 8) + (y << 6) + 90] = 14;
		bitmap[(y << 8) + (y << 6) + 91] = 14;
		y++;
		bitmap[(y << 8) + (y << 6) + 90] = 14;
		bitmap[(y << 8) + (y << 6) + 91] = 14;
		for (y = 0 + b; y < 99 + b; y++)
			for (x = 0; x <= 79; x++) {
				back = (Screen[ActivePage][y][(x << 1) + 1] & 112) >> 4;
				fore = Screen[ActivePage][y][(x << 1) + 1] & 15;
				chr = Screen[ActivePage][y][(x << 1)];
				up = (y - b) * 2 * 320 + 100 + x;
				down = (y - b) * 2 * 320 + 100 + x + 320;
				bitmap[up] = back;
				bitmap[down] = back;
				if (chr <= 175 && chr != 32 && chr != 0)
					bitmap[up] = fore;
				if (chr >= 224)
					bitmap[up] = fore;

				if ((chr >= 176 && chr <= 218) || chr == 219 || chr == 221 || chr == 222) {
					bitmap[up] = fore;
					bitmap[down] = fore;
				}
				if (chr == 220)
					bitmap[down] = fore;
				if (chr == 223)
					bitmap[up] = fore;
			}
		memcpy(graph_mem, &bitmap, 64000);
		ch = getch();
		if(ch==0 || ch==0xe0)
			ch|=getch()<<8;
		switch (ch) {
		case KEY_UP:
			if (b > 0)
				b -= 1;
			break;
		case KEY_DOWN:
			if (b + 50 < MaxLines)
				b += 1;
			break;
		}
	} while (ch != 27);
	vga_setmode(TEXT);
	clear();
	Statusline();
}
