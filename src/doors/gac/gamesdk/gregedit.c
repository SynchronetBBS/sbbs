/*    This is a BASIC, simple line editor for use with the OpenDoors
			package since this is about the only thing OpenDoors lacks.

			To use this ugly thing the way it is, include the code at the
			end of your program with an include statement:
							 #include "bp_lined.c"
			And declare the static character array "text" of 20K somewhere
			at the beginning of your code like so:
							 static char text[20000];

			o That's it, just make the call:   bp_lined(80) and you are set.
			o The user can hit a Ctrl-C, Ctrl-K, Ctrl-Z, or the ESCape key
			o when he is done entering in information. Blank trailing lines
			o are not saved by the editor so if the user hits the return key
			o 10 times before realizing that he needed to hit the ESC key
			o to exit, those last 10 blank lines won't be saved in the text
			o buffer.

			The only parameter to pass is an integer describing how wide
			you want the editor to be. In other words, this is the number
			that word-wrapping will occur at.
			When you press the ESC key, Ctrl-Z, Ctrl-C, or a Ctrl-K, the
			line editor will exit and will leave the text array stuffed
			full of whatever wonderfull things you had to say :-)

			This is not meant to be a ready to roll package, all though
			IT WORKS as is.
			This is meant for YOU to hack to your hearts content, fix it
			up, add some ansi, maybe a few twirling thingy's, whatever, and
			use in whatever program you like. I want no recognition, after
			looking at this code, you probably wouldn't want any either.

			If someone spruces this thing up and really makes it looking
			pretty usefull, please send me a copy of your code. I can be
			reached at FidoNet: 1:230/61, or just drop me a note in the
			OpenDoors echo.

			Actually, i'll have this thing spruced up and will release
			the code to the polished line editor in another week or so.
			This is also the base code for my full screen ansi editor that
			i'm just about done with. It is also for OpenDoors, but the
			source won't be give out, just the .obj's.
			This should be a great start for those who have complained that
			OpenDoors did not include a word-wrapping line editor.

			An example OpenDoors program and call should look like this:


*/

#include "gregedit.h"
#include "gac_edit.h"


/* Enter and edit text.
	 Make the call   bp_lined(cols)
	 where cols is the width of the editor that you choose. You must
	 choose a number between 2 and 80 inclusive. Word-Wrapping occurs at
	 this number.
	 RETURN: number of lines written/edited. The text[] array holds the message
					 that the user entered.
*/
INT16 caller_cols;
INT16 ln;  /* line number to start editing at */
INT16  end;   /* line number at end of file to edit */
INT16  lastline;  /* last line number, for detecting when to display */
char *cp,c;             /* character pointers for input from user */
char rptchar;           /* repeat-last-char count */
char *tp;    /* ptr to the text buffer itself */
char word[162];      /* word[] must be at least 2X max line len */
INT16  run;               /* 1 == keep inputting, etc */
INT16  pw;   /* length of 'word' (index into word[]) */
INT16  tw;   /* length of tp */
INT16  line;
INT16  arewedone; /* flag for the second hit of the enter key */

char cur_line[81];
// #define MSG_LEN 20001  //this should be in a header file somewhere
// extern char msg_text[];


/*--------------------
	Main entrance to the line editor.
 ---------------------*/
