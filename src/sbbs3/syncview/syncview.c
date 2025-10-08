#include <stdio.h>
#include <string.h>

#include "dirwrap.h"
#include "genwrap.h"
#include "xpmap.h"

#include "ciolib.h"
#include "cterm.h"
#include "ripper.h"
#include "sauce.h"
#include "term.h"
#include "vidmodes.h"

#define SCROLL_LINES	100000
#define BUF_SIZE		1024

static int cvmode;
struct cterminal *cterm;

char *fnames[] = {
	"IBM VGA",
	"IBM VGA50",
	"IBM VGA25G",
	"IBM EGA",
	"IBM EGA43",
	"IBM VGA ",
	"IBM VGA50 ",
	"IBM VGA25G ",
	"IBM EGA ",
	"IBM EGA43 ",
	"Amiga Topaz 1",
	"Amiga Topaz 1+"
	"Amiga Topaz 2",
	"Amiga Topaz 2+"
	"Amiga P0T-NOoDLE",
	"Amiga MicroKnight"
	"Amiga MicroKnight+"
	"Amigal mOsOul",
	"C64 PETSCII unshifted",
	"C64 PETSCII shifted",
	"Atari ATASCII",
};

int fonts[] = {
	0,
	0,
	0,
	0,
	0,
	-1,
	-1,
	-1,
	-1,
	-1,
	42,
	40,
	42,
	40,
	37,
	41,
	39,
	32,
	33,
	36,
};

struct terminal term = {
	.height = 25,
	.width = 80,
	.nostatus = 1,
};

void
get_term_win_size(int *width, int *height, int *pixelw, int *pixelh, int *nostatus)
{
	struct  text_info txtinfo;
	int vmode;

	gettextinfo(&txtinfo);
	vmode = find_vmode(txtinfo.currmode);

	*width = vparams[cvmode].cols;
	*height = txtinfo.screenheight;

	if (vmode == -1) {
		if (pixelw)
			*pixelw = *width * 8;
		if (pixelh)
			*pixelh = *height * 16;
	}
	else {
		if (pixelw)
			*pixelw = *width * vparams[vmode].charwidth;
		if (pixelh)
			*pixelh = *height * vparams[vmode].charheight;
	}
}

int rgetch(int rip)
{
	if (rip)
		return rip_getch();
	return getch();
}

void viewscroll(int rip)
{
	int	top;
	int key;
	int i;
	struct	text_info txtinfo;

	gettextinfo(&txtinfo);
	if(cterm->backpos>cterm->backlines) {
		memmove(cterm->scrollback,cterm->scrollback+cterm->width,cterm->width*sizeof(*cterm->scrollback)*(cterm->backlines-1));
		cterm->backpos=cterm->backlines;
	}
	vmem_gettext(1,1,vparams[cvmode].cols,txtinfo.screenheight,cterm->scrollback+(cterm->backpos)*cterm->width);
	top=cterm->backpos;
	for(i=0;!i;) {
		if(top<1)
			top=1;
		if(top>cterm->backpos)
			top=cterm->backpos;
		vmem_puttext(1,1,vparams[cvmode].cols,txtinfo.screenheight,cterm->scrollback+(vparams[cvmode].cols*top));
		key=rgetch(rip);
		switch(key) {
			case 0xe0:
			case 0:
				switch(key|rgetch(rip)<<8) {
					case CIO_KEY_UP:
						top--;
						break;
					case CIO_KEY_DOWN:
						top++;
						break;
					case CIO_KEY_PPAGE:
						top-=txtinfo.screenheight;
						break;
					case CIO_KEY_NPAGE:
						top+=txtinfo.screenheight;
						break;
				}
				break;
			case 'j':
			case 'J':
				top--;
				break;
			case 'k':
			case 'K':
				top++;
				break;
			case 'h':
			case 'H':
				top-=txtinfo.screenheight;
				break;
			case 'l':
			case 'L':
				top+=txtinfo.screenheight;
				break;
			case '\033':
				i=1;
				break;
		}
	}
	return;
}

void lfexpand(char *buf, int *len)
{
	char	newbuf[BUF_SIZE*2];
	int		newlen=0;
	int		i;

	for(i=0; i<*len; i++) {
		if(buf[i]=='\n') {
			newbuf[newlen++]='\r';
		}
		newbuf[newlen++]=buf[i];
	}
	*len=newlen;
	memcpy(buf, newbuf, newlen);

	return;
}

