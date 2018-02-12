#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "saucedefs.h"
#include "dirwrap.h"

char *types[] = {
	"None",
	"Character",
	"Bitmap",
	"Vector",
	"Audio",
	"BinaryText",
	"XBin",
	"Archive",
	"Executable"
};

char *descriptions[] = {
	"Undefined filetype.",
	"A character based file.",
	"Bitmap graphic and animation files.",
	"A vector graphic file.",
	"An audio file.",
	"This is a raw memory copy of a text mode screen. Also known as a .BIN file.",
	"An XBin or eXtended BIN file.",
	"An archive file.",
	"A executable file."
};

struct filetype {
	uint8_t dtype;
	uint8_t ftype;
	char *name;
	char *desc;
	char *ti1n;
	char *ti2n;
	char *ti3n;
	bool has_ansiflags;
	bool has_fontname;
	bool filetype_is_width;
};

struct filetype ftypes[] = {
	{0,0,"-","Undefined filetype.",NULL,NULL,NULL,false,false,false},
	{1,0,"ASCII","Plain ASCII text file with no formatting codes or color codes.","Character width","Number of lines",NULL,true,true,false},
	{1,1,"ANSi","A file with ANSi coloring codes and cursor positioning.","Character width","Number of lines",NULL,true,true,false},
	{1,2,"ANSiMation","Like an ANSi file, but it relies on a fixed screen size.","Character width","Character screen height",NULL,true,true,false},
	{1,3,"RIP script","Remote Imaging Protocol graphics.","Pixel width (640) ","Pixel height (350) ","Number of colors (16) ",false,false,false},
	{1,4,"PCBoard","A file with PCBoard color codes and macros, and ANSi codes.","Character width","Number of lines",NULL,false,false,false},
	{1,5,"Avatar","A file with Avatar color codes, and ANSi codes.","Character width","Number of lines",NULL,false,false,false},
	{1,6,"HTML","HyperText Markup Language",NULL,NULL,NULL,false,false,false},
	{1,7,"Source","Source code for some programming language.\nThe file extension should determine the programming language.",NULL,NULL,NULL,false,false,false},
	{1,8,"TundraDraw","A TundraDraw file.\nLike ANSI, but with a custom palette.","Character width","Number of lines",NULL,false,false,false},
	{2,0,"GIF","CompuServe Graphics Interchange Format","Pixel width ","Pixel height ","Pixel depth",false,false,false},
	{2,1,"PCX","ZSoft Paintbrush PCX","Pixel width ","Pixel height ","Pixel depth",false,false,false},
	{2,2,"LBM/IFF","DeluxePaint LBM/IFF","Pixel width ","Pixel height ","Pixel depth",false,false,false},
	{2,3,"TGA","Targa Truecolor","Pixel width ","Pixel height ","Pixel depth",false,false,false},
	{2,4,"FLI","Autodesk FLI animation","Pixel width ","Pixel height ","Pixel depth",false,false,false},
	{2,5,"FLC","Autodesk FLC animation","Pixel width ","Pixel height ","Pixel depth",false,false,false},
	{2,6,"BMP","Windows or OS/2 Bitmap","Pixel width ","Pixel height ","Pixel depth",false,false,false},
	{2,7,"GL","Grasp GL Animation","Pixel width ","Pixel height ","Pixel depth",false,false,false},
	{2,8,"DL","DL Animation","Pixel width ","Pixel height ","Pixel depth",false,false,false},
	{2,9,"WPG","Wordperfect Bitmap","Pixel width ","Pixel height ","Pixel depth",false,false,false},
	{2,10,"PNG","Portable Network Graphics","Pixel width ","Pixel height ","Pixel depth",false,false,false},
	{2,11,"JPG/JPeg","JPeg image (any subformat)","Pixel width ","Pixel height ","Pixel depth",false,false,false},
	{2,12,"MPG","MPeg video (any subformat)","Pixel width ","Pixel height ","Pixel depth",false,false,false},
	{2,13,"AVI","Audio Video Interleave (any subformat)","Pixel width ","Pixel height ","Pixel depth",false,false,false},
	{3,0,"DXF","CAD Drawing eXchange Format",NULL,NULL,NULL,false,false,false},
	{3,1,"DWG","AutoCAD Drawing File",NULL,NULL,NULL,false,false,false},
	{3,2,"WPG","WordPerfect or DrawPerfect vector graphics",NULL,NULL,NULL,false,false,false},
	{3,3,"3DS","3D Studio",NULL,NULL,NULL,false,false,false},
	{4,0,"MOD","4, 6 or 8 channel MOD (NoiseTracker)",NULL,NULL,NULL,false,false,false},
	{4,1,"669","Renaissance 8 channel 669",NULL,NULL,NULL,false,false,false},
	{4,2,"STM","Future Crew 4 channel ScreamTracker",NULL,NULL,NULL,false,false,false},
	{4,3,"S3M","Future Crew variable channel ScreamTracker 3",NULL,NULL,NULL,false,false,false},
	{4,4,"MTM","Renaissance variable channel MultiTracker",NULL,NULL,NULL,false,false,false},
	{4,5,"FAR","Farandole composer",NULL,NULL,NULL,false,false,false},
	{4,6,"ULT","UltraTracker",NULL,NULL,NULL,false,false,false},
	{4,7,"AMF","DMP/DSMI Advanced Module Format",NULL,NULL,NULL,false,false,false},
	{4,8,"DMF","Delusion Digital Music Format (XTracker)",NULL,NULL,NULL,false,false,false},
	{4,9,"OKT","Oktalyser",NULL,NULL,NULL,false,false,false},
	{4,10,"ROL","AdLib ROL file (FM audio)",NULL,NULL,NULL,false,false,false},
	{4,11,"CMF","Creative Music File (FM Audio)",NULL,NULL,NULL,false,false,false},
	{4,12,"MID","MIDI (Musical Instrument Digital Interface)",NULL,NULL,NULL,false,false,false},
	{4,13,"SADT","SAdT composer (FM Audio)",NULL,NULL,NULL,false,false,false},
	{4,14,"VOC","Creative Voice File",NULL,NULL,NULL,false,false,false},
	{4,15,"WAV","Waveform Audio File Format",NULL,NULL,NULL,false,false,false},
	{4,16,"SMP8","Raw, single channel 8bit sample","Sample rate",NULL,NULL,false,false,false},
	{4,17,"SMP8S","Raw, stereo 8 bit sample","Sample rate",NULL,NULL,false,false,false},
	{4,18,"SMP16","Raw, single-channel 16 bit sample","Sample rate",NULL,NULL,false,false,false},
	{4,19,"SMP16S","Raw, stereo 16 bit sample","Sample rate",NULL,NULL,false,false,false},
	{4,20,"PATCH8","8 Bit patch file",NULL,NULL,NULL,false,false,false},
	{4,21,"PATCH16","16 bit patch file",NULL,NULL,NULL,false,false,false},
	{4,22,"XM","FastTracker ][ module",NULL,NULL,NULL,false,false,false},
	{4,23,"HSC","HSC Tracker (FM Audio)",NULL,NULL,NULL,false,false,false},
	{4,24,"IT","Impulse Tracker",NULL,NULL,NULL,false,false,false},
	{5,0 ,"-","Binary screen image",NULL,NULL,NULL,true,true,true},
	{6,0,"-","eXtended Bin","Character width","Number of lines",NULL,false,false,false},
	{7,0,"ZIP","PKWare Zip.",NULL,NULL,NULL,false,false,false},
	{7,1,"ARJ","Archive Robert K. Jung",NULL,NULL,NULL,false,false,false},
	{7,2,"LZH","Haruyasu Yoshizaki (Yoshi)",NULL,NULL,NULL,false,false,false},
	{7,3,"ARC","S.E.A.",NULL,NULL,NULL,false,false,false},
	{7,4,"TAR","Unix TAR",NULL,NULL,NULL,false,false,false},
	{7,5,"ZOO","ZOO",NULL,NULL,NULL,false,false,false},
	{7,6,"RAR","RAR",NULL,NULL,NULL,false,false,false},
	{7,7,"UC2","UC2",NULL,NULL,NULL,false,false,false},
	{7,8,"PAK","PAK",NULL,NULL,NULL,false,false,false},
	{7,9,"SQZ","SQZ",NULL,NULL,NULL,false,false,false},
	{8,0,"-","Any executable file. .exe, .dll, .bat, ...\nExecutable scripts such as .vbs should be tagged as Source.",NULL,NULL,NULL,false,false,false},
};