// INT16 gac_lined(INT16 y, INT16 end, char *text) /* pass how wide the editor should be and the maximum line number */
// 1/97 removed far
INT16 gac_lined(INT16 y, INT16 last) /* pass how wide the editor should be and the maximum line number */
 {
		INT16 i=0;

	// clear anything out of the buffer that may be present!
	memset(msg_text, '\0', MSG_LEN);

	arewedone=0;
	 msg_text[0] = '\0';
	 cur_line[0] = '\0';
	 word[0] = '\0';
//   end = 99; /* maximum of 99 lines allowed in the editor */
	 end = last;
	 ln = 0;   /* line number to start with */
	 caller_cols = y; /* this will come from a param passwd to bp_lined */
	 tp = &msg_text[ln * caller_cols]; /* ptr to the text */
	 tw= 0;         /* length of current line */

	 if (caller_cols > 80 || caller_cols< 2) /* columns must be < 80 and > 1 */
			caller_cols = 80;

	 lastline= -1;      /* trigger line # display */
	 rptchar= 0;        /* no character repeat count */
//   ins_line(ln,end, msg_text);    /* incase we're I)nserting */
//   ins_line(ln,end);  /* incase we're I)nserting */

	 run = 1;
	 while ( run && (ln < end))
		{
			if (ln != lastline)
				{
					 lastline = ln;      /* only once each line */
					 line = 0;            /* override "More?" */
					 od_printf("\r\n"); /* prevent overwrap */
					 od_printf("%2u: ",ln + 1);    /* display line number */
					 for (cp = tp; *cp; ++cp) /* display line so far */
							{
								if (*cp >= ' ') od_printf("%c",*cp);
							}
				}

/* OK, now build a 'word', character by character. We stop (unbeknownst
to the user) when we get a CR, space, etc. We also "fall out" if the word
becomes too long to fit on the screen; hence the "while strlen + pc < width".
*/
	 pw= 0;
	 /* do the following "while it fits the line" */
	 while (tw + pw + WIDOFF < caller_cols)
			{
				if (rptchar != 0)   /* if repeated character */
							--rptchar;    /* count one, */
				else
							c = od_get_key(TRUE);  /* get one word */

				if ((c == SUB) || (c == VT) || (c == ETX) || (c == 27))
					 {
							run= 0;     /* if ^Z, ^K or ^C or ESCape */
							// allow user to quit editing message
							if (c == ETX)
							return(0);
							break;      /* stop editing */
					 }
				if (c == CR)     /* stop if CR, */
					 break;
				if (c == BS)    /* DEL becomes BS */
						c = BS;

				if (c == TAB)    /* if we get a tab, */
					 {
							/* count = column mod 8 */
							rptchar = 8 - ((tw + pw) % 8);
							c= ' ';       /* set the character, */
							continue;     /* then go do it */
					 }
			 if (c == CAN)        /* if delete line */
					 {
							/* for the entire line */
							rptchar= tw + pw;
							c = BS;     /* set as back space, */
							continue;   /* go do it */
					 }

/* If a character is deleted, backspace on the screen. First delete characters
from the word we are building, if that is empty, then from the line we
are building.
*/

			 if (c == BS)
					{
						 /* remove characters from the word */
						 if (pw > 0)
								--pw;
						 else
								/* or the line */
								if (tw > 0)
									 --tw;
								else
									 continue;     /* (neither, ignore BS) */
						 erase(1);      /* then erase on screen */
						 od_printf("%c",BS);
						 continue;
					}

			 if (c >= ' ')
					{
						 od_printf("%c",c);
						 /* store characters in word */
						 word[pw++] = c;
					}
			 if (pw >= sizeof(word) - 1) /* max length */
						 break;
			 if (c == TAB) break;       /* word terminators */
			 if (c == ' ') break;
			 if (c == ',') break;
			 if (c == ';') break;
	 }   /* closes off the while loop */
			 word[pw] = '\0';     /* terminate the word */
			 tp[tw] = '\0';   /* and the line so far */

/* We have text on the screen, and a word in the word[] array. If it will
fit within the screen line length, display as-is, if not, do a CR/LF and
do it on the next line. (This gets executed when word[] is zero length,
ie. only a CR is entered.) */

	if (tw + pw + WIDOFF < caller_cols)
					 {
							strcat(tp,word);     /* add it if it fits, */
							tw += pw; /* current line length, */

/* When we go to the next line, insert a blank line; if we're entering
new text this is a waste of time, but if we're inserting it moves existing
text down one line. */

					 }
					 else
						 {    /* doesnt fit */
								erase(pw);    /* clear last word from screen, */
								/* no 0 length lines please */
								if (tw == 0)
									 strcat(tp," ");
								tp += caller_cols;    /* go to next line */
								++ln;                            /* next line # */
//                              ins_line(ln,end, msg_text);                /* insert a blank one, */
//                              ins_line(ln,end);                /* insert a blank one, */
								strcpy(tp,word);      /* put word on next line, */
								tw= pw; /* also its line length */
						}

/* If a CR was entered, then we want to force things to start on the next
line. Also stuff a CR when the line is ended if its not a blank line by itself. */

				if (c == CR)  /* if user hit a hard return */
					 {
							strcat(tp,"\r\n");    /* add a CR/LF */
							tp += caller_cols;    /* next line */
							++ln;                            /* next line # */
//                          ins_line(ln,end, msg_text);                /* insert a blank one, */
//                          ins_line(ln,end);                /* insert a blank one, */
							tw= 0;         /* word line is blank */
					 }
		}
				if ( ! *tp)
							strcpy(tp,"\r\n");

/* Now find the first unused line, and return that as the next line. */

				while (*tp)  /* find the next blank line */
					{
						 if (ln >= end)     /* dont go past the end */
								 break;
						 tp += caller_cols;
						 ++ln;
					}

// copy the buffer contents into a single string, instead of
// having them seperated (start at second string)
				tp = &msg_text[caller_cols];
				for (i=1; i<ln; i++)
				{
					strcpy(cur_line, tp);
					strcat(msg_text, cur_line);
					tp += caller_cols;
				}


				return(ln);  /* return the number of lines written */
} /* END OF MAIN BP_LINED MODULE  */




/* Delete a line from the text array. */

// this will have to be modified if I ever use it, since I am concatanating
// each line to the text array

//del_line(line, end)
//INT16 line,end;
//del_line(INT16 line, INT16 end, char *msg_text)
/*
INT16 del_line(INT16 line, INT16 end )
{
	 char *tp;

	 tp= &msg_text[line * caller_cols];    // ptr to 1st line
	 while (line < end)
		 {
				 strcpy(tp,tp + caller_cols); // copy the line,
				 tp += caller_cols;                      // next ...
				 ++line;
		 }
				 *tp= '\0';                    // last line is empty
				 return(--end);
}
*/


/* Insert a blank line. Copies all lines down one, dropping
off one at the bottom (which is presumably blank.) */

//ins_line(line,end)
//INT16 line,end;
//ins_line(INT16 line, INT16 end, char *msg_text)
/*
INT16 ins_line(INT16 line, INT16 end)
{
	 INT16 i;

	 // tp == address of last line
	 tp= &msg_text[end * caller_cols];
	 for (i = end - line; i; --i)
		 {
				strcpy(tp,tp - caller_cols); // copy line down,
				tp -= caller_cols;                      // next ...
		 }
	 *tp= '\0';       // inserted line is empty
	 return(end);
}

*/


/* Erase a word. Backspace, then type spaces. */
/* I know this is kludggy, but it works       */

//INT16 n;
// 1/97 removed far
INT16 erase(INT16 n)
{
	 INT16 i;

	 for (i= 0; i < n; i++)
			 od_printf("%c",BS);
	 for (i= 0; i < n; i++)
			 od_printf("%c",' ');
	 return(1);
}

