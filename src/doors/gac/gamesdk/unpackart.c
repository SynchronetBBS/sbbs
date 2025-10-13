#include <stdio.h>
#include <string.h>

#include "dirwrap.h"
#include "genwrap.h"

static char line[261];

static void
usage(const char *argv0)
{
	printf("Usage: %s in_ext.file\n\n"
	     "File in in.file must not exist, or an error occurs.\n\n"
	     "Unpacks files from in_ext.file into NAME.ext.\n", argv0);
	exit(1);
}

static void
HelpDecrypt(char *line)
{
	char *curp;
	char tmp;

	curp = &line[0];
	while (curp[0] != '\0')
	{

				if (curp[0] > 0 )
						curp[0] -= 128;
				else
						curp[0] += 128; //should be 127
		curp++;

	}

	/*
	 * The swab() that was used did not work properly when src and dst
	 * overlapped, only the first two pairs got swapped.
	 */
	if (line[0] && line[1]) {
		tmp = line[0];
		line[0] = line[1];
		line[1] = tmp;
	}
	if (line[2] && line[3]) {
		tmp = line[2];
		line[2] = line[3];
		line[3] = tmp;
	}
	//swab(line, line, sizeof(line));
	//strcpy(line, strrev(line));
	strrev(line);

	// if there are blanks on the end remove them...

	while (curp[strlen(curp)-1] == ' ')
	{
		curp[strlen(curp)-1] = '\0';
	}

	return;
}

int
main(int argc, char **argv)
{
	FILE *outfile = NULL;
	FILE *infile = NULL;
	int ret = 1;
	char *p;
	char *ext = NULL;
	char *dupfname = NULL;

	if (argc != 2)
		usage(argv[0]);
	infile = fopen(argv[1], "rb");
	if (infile == NULL) {
		fprintf(stderr, "Error %d opening %s\n", errno, argv[1]);
		goto exit;
	}
	dupfname = strdup(argv[1]);
	p = getfname(dupfname);
	if (p) {
		ext = getfext(p);
		if (ext) {
			*ext = 0;
			ext = strrchr(p, '_');
			if (ext) {
				*ext = '.';
			}
			else {
				FREE_AND_NULL(dupfname);
				ext = NULL;
			}
		}
		else {
			FREE_AND_NULL(dupfname);
			ext = NULL;
		}
	}
	else
		FREE_AND_NULL(dupfname);
	if (!ext) {
		fputs("Unable to get extension, using .out\n", stderr);
		ext = ".out";
	}

	while (fgets(line, sizeof(line), infile)) {
		truncnl(line);
		if (line[0] == '@' && line[1] == '#') {
			int skip = 0;

			if (outfile)
				fclose(outfile);
			HelpDecrypt(&line[2]);
			strlcat(line, ext, sizeof(line));
			for (p = line; *p; p++) {
				if (*p < 32 || *p > 126) {
					skip = 1;
					break;
				}
			}
			if (!skip) {
				if (fexist(line)) {
					fprintf(stderr, "%s already exists, skipping\n", line);
				}
				else {
					outfile = fopen(&line[2], "wb");
					if (outfile == NULL) {
						fprintf(stderr, "Error %d opening %s\n", errno, line);
					}
				}
			}
		}
		else {
			if (outfile) {
				HelpDecrypt(line);
				fputs(line, outfile);
				fputs("\r\n", outfile);
			}
		}
	}
	
	ret = 0;

exit:
	free(dupfname);
	if (infile)
		fclose(infile);
	if (outfile)
		fclose(outfile);
	return ret;
}