struct fontinfo {
	char *name;
	char *size;
	char *resolution;
	char *dar;
	char *par;
	char *stretch;
	char *desc;
	bool has_8px;
};

struct fontinfo finfo[] = {
	{"IBM VGA","9x16","720x400","4:3","20:27 (1:1.35)","35%","Standard hardware font on VGA cards for 80x25 text mode (code page 437)",true},
	{" ","8x16","640x400","4:3","6:5 (1:1.2)","20%","Modified stats when using an 8 pixel wide version of \"IBM VGA\" or code page variant.",false},
	{"IBM VGA50","9x8","720x400","4:3","20:27 (1:1.35)","35%","Standard hardware font on VGA cards for condensed 80x50 text mode (code page 437)",true},
	{" ","8x8","640x400","4:3","5:6 (1:1.2)","20%","Modified stats when using an 8 pixel wide version of \"IBM VGA50\" or code page variant.",false},
	{"IBM VGA25G","8x19","640x480","4:3","1:1","0%","Custom font for emulating 80x25 in VGA graphics mode 12 (640x480 16 color) (code page 437).",false},
	{"IBM EGA","8x14","640x350","4:3","35:48 (1:1.3714)","37.14%","Standard hardware font on EGA cards for 80x25 text mode (code page 437)",false},
	{"IBM EGA43","8x8","640x350","4:3","35:48 (1:1.3714)","37.14%","Standard hardware font on EGA cards for condensed 80x43 text mode (code page 437)",false},
	{"IBM VGA ###","9x16","720x400","4:3","20:27 (1:1.35)","35%","Software installed code page font for VGA 80x25 text mode",false},
	{"IBM VGA50 ###","9x8","720x400","4:3","20:27 (1:1.35)","35%","Software installed code page font for VGA condensed 80x50 text mode",false},
	{"IBM VGA25G ###","8x19","640x480","4:3","1:1","0%","Custom font for emulating 80x25 in VGA graphics mode 12 (640x480 16 color).",false},
	{"IBM EGA ###","8x14","640x350","4:3","35:48 (1:1.3714)","37.14%","Software installed code page font for EGA 80x25 text mode",false},
	{"IBM EGA43 ###","8x8","640x350","4:3","35:48 (1:1.3714)","37.14%","Software installed code page font for EGA condensed 80x43 text mode",false},
	{"Amiga Topaz 1","8x8","640x200","4:3","5:12 (1:2.4)","140%","Original Amiga Topaz Kickstart 1.x font. (A500, A1000, A2000)",false},
	{"Amiga Topaz 1+","8x8","640x200","4:3","5:12 (1:2.4)","140%","Modified Amiga Topaz Kickstart 1.x font. (A500, A1000, A2000)",false},
	{"Amiga Topaz 2","8x8","640x200","4:3","5:12 (1:2.4)","140%","Original Amiga Topaz Kickstart 2.x font (A600, A1200, A4000)",false},
	{"Amiga Topaz 2+","8x8","640x200","4:3","5:12 (1:2.4)","140%","Modified Amiga Topaz Kickstart 2.x font (A600, A1200, A4000)",false},
	{"Amiga P0T-NOoDLE","8x8","640x200","4:3","5:12 (1:2.4)","140%","Original P0T-NOoDLE font.",false},
	{"Amiga MicroKnight","8x8","640x200","4:3","5:12 (1:2.4)","140%","Original MicroKnight font.",false},
	{"Amiga MicroKnight+","8x8","640x200","4:3","5:12 (1:2.4)","140%","Modified MicroKnight font.",false},
	{"Amiga mOsOul","8x8","640x200","4:3","5:12 (1:2.4)","140%","Original mOsOul font.",false},
	{"C64 PETSCII unshifted","8x8","320x200","4:3","5:6 (1:1.2)","20%","Original Commodore PETSCII font (PET, VIC-20, C64, CBM-II, Plus/4, C16, C116 and C128) in the unshifted mode. Unshifted mode (graphics) only has uppercase letters and additional graphic characters. This is the normal boot font.",false},
	{"C64 PETSCII shifted","8x8","320x200","4:3","5:6 (1:1.2)","20%","Original PETSCII font in shifted mode. Shifted mode (text) has both uppercase and lowercase letters. This mode is actuated by pressing Shift+Commodore key.",false},
	{"Atari ATASCII","8x8","320x192","4:3","4:5 (1:1.25)","25%","Original ATASCII font (Atari 400, 800, XL, XE)",false},
};