void setFontByName(const char *fname)
{
	int f = 0;
	for (int i = 0; conio_fontdata[i].desc; i++) {
		if (strcmp(fname, conio_fontdata[i].desc) == 0) {
			f = i;
			break;
		}
	}
	setfont(f, false, 1);
	setfont(f, false, 2);
	setfont(f, false, 3);
	setfont(f, false, 4);
}

int conn_send(const void *buf, size_t len, unsigned timeout)
{
	return len;
}

int main(int argc, char **argv)
{
	struct text_info	ti;
	FILE	*f;
	char	buf[BUF_SIZE*2];	/* Room for lfexpand */
	int		len;
	int		speed=0;
	struct vmem_cell	*scrollbuf;
	char	*infile=NULL;
	char	title[MAX_PATH+1];
	int		expand=0;
	int		ansi=0;
	int		i;
	int		mode = C80;
	int		emulation = CTERM_EMULATION_ANSI_BBS;
	char		font_name[64] = "Codepage 437 English";
	double          scaling = 1.0;
	sauce_record_t  sauce = {0};
	int             setflags = 0;
	int             flags;
	char            stitle[SAUCE_LEN_TITLE + 1] = "";
	bool            saucy = false;
	off_t           stop;
	size_t          pos = 0;
	int             rip = RIP_VERSION_NONE;
	bool            bintext = false;

	/* Parse command line */
	for(i=1; i<argc; i++) {
		if(argv[i][0]=='-') {
			if(argv[i][1]=='l' && argv[i][2]==0)
				expand=1;
			else if(argv[i][1]=='s') {
				char *p = &argv[i][2];
				if (!*p) {
					i++;
					if (i >= argc)
						break;
					p = argv[i];
				}
				double mult = strtof(p, NULL);
				if (mult < 1 || mult > 1000) {
					fputs("Can't parse scaling value\n\n", stdout);
					goto usage;
				}
				scaling *= mult;
			}
			else if(argv[i][1]=='a' && argv[i][2]==0) {
				mode = C80;
				emulation = CTERM_EMULATION_ANSI_BBS;
				ansi=1;
			}
			else if(argv[i][1]=='A' && argv[i][2]==0) {
				emulation = CTERM_EMULATION_ATASCII;
				mode = ATARIST_40X25;
				ansi=0;
				strlcpy(font_name, "Atari", sizeof(font_name));
				scaling *= 2;
				rip = RIP_VERSION_NONE;
			}
			else if(argv[i][1]=='B' && argv[i][2]==0) {
				emulation = CTERM_EMULATION_BEEB;
				mode = PRESTEL_40X25;
				ansi=0;
				strlcpy(font_name, "Prestel", sizeof(font_name));
				rip = RIP_VERSION_NONE;
			}
			else if(argv[i][1]=='C' && argv[i][2]==0) {
				emulation = CTERM_EMULATION_PETASCII;
				mode = C64_40X25;
				ansi=0;
				strlcpy(font_name, "Commodore 64 (UPPER)", sizeof(font_name));
				scaling *= 2;
				rip = RIP_VERSION_NONE;
			}
			else if(argv[i][1]=='P' && argv[i][2]==0) {
				emulation = CTERM_EMULATION_PRESTEL;
				mode = PRESTEL_40X25;
				ansi=0;
				strlcpy(font_name, "Prestel", sizeof(font_name));
				rip = RIP_VERSION_NONE;
			}
			else if(argv[i][1]=='R' && argv[i][2]==0) {
				emulation = CTERM_EMULATION_ANSI_BBS;
				mode = EGA80X25;
				ansi=0;
				strlcpy(font_name, conio_fontdata[0].desc, sizeof(font_name));
				rip = RIP_VERSION_1;
			}
			else if(argv[i][1]=='S' && argv[i][2]==0) {
				emulation = CTERM_EMULATION_ATARIST_VT52;
				mode = ATARIST_80X25;
				ansi=0;
				strlcpy(font_name, "Atari ST", sizeof(font_name));
				rip = RIP_VERSION_NONE;
			}
			else
				goto usage;
		}
		else {
			if(infile==NULL)
				infile=argv[i];
			else
				goto usage;
		}
	}

	if(infile) {
		if((f=fopen(infile,"rb"))==NULL) {
			printf("Cannot read %s\n",infile);
			return(-1);
		}
		if (sauce_fread_record(f, &sauce)) {
			int cols = 80;
			int rows = 25;
			cvmode = find_vmode(CIOLIB_MODE_CUSTOM);
			saucy = true;
			stop = flength(infile) - 128 - 1;
			if (sauce.comments)
				stop -= (sauce.comments * 64 + 5);

			memcpy(stitle, sauce.title, sizeof(sauce.title));
			stitle[sizeof(stitle) - 1] = 0;
			truncsp(stitle);
			uint16_t type = (((uint16_t)sauce.datatype) << 8) | sauce.filetype;
			switch (type) {
				case 0x0102:	// ANSiMation
					speed = 28800;
					// Fallthrough
				case 0x0100:	// ASCII
				case 0x0101:	// ANSi
					// These types have a font name...
					if (sauce.tinfos[0]) {
						for (size_t f = 0; f < sizeof(fnames) / sizeof(fnames[0]); f++) {
							size_t nlen = strlen(fnames[f]);
							if (fonts[f] >= 0)
								nlen++;
							if (strncmp(sauce.tinfos, fnames[f], nlen) == 0) {
								if (fonts[f] == -1) {
									char *p = &sauce.tinfos[nlen];
									if (strcmp(p, "437"))
										strcpy(font_name, conio_fontdata[0].desc);
									else if (strcmp(p, "850"))
										strcpy(font_name, conio_fontdata[18].desc);
									else if (strcmp(p, "865"))
										strcpy(font_name, conio_fontdata[28].desc);
									else if (strcmp(p, "866"))
										strcpy(font_name, conio_fontdata[25].desc);
									else if (strcmp(p, "1131"))
										strcpy(font_name, conio_fontdata[31].desc);
									else if (strcmp(p, "1251"))
										strcpy(font_name, conio_fontdata[20].desc);
									else if (strcmp(p, "MIK"))
										strcpy(font_name, conio_fontdata[20].desc);
								}
								else
									strcpy(font_name, conio_fontdata[fonts[f]].desc);
							}
						}
					}
					// Fallthrough
				case 0x0104:	// PCBoard
				case 0x0105:	// Avatar
				case 0x0108:	// TundraDraw
					cols = sauce.tinfo1;
					if (cols > 255)
						cols = 255;
					rows = sauce.tinfo2;
					if (rows > 60)
						rows = 60;
					vparams[cvmode].cols = cols;
					vparams[cvmode].rows = rows;
					mode = CIOLIB_MODE_CUSTOM;
					break;
				case 0x0103:	// RIP
					if (sauce.tinfo1 == 640) {
						if (sauce.tinfo2 == 350)
							mode = EGA80X25;
					}
					rip = RIP_VERSION_1;
					break;
				// TODO: 0x0500 (Binary screen image) and 0x0600 (XBin)
			}
			if (sauce.datatype == 0x05) {
				// Just puttext() the whole thing...
				bintext = true;
				cols = sauce.filetype * 2;
				// Work around issue in ds-mim02.bin and ds-mim03.bin by dea(dsoul) from mimic
				if (cols == 2)
					cols = 160;
				rows = stop / (cols * 2);
				if (rows > 60)
					rows = 60;
				vparams[cvmode].cols = cols;
				vparams[cvmode].rows = rows;
				mode = CIOLIB_MODE_CUSTOM;
			}
			if (sauce.tflags & sauce_ansiflag_nonblink) {
				vparams[cvmode].flags |= (CIOLIB_VIDEO_NOBLINK | CIOLIB_VIDEO_BGBRIGHT);
				setflags |= (CIOLIB_VIDEO_NOBLINK | CIOLIB_VIDEO_BGBRIGHT);
			}
			switch (sauce.tflags & sauce_ansiflag_spacing_mask) {
				case sauce_ansiflag_spacing_9pix:
					setflags |= (CIOLIB_VIDEO_EXPAND | CIOLIB_VIDEO_LINE_GRAPHICS_EXPAND);
					vparams[cvmode].flags |= (CIOLIB_VIDEO_EXPAND | CIOLIB_VIDEO_LINE_GRAPHICS_EXPAND);
					vparams[cvmode].charwidth = 9;
					break;
				case sauce_ansiflag_spacing_legacy:
				case sauce_ansiflag_spacing_8pix:
				default:
					vparams[cvmode].charwidth = 8;
					break;
			}
			switch (sauce.tflags & sauce_ansiflag_ratio_mask) {
				case sauce_ansiflag_ratio_square:
					vparams[cvmode].aspect_width = vparams[cvmode].cols * vparams[cvmode].charwidth;
					vparams[cvmode].aspect_height = vparams[cvmode].rows * vparams[cvmode].charheight;
					break;
				case sauce_ansiflag_ratio_legacy:
				case sauce_ansiflag_ratio_rect:
				default:
					break;
			}
		}
		if (stitle[0])
			snprintf(title, sizeof(title), "SyncView: %s (%s)", getfname(argv[1]), stitle);
		else
			snprintf(title, sizeof(title), "SyncView: %s",getfname(argv[1]));
	}
	else {
		f=stdin;
		strcpy(title,"SyncView: [stdin]");
	}

	ciolib_initial_scaling = scaling;
	ciolib_initial_program_name = "SyncView";
	ciolib_initial_program_class = "SyncView";
	ciolib_initial_mode = mode;
	if(ansi) {
		initciolib(CIOLIB_MODE_ANSI);
		puts("START OF ANSI...");
	}

	textmode(mode);
	flags = getvideoflags();
	flags |= setflags;
	setvideoflags(flags);
	setFontByName(font_name);
	gettextinfo(&ti);
	term.width = vparams[cvmode].cols;
	term.height = ti.screenheight;
	if((scrollbuf=malloc(SCROLL_LINES*vparams[cvmode].cols*sizeof(*scrollbuf)))==NULL) {
		cprintf("Cannot allocate memory\n\n\rPress any key to exit.");
		getch();
		return(-1);
	}

	cterm=cterm_init(ti.screenheight, vparams[cvmode].cols, 1, 1, SCROLL_LINES, vparams[cvmode].cols, scrollbuf, emulation);
	if(!cterm) {
		fputs("ERROR Initializing CTerm!\n", stderr);
		return 1;
	}
	if (rip != RIP_VERSION_NONE)
		init_rip_ver(rip);
	settitle(title);
	if (bintext) {
		size_t row = 0;
		size_t rows = stop / (vparams[cvmode].cols * 2);

		fclose(f);
		struct xpmapping *map = xpmap(infile, XPMAP_READ);
		if (map) {
			for (row = 0; row < rows; row += ti.screenheight) {
				size_t sr = rows - row;
				if (sr > ti.screenheight)
					sr = ti.screenheight;
				if (row) {
					for (size_t i = 0; i < sr; i++)
						cterm_scrollup(cterm);
				}
				puttext(1, 1 + (ti.screenheight - sr), vparams[cvmode].cols, ti.screenheight, &map->addr[row * vparams[cvmode].cols * 2]);
			}
			xpunmap(map);
		}
	}
	else {
		while((len=fread(buf, 1, BUF_SIZE, f))!=0) {
			if (saucy && pos + len > stop) {
				len = stop - pos;
			}
			pos += len;
			if(expand)
				lfexpand(buf, &len);
			if (rip)
				len = parse_rip((uint8_t *)buf, len, sizeof(buf));
			cterm_write(cterm, buf, len, NULL, 0, &speed);
			if (pos >= stop)
				break;
		}
	}
	if(ansi) {
		puts("");
		puts("END OF ANSI");
		if(cterm->backpos>cterm->backlines) {
			memmove(cterm->scrollback,cterm->scrollback+cterm->width,cterm->width*sizeof(*cterm->scrollback)*(cterm->backlines-1));
			cterm->backpos=cterm->backlines;
		}
		vmem_gettext(1,1,vparams[cvmode].cols,ti.screenheight,cterm->scrollback+(cterm->backpos)*cterm->width);
		cterm->backpos += ti.screenheight;
		puttext_can_move=1;
		puts("START OF SCREEN DUMP...");
		for (i = 0; i < cterm->backpos; i += ti.screenheight) {
			clrscr();
			vmem_puttext(1,1,vparams[cvmode].cols,cterm->backpos - i > ti.screenheight ? ti.screenheight : cterm->backpos - i,cterm->scrollback+(vparams[cvmode].cols*i));
		}
	}
	else
		viewscroll(rip);
	return(0);

usage:
	cprintf("Usage: %s [-s <scaling>] [-l] [-a | -A | -B | -C | -P | -V] [filename]\r\n\r\n"
			"Displays the ANSI file filename expanding \\n to \\r\\n if -l is specified.\r\n"
			"If no filename is specified, reads input from stdin\r\n"
			"Will be scaled by <scaling> (can be fractional)\r\n"
			"If -a is specified, outputs ANSI to stdout\r\n"
			"If -A is specified, outputs in Atari mode\r\n"
			"If -B is specified, outputs in BBC Micro mode\r\n"
			"If -C is specified, outputs in C64 mode\r\n"
			"If -R is specified, outputs in RIP mode\r\n"
			"If -S is specified, outputs in AtariST VT-52 mode\r\n"
			"\r\n"
			"Press any key to exit.", argv[0]);
	return(-1);
}
