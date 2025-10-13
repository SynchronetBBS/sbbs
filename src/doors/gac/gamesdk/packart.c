#include <stdio.h>

#include "dirwrap.h"
#include "genwrap.h"

static char line[261];

static void
usage(const char *argv0)
{
	printf("Usage: %s out.file file1.type file2.type...\n\n"
	     "out.file must not exist, or an error occurs.\n\n"
	     "at least line fileX.type file is required\n\n"
	     "Packs fileX.type into out.file.\n", argv0);
	exit(1);
}

static void HelpEncrypt(void)
{
	char *curp;
	char tmp;

	if (strlen(line) & 1)
		strlcat(line, " ", sizeof(line));
	strrev(line);
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

	curp = &line[0];
	for (curp = line; *curp; curp++) {
		if (*curp > 0 )
			*curp -= 128;
		else
			*curp += 128;
	}

	return;
}

int
main(int argc, char **argv)
{
	FILE *outfile = NULL;
	FILE *infile = NULL;
	int i;
	int ret = 1;
	char *p;
	char *fname;

	if (argc < 3)
		usage(argv[0]);
	if (fexist(argv[1])) {
		fprintf(stderr, "%s already exists\n", argv[1]);
		usage(argv[0]);
	}
	for (i = 2; i < argc; i++) {
		if (!fexist(argv[i])) {
			fprintf(stderr, "%s does not exist\n", argv[i]);
			usage(argv[0]);
		}
	}
	outfile = fopen(argv[1], "wb");
	if (outfile == NULL) {
		fprintf(stderr, "Error %d opening %s\n", errno, argv[1]);
		goto exit;
	}

	for (i = 2; i < argc; i++) {
		infile = fopen(argv[i], "rb");
		if (infile == NULL) {
			fprintf(stderr, "Error %d opening %s\n", errno, argv[i]);
			goto exit;
		}
		fputs("@#", outfile);
		fname = getfname(argv[i]);
		if (!fname) {
			fprintf(stderr, "Error getting file name from %s\n", argv[i]);
			goto exit;
		}
		strlcpy(line, fname, sizeof(line));
		p = strrchr(line, '.');
		if (p)
			*p = 0;
		strupr(line);
		HelpEncrypt();
		fputs(line, outfile);
		fputs("\r\n", outfile);
		while (fscanf(infile, "%[^\r\n]\r\n", line) == 1) {
			HelpEncrypt();
			fputs(line, outfile);
			fputs("\r\n", outfile);
		}
		fclose(infile);
		infile = NULL;
	}
	fputs("@#", outfile);
	memset(line, 0, sizeof(line));
	HelpEncrypt();
	fputs(line, outfile);
	fputs("\r\n", outfile);
	
	ret = 0;

exit:
	if (infile)
		fclose(infile);
	if (outfile)
		fclose(outfile);
	return ret;
}