struct codepages {
	char *code;
	char *name;
};

struct codepages cpages[] = {
	{"437","The character set of the original IBM PC. Also known as 'MS-DOS Latin US'."},
	{"720","Arabic. Also known as 'Windows-1256'."},
	{"737","Greek. Also known as 'MS-DOS Greek'."},
	{"775","Baltic Rim (Estonian, Lithuanian and Latvian). Also known as 'MS-DOS Baltic Rim'."},
	{"819","Latin-1 Supplemental. Also known as 'Windows-28591' and 'ISO/IEC 8859-1'."},
	{"850","Western Europe. Also known as 'MS-DOS Latin 1'."},
	{"852","Central Europe (Bosnian, Croatian, Czech, Hungarian, Polish, Romanian, Serbian and Slovak). Also known as 'MS-DOS Latin 2'."},
	{"855","Cyrillic (Serbian, Macedonian Bulgarian, Russian). Also known as 'MS-DOS Cyrillic'."},
	{"857","Turkish. Also known as 'MS-DOS Turkish'."},
	{"858","Western Europe."},
	{"860","Portuguese. Also known as 'MS-DOS Portuguese'."},
	{"861","Icelandic. Also known as 'MS-DOS Icelandic'."},
	{"862","Hebrew. Also known as 'MS-DOS Hebrew'."},
	{"863","French Canada. Also known as 'MS-DOS French Canada'."},
	{"864","Arabic."},
	{"865","Nordic."},
	{"866","Cyrillic."},
	{"869","Greek 2. Also known as 'MS-DOS Greek 2'."},
	{"872","Cyrillic."},
	{"KAM","'Kamenicky' encoding. Also known as 'KEYBCS2'."},
	{"MAZ","'Mazovia' encoding."},
	{"MIK","Cyrillic."},
};

