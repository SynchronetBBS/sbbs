/* SMMDEFS.H */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#define SMM_VER "2.10"

#define SMM_METRIC			(1<<0)	/* Use metric measurements */

#define USER_DELETED		(1<<0)	/* Bits for user.misc field */
#define USER_FROMSMB		(1<<1)	/* Imported from SMB */
#define USER_REQZIP 		(1<<2)	/* Require mate in zip range */
#define USER_REQAGE 		(1<<3)	/* Require mate in age range */
#define USER_REQWEIGHT		(1<<4)	/* Require mate in weight range */
#define USER_REQHEIGHT		(1<<5)	/* Require mate in height range */
#define USER_REQINCOME		(1<<6)	/* Require mate in income range */
#define USER_REQMARITAL 	(1<<7)	/* Require marital status */
#define USER_REQZODIAC		(1<<8)	/* Require zodiac */
#define USER_REQRACE		(1<<9)	/* Require race */
#define USER_REQHAIR		(1<<10) /* Require hair */
#define USER_REQEYES		(1<<11) /* Require eyes */
#define USER_FRIEND 		(1<<12) /* Seeking same-sex friendship */
#define USER_PHOTO			(1<<13) /* Photo attached to profile */


#define ZODIAC_ARIES		(1<<0)	/* Mar 21 - Apr 19 */
#define ZODIAC_TAURUS		(1<<1)	/* Apr 20 - May 20 */
#define ZODIAC_GEMINI		(1<<2)	/* May 21 - Jun 20 */
#define ZODIAC_CANCER		(1<<3)	/* Jun 21 - Jul 22 */
#define ZODIAC_LEO			(1<<4)	/* Jul 23 - Aug 22 */
#define ZODIAC_VIRGO		(1<<5)	/* Aug 23 - Sep 22 */
#define ZODIAC_LIBRA		(1<<6)	/* Sep 23 - Oct 22 */
#define ZODIAC_SCORPIO		(1<<7)	/* Oct 23 - Nov 21 */
#define ZODIAC_SAGITTARIUS	(1<<8)	/* Nov 22 - Dec 21 */
#define ZODIAC_CAPRICORN	(1<<9)	/* Dec 22 - Jan 19 */
#define ZODIAC_AQUARIUS 	(1<<10) /* Jan 20 - Feb 18 */
#define ZODIAC_PISCES		(1<<11) /* Feb 19 - Mar 20 */

#define HAIR_BLONDE 		(1<<0)
#define HAIR_BROWN			(1<<1)
#define HAIR_RED			(1<<2)
#define HAIR_BLACK			(1<<3)
#define HAIR_GREY			(1<<4)
#define HAIR_OTHER			(1<<5)

#define EYES_BLUE			(1<<0)
#define EYES_GREEN			(1<<1)
#define EYES_HAZEL			(1<<2)
#define EYES_BROWN			(1<<3)
#define EYES_OTHER			(1<<4)

#define RACE_WHITE			(1<<0)
#define RACE_BLACK			(1<<1)
#define RACE_HISPANIC		(1<<2)
#define RACE_ASIAN			(1<<3)
#define RACE_AMERINDIAN 	(1<<4)
#define RACE_MIDEASTERN 	(1<<5)
#define RACE_OTHER			(1<<6)

#define MARITAL_SINGLE		(1<<0)
#define MARITAL_MARRIED 	(1<<1)
#define MARITAL_DIVORCED	(1<<2)
#define MARITAL_WIDOWED 	(1<<3)
#define MARITAL_OTHER		(1<<4)

typedef struct {

	uchar	txt[82];
	uchar	answers;		/* Total answers */
	uchar	allowed;		/* number of answers allowed for self */
	uchar	ans[16][62];	/* Answers */

	} question_t;

typedef struct {

	uchar	name[9];
	uchar	desc[26];
	uchar	req_age;
	uchar	total_ques;
	question_t	que[20];

	} questionnaire_t;

typedef struct {

	uchar	name[9];	   /* Name of questionnaire */
	ushort	self[20];	   /* Answers to questions */
	ushort	pref[20];	   /* Preferred partner's answer(s) */

	} queans_t;

typedef struct {

	ulong	number; 		/* User's number on BBS */
	ulong	system; 		/* CRC-32 of system name */
	ulong	name;
	ulong	updated;

	} ixb_t;

typedef struct {

	uchar	name[26];		/* User's name or alias */
	uchar	realname[26];	/* User's name or alias on the BBS */
	uchar	system[41]; 	/* BBS name */
	ulong	number; 		/* User's number on BBS */
	uchar	birth[9];		/* MM/DD/YY format */
	uchar	zipcode[11];	/* Zip code */
	uchar	location[31];	/* City, state */
	ushort	pref_zodiac;	/* Preferred zodiac sign */
	uchar	sex;			/* 'M' or 'F' */
	uchar	pref_sex;
	uchar	marital;
	uchar	pref_marital;
	uchar	race;
	uchar	pref_race;
	uchar	hair;			/* hair color */
	uchar	pref_hair;		/* preferred hair color */
	uchar	eyes;			/* eye color */
	uchar	pref_eyes;		/* preferred eye color */
	ushort	weight; 		/* Sad, we have to use 16-bits! */
	uchar	height; 		/* in inches */
	ushort	min_weight;
	ushort	max_weight;
	uchar	min_height;
	uchar	max_height;
	uchar	min_age;
	uchar	max_age;
	uchar	min_zipcode[11];
	uchar	max_zipcode[11];
	ulong	income;
	ulong	min_income;
	ulong	max_income;
	time_t	created;
	time_t	updated;
	uchar	note[5][51];
	ulong	misc;
	time_t	lastin;
	uchar	min_match;
	uchar	purity; 		/* Purity % */
	uchar	mbtype[5];		/* Myers-Briggs personality type */
	time_t	photo;			/* Date last photo hatched */
	uchar	filler[60]; 	/* Available for future use */
	queans_t   queans[5];

	} user_t;

typedef struct {

	uchar	name[26];		/* User's name or alias */
    uchar   system[41];     /* BBS name */
	uchar	text[5][71];	/* Writing */
	time_t	written;		/* Date/time created */
	time_t	imported;

	} wall_t;

#define TGRAM_NOTICE "\1n\1h\1mNew telegram in \1wMatch Maker \1mfor you!" \
                "\7\1n\r\n"
#define DEFAULT_ZMODEM_SEND "%!dsz portx %u,%i sz %f"
#define TM_YEAR(yy)		((yy)%100)
