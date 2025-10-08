#include <stdio.h>
#include <string.h>

#include "dirwrap.h"
#include "genwrap.h"
#include "xpendian.h"
#include "xpmap.h"

#include "bitmap_con.h"
#include "ciolib.h"
#include "cterm.h"
#include "ripper.h"
#include "sauce.h"
#include "term.h"
#include "vidmodes.h"

#define SCROLL_LINES	100000
#define BUF_SIZE		1024

extern void *hack_font1;
extern void *hack_font2;

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

#if defined(__GNUC__)
        #define PACKED_STRUCT __attribute__((packed))
#else   /* non-GCC compiler */
        #pragma pack(push)
        #pragma pack(1)
        #define PACKED_STRUCT
#endif

struct xbin_header {
	char     id[4];
	uint8_t  eof;
	uint16_t width;
	uint16_t height;
	uint8_t  fontsize;
	uint8_t  flags;
} PACKED_STRUCT;

#if !defined(__GNUC__)
        #pragma pack(pop)       /* original packing */
#endif
#undef PACKED_STRUCT

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

uint16_t expand_channel(uint16_t val)
{
	uint16_t ret = val << 10;
	ret |= val << 4;
	ret |= val >> 2;
	return ret;
}

uint8_t *
xbin_uncompress(uint8_t *data, uint16_t width, uint16_t height, size_t sz)
{
	uint8_t *ret = malloc((size_t)width * height * 2);
	if (ret == NULL)
		return ret;
	size_t out = 0;
	for (size_t off = 0; off < sz; off++) {
		int type = data[off] & 0xc0;
		int counter = (data[off] & 0x3f) + 1;
		uint8_t ch, attr;
		switch (type) {
			case 0:    // No compression
				for (size_t i = 0; i < counter; i++) {
					ret[out++] = data[++off];
					ret[out++] = data[++off];
				}
				break;
			case 0x40: // Character compression
				ch = data[++off];
				for (size_t i = 0; i < counter; i++) {
					ret[out++] = ch;
					ret[out++] = data[++off];
				}
				break;
			case 0x80: // Attribute compression
				attr = data[++off];
				for (size_t i = 0; i < counter; i++) {
					ret[out++] = data[++off];
					ret[out++] = attr;
				}
				break;
			default:   // Character/Attribute compression
				ch = data[++off];
				attr = data[++off];
				for (size_t i = 0; i < counter; i++) {
					ret[out++] = ch;
					ret[out++] = attr;
				}
				break;
		}
	}
	return ret;
}