int main(int argc, char **argv)
{
	FILE *f;
	int i, j, k, l;
	struct sauce sauce;
	char *buf;
	char *cpp;
	size_t namelen;

	for (i=1; i<argc; i++) {
		if (!fexist(argv[i])) {
			fprintf(stderr, "No such file %s.\n", argv[i]);
			continue;
		}

		if (flength(argv[i]) < 128) {
			fprintf(stderr, "No SAUCE record in %s.\n", argv[i]);
			continue;
		}

		f = fopen(argv[i], "rb");
		if (f == NULL) {
			fprintf(stderr, "Unable to open %s.\n", argv[i]);
			continue;
		}

		fseek(f, -128, SEEK_END);
		if (fread(&sauce, sizeof(sauce), 1, f) < 1) {
			fclose(f);
			fprintf(stderr, "Unable to read SAUCE record from %s.\n", argv[i]);
			continue;
		}

		if (strncmp(sauce.id, "SAUCE", 5)) {
			fprintf(stderr, "No SAUCE record in %s.\n", argv[i]);
			fclose(f);
			continue;
		}
		if (strncmp(sauce.ver, "00", 2)) {
			fprintf(stderr, "Unsupported SAUCE version %.2s in %s.\n", sauce.ver, argv[i]);
			fclose(f);
			continue;
		}
		fprintf(stdout, "\n--- %s ---\n", argv[i]);
		if (sauce.title[0])
			fprintf(stdout, "Title: %.35s\n", sauce.title);
		if (sauce.author[0])
			fprintf(stdout, "Author: %.20s\n", sauce.author);
		if (sauce.group[0])
			fprintf(stdout, "Group: %.20s\n", sauce.group);
		if (sauce.date[0] && sauce.date[1] && sauce.date[2] && sauce.date[3] && sauce.date[4] && sauce.date[5] && sauce.date[6] && sauce.date[7])
			fprintf(stdout, "Date: %.4s-%.2s-%.2s\n", sauce.date, sauce.date+4, sauce.date+6);
		if (sauce.datatype < sizeof(types)/sizeof(types[0]))
			fprintf(stdout, "Type: %s (%hhu)\n", types[sauce.datatype], sauce.datatype);
		else
			fprintf(stdout, "Type: Invalid Type (%hhu)\n", sauce.datatype);
		for (j=0; j<sizeof(ftypes)/sizeof(ftypes[0]); j++) {
			if (ftypes[j].dtype == sauce.datatype && (ftypes[j].ftype == sauce.filetype || ftypes[j].filetype_is_width)) {
				fprintf(stdout, "FileType: %s (%hhu)\n", ftypes[j].name, sauce.filetype);
				if (sauce.tinfo1 && ftypes[j].ti1n)
					fprintf(stdout, "%s: %" PRIu16 "\n", ftypes[j].ti1n, sauce.tinfo1);
				if (sauce.tinfo2 && ftypes[j].ti2n)
					fprintf(stdout, "%s: %" PRIu16 "\n", ftypes[j].ti2n, sauce.tinfo2);
				if (sauce.tinfo3 && ftypes[j].ti3n)
					fprintf(stdout, "%s: %" PRIu16 "\n", ftypes[j].ti3n, sauce.tinfo3);
				if (ftypes[j].filetype_is_width && sauce.filetype)
					fprintf(stdout, "Width: %u\n", sauce.filetype*2);
				if (ftypes[j].has_ansiflags) {
					puts("ANSiFlags:");
					if (sauce.tflags & sauce_ansiflag_nonblink)
						puts("\tNon-blink mode (iCE Color)");
					if (sauce.tflags & sauce_ansiflag_spacing_mask) {
						switch(sauce.tflags & sauce_ansiflag_spacing_mask) {
							case sauce_ansiflag_spacing_8pix:
								puts("\tSelect 8 pixel font.");
								break;
							case sauce_ansiflag_spacing_9pix:
								puts("\tSelect 9 pixel font.");
								break;
						}
					}
					if (sauce.tflags & sauce_ansiflag_ratio_mask) {
						switch(sauce.tflags & sauce_ansiflag_ratio_mask) {
							case sauce_ansiflag_ratio_rect:
								puts("\tImage was created for a legacy device without square pixels.");
								break;
							case sauce_ansiflag_ratio_square:
								puts("\tImage was created for a modern device with square pixels.");
								break;
						}
					}
				}
				if (ftypes[j].has_fontname) {
					if (sauce.tinfos[0]) {
						fprintf(stdout, "FontName: %.22s\n", sauce.tinfos);
						for (k = 0; k<sizeof(finfo)/sizeof(finfo[0]); k++) {
							namelen = 22;
							cpp = strstr(finfo[k].name, "###");
							if (cpp != NULL)
								namelen = (cpp-finfo[k].name);
							if (strncmp(sauce.tinfos, finfo[k].name, namelen) == 0) {
								if (finfo[k].has_8px && (sauce.tflags & sauce_ansiflag_spacing_8pix))
									k++;
								if (cpp) {
									for (l=0; l<sizeof(cpages)/sizeof(cpages[0]); l++) {
										if (strcmp(cpages[l].code, sauce.tinfos + (cpp-finfo[k].name)) == 0) {
											fprintf(stdout, "\tCode Page: %.*s - %s\n", (int)strlen(cpp), sauce.tinfos + (cpp-finfo[k].name), cpages[l].name);
											break;
										}
									}
								}
								if (l == sizeof(cpages)/sizeof(cpages[0]))
									fprintf(stdout, "\tCode Page: %.*s - Invalid/Unsupported\n", (int)strlen(cpp), sauce.tinfos + (cpp-finfo[k].name));
								fprintf(stdout,"\tFont Size: %s\n", finfo[k].size);
								fprintf(stdout,"\tResolution: %s\n", finfo[k].resolution);
								fprintf(stdout,"\tDisplay Aspect Ratio: %s\n", finfo[k].dar);
								fprintf(stdout,"\tPixel Aspect Ratio: %s\n", finfo[k].par);
								fprintf(stdout,"\tVertical Stretch: %s\n", finfo[k].stretch);
								fprintf(stdout,"\tDescription: %s\n", finfo[k].desc);
								break;
							}
						}
					}
				}
				break;
			}
		}
		if (sauce.comments) {
			if (fseek(f, -128-(sauce.comments*64)-5, SEEK_END)) {
				fputs("Unable to locate comment block\n", stderr);
			}
			else {
				buf = malloc(sauce.comments*64+5);
				if (buf) {
					if (fread(buf, sauce.comments*64+5, 1, f) != 1) {
						fputs("Error reading SAUCE comment block.\n", stderr);
					}
					else {
						if (strncmp(buf, "COMNT", 5)) {
							fputs("Invalid comment block.\n", stderr);
						}
						else {
							puts("Comments:");
							for (j=0; j<sauce.comments; j++) {
								fprintf(stdout, "\t%.64s\n", buf+5+(j*64));
							}
						}
					}
					free(buf);
				}
			}
		}
		fclose(f);
	}
}
