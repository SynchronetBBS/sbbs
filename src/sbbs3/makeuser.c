/* Program to add a user to a Synchronet user database */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "load_cfg.h"
#include "str_util.h"
#include "userdat.h"
#include "scfglib.h"
#include <stdarg.h>

scfg_t scfg;

/****************************************************************************/
/* This is needed by load_cfg.c												*/
/****************************************************************************/
int lprintf(int level, const char *fmat, ...)
{
	va_list argptr;
	char    sbuf[512];
	int     chcount;

	va_start(argptr, fmat);
	chcount = vsprintf(sbuf, fmat, argptr);
	va_end(argptr);
	truncsp(sbuf);
	printf("%s\n", sbuf);
	return chcount;
}

char *usage = "\nusage: makeuser [ctrl_dir] name [-param value] [...]\n"
              "\nparams:\n"
              "\t-P\tPassword\n"
              "\t-R\tReal name\n"
              "\t-H\tChat handle\n"
              "\t-G\tGender (M or F)\n"
              "\t-B\tBirth date (in MM/DD/YY or DD/MM/YY format)\n"
              "\t-T\tTelephone number\n"
              "\t-N\tNetmail (Internet e-mail) address\n"
              "\t-A\tStreet address\n"
              "\t-L\tLocation (city, state)\n"
              "\t-Z\tZip/Postal code\n"
              "\t-S\tSecurity level\n"
              "\t-F#\tFlag set #\n"
              "\t-FE\tExemption flags\n"
              "\t-FR\tRestriction flags\n"
              "\t-E\tExpiration days\n"
              "\t-C\tComment\n"
              "\nNOTE: multi-word user name and param values must be enclosed in \"quotes\"\n"
;

/*********************/
/* Entry point (duh) */
/*********************/
int main(int argc, char **argv)
{
	const char* p;
	char        error[512];
	int         i;
	int         first_arg = 1;
	time_t      now;
	user_t      user;

	fprintf(stderr, "\nMAKEUSER v%s-%s - Adds User to Synchronet User Database\n"
	        , VERSION
	        , PLATFORM_DESC
	        );

	if (argc < 2) {
		printf("%s", usage);
		return 1;
	}

	if (strcspn(argv[first_arg], "/\\") != strlen(argv[first_arg]))
		p = argv[first_arg++];
	else
		p = get_ctrl_dir(/* warn: */ TRUE);

	memset(&scfg, 0, sizeof(scfg));
	scfg.size = sizeof(scfg);
	SAFECOPY(scfg.ctrl_dir, p);

	if (chdir(scfg.ctrl_dir) != 0)
		fprintf(stderr, "!ERROR changing directory to: %s", scfg.ctrl_dir);

	printf("\nLoading configuration files from %s\n", scfg.ctrl_dir);
	if (!load_cfg(&scfg, /* text: */ NULL, /* prep: */ TRUE, /* node: */ FALSE, error, sizeof(error))) {
		fprintf(stderr, "!ERROR loading configuration files: %s\n", error);
		exit(1);
	}

	now = time(NULL);

	memset(&user, 0, sizeof(user));

	/****************/
	/* Set Defaults */
	/****************/

	newuserdefaults(&scfg, &user);

	for (i = first_arg; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (argv[i + 1] == NULL) {
				printf("%s", usage);
				return 1;
			}
			switch (toupper(argv[i++][1])) {
				case 'A':
					SAFECOPY(user.address, argv[i]);
					break;
				case 'B':
					SAFECOPY(user.birth, argv[i]);
					break;
				case 'L':
					SAFECOPY(user.location, argv[i]);
					break;
				case 'C':
					SAFECOPY(user.comment, argv[i]);
					break;
				case 'E':
					user.expire = (time32_t)(now + ((long)atoi(argv[i]) * 24L * 60L * 60L));
					break;
				case 'F':
					switch (toupper(argv[i - 1][2])) {
						case '1':
							user.flags1 = aftou32(argv[i]);
							break;
						case '2':
							user.flags2 = aftou32(argv[i]);
							break;
						case '3':
							user.flags3 = aftou32(argv[i]);
							break;
						case '4':
							user.flags4 = aftou32(argv[i]);
							break;
						case 'E':
							user.exempt = aftou32(argv[i]);
							break;
						case 'R':
							user.rest = aftou32(argv[i]);
							break;
						default:
							printf("%s", usage);
							return 1;
					}
					break;
				case 'G':
					user.sex = toupper(argv[i][0]);
					break;
				case 'H':
					SAFECOPY(user.handle, argv[i]);
					break;
				case 'N':
					SAFECOPY(user.netmail, argv[i]);
					break;
				case 'P':
					SAFECOPY(user.pass, argv[i]);
					strupr(user.pass);
					break;
				case 'R':
					SAFECOPY(user.name, argv[i]);
					break;
				case 'S':
					user.level = atoi(argv[i]);
					break;
				case 'T':
					SAFECOPY(user.phone, argv[i]);
					break;
				case 'Z':
					SAFECOPY(user.zipcode, argv[i]);
					break;
				default:
					printf("%s", usage);
					return 1;
			}
		}
		else
			SAFECOPY(user.alias, argv[i]);
	}

	if (user.alias[0] == 0) {
		printf("%s", usage);
		return 1;
	}

	if ((i = matchuser(&scfg, user.alias, FALSE)) != 0) {
		printf("!User (%s #%d) already exists\n", user.alias, i);
		return 2;
	}

	if (user.handle[0] == 0)
		SAFECOPY(user.handle, user.alias);
	if (user.name[0] == 0)
		SAFECOPY(user.name, user.alias);

	if ((i = newuserdat(&scfg, &user)) != 0) {
		fprintf(stderr, "!ERROR %d adding new user record\n", i);
		return i;
	}

	printf("User record #%d (%s) created successfully.\n", user.number, user.alias);

	return 0;
}