bool
setup_xbin(uint8_t *data, int *mode, int *setflags, int cmode, bool *xbin)
{
	struct xbin_header *hdr = (void *)data;
	if (memcmp(hdr->id, "XBIN", 4) == 0) {
		*mode = CIOLIB_MODE_CUSTOM;
		uint16_t cols = LE_INT16(hdr->width);
		if (cols == 0)
			cols = 80;
		vparams[cvmode].cols = LE_INT16(hdr->width);
		int rows = LE_INT16(hdr->height);
		if (rows > 60)
			rows = 60;
		vparams[cvmode].rows = rows;
		vparams[cvmode].charwidth = 8;
		if (hdr->fontsize > 0 && hdr->fontsize < 33)
			vparams[cvmode].charheight = hdr->fontsize;
		if (hdr->flags & (1 << 3)) {
			*setflags |= (CIOLIB_VIDEO_NOBLINK | CIOLIB_VIDEO_BGBRIGHT);
			vparams[cvmode].flags |= (CIOLIB_VIDEO_NOBLINK | CIOLIB_VIDEO_BGBRIGHT);
		}
		else {
			*setflags &= ~(CIOLIB_VIDEO_NOBLINK | CIOLIB_VIDEO_BGBRIGHT);
			vparams[cvmode].flags &= ~(CIOLIB_VIDEO_NOBLINK | CIOLIB_VIDEO_BGBRIGHT);
		}
		if (hdr->flags & (1 << 4)) {
			*setflags |= (CIOLIB_VIDEO_NOBRIGHT | CIOLIB_VIDEO_ALTCHARS);
			vparams[cvmode].flags |= (CIOLIB_VIDEO_NOBRIGHT | CIOLIB_VIDEO_ALTCHARS);
		}
		*xbin = true;

		bool palette = (hdr->flags & (1 << 0));
		bool font = (hdr->flags & (1 << 1));
		bool twocharsets = (hdr->flags & (1 << 4));
		size_t offset = sizeof(struct xbin_header);
		if (palette) {
			offset += 48;
		}
		if (font) {
			vstat.forced_font = &data[offset];
			hack_font1 = &data[offset];
			offset += hdr->fontsize * 256;
			if (twocharsets) {
				vstat.forced_font2 = &data[offset];
				hack_font2 = &data[offset];
				offset += hdr->fontsize * 256;
			}
		}
		return true;
	}
	return false;
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
	bool            xbin = false;
	struct xpmapping *map = NULL;

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
			else if(argv[i][1]=='b' && argv[i][2]==0) {
				emulation = CTERM_EMULATION_ANSI_BBS;
				mode = C80;
				ansi=0;
				strlcpy(font_name, conio_fontdata[0].desc, sizeof(font_name));
				rip = RIP_VERSION_NONE;
				bintext = true;
				xbin = false;
			}
			else if(argv[i][1]=='x' && argv[i][2]==0) {
				emulation = CTERM_EMULATION_ANSI_BBS;
				mode = C80;
				ansi=0;
				strlcpy(font_name, conio_fontdata[0].desc, sizeof(font_name));
				rip = RIP_VERSION_NONE;
				bintext = false;
				xbin = true;
			}
			else if(argv[i][1]=='A' && argv[i][2]==0) {
				emulation = CTERM_EMULATION_ATASCII;
				mode = ATARIST_40X25;
				ansi=0;
				strlcpy(font_name, "Atari", sizeof(font_name));
				scaling *= 2;
				rip = RIP_VERSION_NONE;
				bintext = false;
				xbin = false;
			}
			else if(argv[i][1]=='B' && argv[i][2]==0) {
				emulation = CTERM_EMULATION_BEEB;
				mode = PRESTEL_40X25;
				ansi=0;
				strlcpy(font_name, "Prestel", sizeof(font_name));
				rip = RIP_VERSION_NONE;
				bintext = false;
				xbin = false;
			}
			else if(argv[i][1]=='C' && argv[i][2]==0) {
				emulation = CTERM_EMULATION_PETASCII;
				mode = C64_40X25;
				ansi=0;
				strlcpy(font_name, "Commodore 64 (UPPER)", sizeof(font_name));
				scaling *= 2;
				rip = RIP_VERSION_NONE;
				bintext = false;
				xbin = false;
			}
			else if(argv[i][1]=='P' && argv[i][2]==0) {
				emulation = CTERM_EMULATION_PRESTEL;
				mode = PRESTEL_40X25;
				ansi=0;
				strlcpy(font_name, "Prestel", sizeof(font_name));
				rip = RIP_VERSION_NONE;
				bintext = false;
				xbin = false;
			}
			else if(argv[i][1]=='R' && argv[i][2]==0) {
				emulation = CTERM_EMULATION_ANSI_BBS;
				mode = EGA80X25;
				ansi=0;
				strlcpy(font_name, conio_fontdata[0].desc, sizeof(font_name));
				rip = RIP_VERSION_1;
				bintext = false;
				xbin = false;
			}
			else if(argv[i][1]=='S' && argv[i][2]==0) {
				emulation = CTERM_EMULATION_ATARIST_VT52;
				mode = ATARIST_80X25;
				ansi=0;
				strlcpy(font_name, "Atari ST", sizeof(font_name));
				rip = RIP_VERSION_NONE;
				bintext = false;
				xbin = false;
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
					if (cols == 0)
						cols = 80;
					rows = sauce.tinfo2;
					if (rows == 0)
						rows = 25;
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
			}
			if (sauce.datatype == 0x05) {
				// Just puttext() the whole thing...
				bintext = true;
				cols = sauce.filetype * 2;
				if (cols == 0)
					cols = 80;
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
			// We parse XBin after because it can override some stuff...
			if (type == 0x0600) {
				fclose(f);
				map = xpmap(infile, XPMAP_READ);
				if (map) {
					if (setup_xbin(map->addr, &mode, &setflags, cvmode, &xbin))
						xbin = true;
					else {
						fprintf(stderr, "Failed to setup XBin\n");
						xpunmap(map);
					}
				}
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
		else {
			if (xbin) {
				fclose(f);
				map = xpmap(infile, XPMAP_READ);
				if (map) {
					if (setup_xbin(map->addr, &mode, &setflags, cvmode, &xbin))
						xbin = true;
					else {
						fprintf(stderr, "Failed to setup XBin\n");
						xpunmap(map);
						xbin = false;
					}
				}
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
		map = xpmap(infile, XPMAP_READ);
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
	else if (xbin) {
		if (map) {
			struct xbin_header *hdr = map->addr;
			if (memcmp(hdr->id, "XBIN", 4))
				fprintf(stderr, "Invalid XBIN header!\n");
			else {
				bool palette = (hdr->flags & (1 << 0));
				bool font = (hdr->flags & (1 << 1));
				bool compressed = (hdr->flags & (1 << 2));
				bool twocharsets = (hdr->flags & (1 << 4));
				size_t offset = sizeof(struct xbin_header);
				if (palette) {
					uint8_t *p = &map->addr[offset];
					for (int col = 0; col < 16; col++) {
						setpalette(palettes[2][col], expand_channel(p[0]), expand_channel(p[1]), expand_channel(p[2]));
						p += 3;
					}
					offset += 48;
				}
				if (font) {
					vstat.forced_font = &map->addr[offset];
					hack_font1 = &map->addr[offset];
					offset += hdr->fontsize * 256;
					if (twocharsets) {
						vstat.forced_font2 = &map->addr[offset];
						hack_font2 = &map->addr[offset];
						offset += hdr->fontsize * 256;
					}
				}
				uint8_t *imgdata = &map->addr[offset];
				if (compressed) {
					imgdata = xbin_uncompress(imgdata, LE_INT16(hdr->width), LE_INT16(hdr->height), stop - offset);
					if (imgdata == NULL) {
						fprintf(stderr, "Unable to decompress XBin data\n");
					}
				}
				size_t rows = LE_INT16(hdr->height);
				size_t row;
				for (row = 0; row < rows; row += ti.screenheight) {
					size_t sr = rows - row;
					if (sr > ti.screenheight)
						sr = ti.screenheight;
					if (row) {
						for (size_t i = 0; i < sr; i++)
							cterm_scrollup(cterm);
					}
					puttext(1, 1 + (ti.screenheight - sr), vparams[cvmode].cols, ti.screenheight, &imgdata[row * vparams[cvmode].cols * 2]);
				}
			}
			// We can't do this or we lose the fonts. :D
			//xpunmap(map);
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
			"If -b is specified, outputs BinaryText\r\n"
			"If -x is specified, outputs XBin\r\n"
			"If -A is specified, outputs in Atari mode\r\n"
			"If -B is specified, outputs in BBC Micro mode\r\n"
			"If -C is specified, outputs in C64 mode\r\n"
			"If -R is specified, outputs in RIP mode\r\n"
			"If -S is specified, outputs in AtariST VT-52 mode\r\n"
			"\r\n"
			"Press any key to exit.", argv[0]);
	return(-1);
}
