/* Chew -- Program to make .GUM encrypted files */

/*
 * Compression code taken from SIXPACK.C by Philip G. Gage.
 * For original source code either do a search for sixpack.c
 * or go here:
 * 
 * http://www.ifi.uio.no/in383/src/ddj/sixpack/
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __MSDOS__
#include <alloc.h>        /* Use <malloc.c> for Power C */
#else /* !__MSDOS__ */
# include <memory.h>
# include "defines.h"
# define farmalloc malloc  /* for flat memory models */
# define farfree free      /* for flat memory models */
# include <sys/types.h>
# include <sys/stat.h>
# include <time.h>
# include <errno.h>

unsigned _dos_getftime (int, unsigned *, unsigned *);
#endif /* __MSDOS__ */

#ifdef __unix__
#include <ctype.h>
#include <unistd.h>
#endif

#define MAX_TOKEN_CHARS 32
#define CODE1           0x7D
#define CODE2           0x1F

void ClearAll ( void );
void AddGUM(FILE *fpGUM, char *pszFileName);
void GetToken ( char *szString, char *szToken );
void AddDir(FILE *fpGUM, char *pszDirName);

#define TEXTSEARCH 1000   /* Max strings to search in text file */
#define BINSEARCH   200   /* Max strings to search in binary file */
#define TEXTNEXT     50   /* Max search at next character in text file */
#define BINNEXT      20   /* Max search at next character in binary file */
#define MAXFREQ    2000   /* Max frequency count before table reset */ 
#define MINCOPY       3   /* Shortest string copy length */
#define MAXCOPY      64   /* Longest string copy length */
#define SHORTRANGE    3   /* Max distance range for shortest length copy */
#define COPYRANGES    6   /* Number of string copy distance bit ranges */
short copybits[COPYRANGES] = {4,6,8,10,12,14};   /* Distance bits */

#define CODESPERRANGE (MAXCOPY - MINCOPY + 1)
int copymin[COPYRANGES], copymax[COPYRANGES];
int maxdistance, maxsize;
int distance, insert = MINCOPY, dictfile = 0, binary = 0;

#define NIL -1                    /* End of linked list marker */
#define HASHSIZE 16384            /* Number of entries in hash table */
#define HASHMASK (HASHSIZE - 1)   /* Mask for hash key wrap */
short FAR *head;
short FAR *tail;       /* Hash table */
short FAR *succ;
short FAR *pred;       /* Doubly linked lists */
unsigned char *buffer;            /* Text buffer */

/* Define hash key function using MINCOPY characters of string prefix */
#define getkey(n) ((buffer[n] ^ (buffer[(n+1)%maxsize]<<4) ^ \
                   (buffer[(n+2)%maxsize]<<8)) & HASHMASK)

/* Adaptive Huffman variables */
#define TERMINATE 256             /* EOF code */
#define FIRSTCODE 257             /* First code for copy lengths */
#define _MAXCHAR (FIRSTCODE+COPYRANGES*CODESPERRANGE-1)
#define SUCCMAX (_MAXCHAR+1)
#define TWICEMAX (2*_MAXCHAR+1)
#define ROOT 1
short left[_MAXCHAR+1], right[_MAXCHAR+1];  /* Huffman tree */
short up[TWICEMAX+1], freq[TWICEMAX+1];

/*** Bit packing routines ***/

int input_bit_count = 0;           /* Input bits buffered */
int input_bit_buffer = 0;          /* Input buffer */
int output_bit_count = 0;          /* Output bits buffered */
int output_bit_buffer = 0;         /* Output buffer */
long bytes_in = 0, bytes_out = 0;  /* File size counters */

long TotalBytes = 0;

/* Write one bit to output file */
void output_bit(output,bit)
  FILE *output;
  int bit;
{
  output_bit_buffer <<= 1;
  if (bit) output_bit_buffer |= 1;
  if (++output_bit_count == 8) {
    putc(output_bit_buffer,output);
    output_bit_count = 0;
    ++bytes_out;
  }
}

/* Read a bit from input file */
int input_bit(input)
  FILE *input;
{
  int bit;

  if (input_bit_count-- == 0) {
    input_bit_buffer = getc(input);
    if (input_bit_buffer == EOF) {
      printf(" UNEXPECTED END OF FILE\n");
      exit(1);
    }
    ++bytes_in;
    input_bit_count = 7;
  }
  bit = (input_bit_buffer & 0x80) != 0;
  input_bit_buffer <<= 1;
  return(bit);
}

/* Write multibit code to output file */
void output_code(output,code,bits)
  FILE *output;
  int code,bits;
{
  int i;

  for (i = 0; i<bits; i++) {
    output_bit(output,code & 0x01);
    code >>= 1;
  }
}

/* Read multibit code from input file */
int input_code(input,bits)
  FILE *input;
  int bits;
{
  int i, bit = 1, code = 0;

  for (i = 0; i<bits; i++) {
    if (input_bit(input)) code |= bit;
    bit <<= 1;
  }
  return(code);
}

/* Flush any remaining bits to output file before closing file */
void flush_bits(output)
  FILE *output;
{
  if (output_bit_count > 0) {
    putc((output_bit_buffer << (8-output_bit_count)),output);
    ++bytes_out;
  }
}

/*** Adaptive Huffman frequency compression ***/

/* Data structure based partly on "Application of Splay Trees
   to Data Compression", Communications of the ACM 8/88 */

/* Initialize data for compression or decompression */
void initialize()
{
  int i, j;

  /* Initialize Huffman frequency tree */
  for (i = 2; i<=TWICEMAX; i++) {
    up[i] = i/2;
    freq[i] = 1;
  }
  for (i = 1; i<=_MAXCHAR; i++) {
    left[i] = 2*i;
    right[i] = 2*i+1;
  }

  /* Initialize copy distance ranges */
  j = 0;
  for (i = 0; i<COPYRANGES; i++) {
    copymin[i] = j;
    j += 1 << copybits[i];
    copymax[i] = j - 1;
  }
  maxdistance = j - 1;
  maxsize = maxdistance + MAXCOPY;
}

/* Update frequency counts from leaf to root */
void update_freq(a,b)
  int a,b;
{
  do {
    freq[up[a]] = freq[a] + freq[b];
    a = up[a];
    if (a != ROOT) {
      if (left[up[a]] == a) b = right[up[a]];
      else b = left[up[a]];
    }
  } while (a != ROOT);

  /* Periodically scale frequencies down by half to avoid overflow */
  /* This also provides some local adaption and better compression */
  if (freq[ROOT] == MAXFREQ)
    for (a = 1; a<=TWICEMAX; a++) freq[a] >>= 1;
}

/* Update Huffman model for each character code */
void update_model(code)
  int code;
{
  int a, b, c, ua, uua;

  a = code + SUCCMAX;
  ++freq[a];
  if (up[a] != ROOT) {
    ua = up[a];
    if (left[ua] == a) update_freq(a,right[ua]);
    else update_freq(a,left[ua]);
    do {
      uua = up[ua];
      if (left[uua] == ua) b = right[uua];
      else b = left[uua];

      /* If high freq lower in tree, swap nodes */
      if (freq[a] > freq[b]) {
        if (left[uua] == ua) right[uua] = a;
        else left[uua] = a;
        if (left[ua] == a) {
          left[ua] = b; c = right[ua];
        } else {
          right[ua] = b; c = left[ua];
        }
        up[b] = ua; up[a] = uua;
        update_freq(b,c); a = b;
      }
      a = up[a]; ua = up[a];
    } while (ua != ROOT);
  }
}

/* Compress a character code to output stream */
void compress(output,code)
  FILE *output;
  int code;
{
  int a, sp = 0;
  int stack[50];

  a = code + SUCCMAX;
  do {
    stack[sp++] = (right[up[a]] == a);
    a = up[a];
  } while (a != ROOT);
  do {
    output_bit(output,stack[--sp]);
  } while (sp);
  update_model(code);
}

/* Uncompress a character code from input stream */
int uncompress(input)
  FILE *input;
{
  int a = ROOT;

  do {
    if (input_bit(input)) a = right[a];
    else a = left[a];
  } while (a <= _MAXCHAR);
  update_model(a-SUCCMAX);
  return(a-SUCCMAX);
}

/*** Hash table linked list string search routines ***/

/* Add node to head of list */
void add_node(n)  
  int n;
{
  int key;

  key = getkey(n);
  if (head[key] == NIL) {
    tail[key] = n;
    succ[n] = NIL;
  } else {
    succ[n] = head[key];
    pred[head[key]] = n;
  }
  head[key] = n;
  pred[n] = NIL;
}

/* Delete node from tail of list */
void delete_node(n)
  int n;
{
  int key;

  key = getkey(n);
  if (head[key] == tail[key])
    head[key] = NIL;
  else {
    succ[pred[tail[key]]] = NIL;
    tail[key] = pred[tail[key]];
  }
}

/* Find longest string matching lookahead buffer string */
int match(n,depth)
  int n,depth;
{
  int i, j, index, key, dist, len, best = 0, count = 0;

  if (n == maxsize) n = 0;
  key = getkey(n);
  index = head[key];
  while (index != NIL) {
    if (++count > depth) break;     /* Quit if depth exceeded */
    if (buffer[(n+best)%maxsize] == buffer[(index+best)%maxsize]) {
      len = 0;  i = n;  j = index;
      while (buffer[i]==buffer[j] && len<MAXCOPY && j!=n && i!=insert) {
        ++len;
        if (++i == maxsize) i = 0;
        if (++j == maxsize) j = 0;
      }
      dist = n - index;
      if (dist < 0) dist += maxsize;
      dist -= len;
      /* If dict file, quit at shortest distance range */
      if (dictfile && dist > copymax[0]) break;
      if (len > best && dist <= maxdistance) {     /* Update best match */
        if (len > MINCOPY || dist <= copymax[SHORTRANGE+binary]) {
          best = len; distance = dist;
        }
      }
    }
    index = succ[index];
  }
  return(best);
}

/*** Finite Window compression routines ***/

#define IDLE 0    /* Not processing a copy */
#define COPY 1    /* Currently processing copy */

/* Check first buffer for ordered dictionary file */
/* Better compression using short distance copies */
int dictionary()
{
  int i = 0, j = 0, k, count = 0;

  /* Count matching chars at start of adjacent lines */
  while (++j < MINCOPY+MAXCOPY) {
    if (buffer[j-1] == 10) {
      k = j;
      while (buffer[i++] == buffer[k++]) ++count;
      i = j;
    }
  }
  /* If matching line prefixes > 25% assume dictionary */
  if (count > (MINCOPY+MAXCOPY)/4) dictfile = 1;
  return(0);
}

/* Encode file from input to output */
int encode(input,output)
  FILE *input, *output;
{
  int c, i, n=MINCOPY, addpos=0, len=0, full=0, state=IDLE, nextlen;

  initialize();
  head = farmalloc((unsigned long)HASHSIZE*sizeof(short));
  tail = farmalloc((unsigned long)HASHSIZE*sizeof(short));
  succ = farmalloc((unsigned long)maxsize*sizeof(short));
  pred = farmalloc((unsigned long)maxsize*sizeof(short));
  buffer = (unsigned char *) malloc(maxsize*sizeof(unsigned char));
  if (head==NULL || tail==NULL || succ==NULL || pred==NULL || buffer==NULL) {
    printf("Error allocating memory\n");
    exit(1);
  }

  /* Initialize hash table to empty */
  for (i = 0; i<HASHSIZE; i++) {
    head[i] = NIL;
  }

  /* Compress first few characters using Huffman */
  for (i = 0; i<MINCOPY; i++) {
    if ((c = getc(input)) == EOF) {
      compress(output,TERMINATE);
      flush_bits(output);
      return(bytes_in);
    }
    compress(output,c);  ++bytes_in;
    buffer[i] = c;
  }

  /* Preload next few characters into lookahead buffer */
  for (i = 0; i<MAXCOPY; i++) {
    if ((c = getc(input)) == EOF) break;
    buffer[insert++] = c;  ++bytes_in;
    if (c > 127) binary = 1;     /* Binary file ? */
  }
  dictionary();  /* Check for dictionary file */

  while (n != insert) {
    /* Check compression to insure really a dictionary file */
    if (dictfile && ((bytes_in % MAXCOPY) == 0))
      if (bytes_in/bytes_out < 2)
        dictfile = 0;     /* Oops, not a dictionary file ! */

    /* Update nodes in hash table lists */
    if (full) delete_node(insert);
    add_node(addpos);

    /* If doing copy, process character, else check for new copy */
    if (state == COPY) {
      if (--len == 1) state = IDLE;
    } else {

      /* Get match length at next character and current char */
      if (binary) {
        nextlen = match(n+1,BINNEXT);
        len = match(n,BINSEARCH);
      } else {
        nextlen = match(n+1,TEXTNEXT);
        len = match(n,TEXTSEARCH);
      }

      /* If long enough and no better match at next char, start copy */
      if (len >= MINCOPY && len >= nextlen) {
        state = COPY;

        /* Look up minimum bits to encode distance */
        for (i = 0; i<COPYRANGES; i++) {
          if (distance <= copymax[i]) {
            compress(output,FIRSTCODE-MINCOPY+len+i*CODESPERRANGE);
            output_code(output,distance-copymin[i],copybits[i]);
            break;
          }
        }
      }
      else   /* Else output single literal character */
        compress(output,buffer[n]);
    }

    /* Advance buffer pointers */
    if (++n == maxsize) n = 0;
    if (++addpos == maxsize) addpos = 0;

    /* Add next input character to buffer */
    if (c != EOF) {
      if ((c = getc(input)) != EOF) {
        buffer[insert++] = c;  ++bytes_in;
      } else full = 0;
      if (insert == maxsize) {
        insert = 0; full = 1;
      }
    }
  }

  /* Output EOF code and free memory */
  compress(output,TERMINATE);
  flush_bits(output);
  farfree(head); farfree(tail); farfree(succ); farfree(pred);
  free(buffer);

  return 0;
}

int main ( int argc, char **argv )
{
    FILE *fpGUM;
    FILE *fpList;
#ifdef __unix__
	FILE *fpAttr;
	struct stat tStat;
#endif
    char szLine[255], szFileName[128], szGumName[128], szFileList[128];

    if (argc == 3)
    {
        strcpy(szGumName, argv[1]);
        strcpy(szFileList, argv[2]);
    }
    else {
        strcpy(szGumName, "archive.gum");
        strcpy(szFileList, "files.lst");
    }

    //initialize();

    printf("CHEW v0.01\n\nChewing to %s\n\n", szGumName);

    fpGUM = fopen(szGumName, "wb");
    if (!fpGUM)
    {
        printf("Couldn't create %s file!\n", szGumName );
        return(0);
    }

    fpList = fopen(szFileList, "r");
    if (!fpList)
    {
        fclose(fpGUM);
        printf("Couldn't read listing file: %s\n", szFileList);
        return(0);
    }

#ifdef __unix__
	fpAttr = fopen("UnixAttr.DAT", "wb");
	if (!fpAttr)
	{
		printf("Couldn't create UnixAttr.DAT file\n");
		return(0);
	}
#endif

    for (;;)
    {
        if (fgets(szLine, 255, fpList) == NULL)
            break;

        GetToken(szLine, szFileName);

        if (szLine[0] == 0 || szLine[0] == '#')
            continue;

        if (szFileName[0] == '/')
        {
            // directory to create
            AddDir(fpGUM, szFileName);
#ifdef __unix__
			stat(szFileName, &tStat);
			tStat.st_mode&=511;
			fprintf(fpAttr, "%o %s\n", tStat.st_mode, szFileName+1);
#endif		
        }
        else
		{
            AddGUM(fpGUM, szFileName);
#ifdef __unix__
			stat(szFileName, &tStat);
			tStat.st_mode&=511;
			fprintf(fpAttr, "%o %s\n", tStat.st_mode, szFileName);
#endif	
		}
    }

#ifdef __unix__
	fclose(fpAttr);
	AddGUM(fpGUM, "UnixAttr.DAT");
	unlink("UnixAttr.DAT");
#endif

    fclose(fpGUM);
    fclose(fpList);

    //printf("output is %ld bytes\n", TotalBytes);
	return(0);
}

void AddGUM(FILE *fpGUM, char *pszFileName)
{
    char szKey[80];
/*    char acChunk[1024]; 
    char acEncryptedChunk[1024]; */
    char szEncryptedName[MAX_FILENAME_LEN];
    char *pcFrom, *pcTo;
    FILE *fpFromFile;
    long lFileSize, Offset1, Offset2;
    long lCompressSize;
    int /*BytesRead,*/ iTemp;
    unsigned date, time;

    ClearAll();

    for (iTemp = 0; iTemp < MAX_FILENAME_LEN; iTemp++)
        szEncryptedName[iTemp] = 0;

    /* encrypt the filename */
    pcFrom = pszFileName;
    pcTo = szEncryptedName;

    printf("Adding file: %s ", pszFileName);

    while (*pcFrom)
    {
        *pcTo = *pcFrom ^ CODE1;
        pcFrom++;
        pcTo++;
    }
    *pcTo = 0;
    //printf("Encrypted name = %s\n", szEncryptedName);

    /* make key using filename */
    sprintf(szKey, "%s%x%x", szEncryptedName, szEncryptedName[0], szEncryptedName[1]);
    //printf("key = '%s%x%x'\n", szEncryptedName, szEncryptedName[0], szEncryptedName[1]);

    pcTo = szKey;
    while (*pcTo)
    {
        *pcTo ^= CODE2;
        pcTo++;
    }


    /* write it to file */
    fwrite(&szEncryptedName, sizeof(szEncryptedName), sizeof(char), fpGUM);

    /* write filesize to psi file */
    fpFromFile = fopen(pszFileName, "rb");
    if (!fpFromFile)
    {
        printf("\aCouldn't open \"%s\"\n", pszFileName);
        exit(0);
    }
    fseek(fpFromFile, 0L, SEEK_END);
    lFileSize = ftell(fpFromFile);
    //printf(" (%ld bytes) ", lFileSize);
    fseek(fpFromFile, 0L, SEEK_SET);
    fwrite(&lFileSize, sizeof(long), 1, fpGUM);

    // record this offset since we come back later to write the compressed
    // size
    Offset1 = ftell(fpGUM);
    fwrite(&lCompressSize, sizeof(long), 1, fpGUM);

    // write datestamp
    _dos_getftime(fileno(fpFromFile), &date, &time);
    fwrite(&date, sizeof(unsigned short), 1, fpGUM);
    fwrite(&time, sizeof(unsigned short), 1, fpGUM);

    //=== encode here
    encode(fpFromFile, fpGUM);
    //printf("Packed from %ld bytes to %ld bytes\n",bytes_in,bytes_out);
    TotalBytes += bytes_out;
    //=== end here

    // we write the compressed size now
    Offset2 = ftell(fpGUM);
    fseek(fpGUM, Offset1, SEEK_SET);
    lCompressSize = bytes_out;
    fwrite(&lCompressSize, sizeof(long), 1, fpGUM);
    fseek(fpGUM, Offset2, SEEK_SET);

    bytes_in = 0; bytes_out = 0;

    fclose(fpFromFile);
    printf("Done.\n");

}

void GetToken ( char *szString, char *szToken )
{
	char *pcCurrentPos;
	unsigned int uCount;

	/* Ignore all of line after comments or CR/LF char */
	pcCurrentPos=(char *)szString;
	while(*pcCurrentPos)
	{
		if(*pcCurrentPos=='\n' || *pcCurrentPos=='\r')
		{
			*pcCurrentPos='\0';
			break;
		}
		++pcCurrentPos;
	}

	/* Search for beginning of first token on line */
	pcCurrentPos = (char *)szString;
	while(*pcCurrentPos && isspace(*pcCurrentPos)) ++pcCurrentPos;

	/* If no token was found, proceed to process the next line */
	if(!*pcCurrentPos)
	{
		szToken[0] = 0;
		szString[0] = 0;
		return;
	}

	/* Get first token from line */
	uCount=0;
	while(*pcCurrentPos && !isspace(*pcCurrentPos))
	{
		if(uCount<MAX_TOKEN_CHARS) szToken[uCount++]=*pcCurrentPos;
		++pcCurrentPos;
	}
	if(uCount<=MAX_TOKEN_CHARS)
		szToken[uCount]='\0';
	else
		szToken[MAX_TOKEN_CHARS]='\0';

	/* Find beginning of configuration option parameters */
	while(*pcCurrentPos && isspace(*pcCurrentPos)) ++pcCurrentPos;

	/* Trim trailing spaces from setting string */
	for(uCount=strlen(pcCurrentPos)-1;uCount>0;--uCount)
	{
		if(isspace(pcCurrentPos[uCount]))
		{
			pcCurrentPos[uCount]='\0';
		}
		else
		{
			break;
		}
	}
}

void ClearAll ( void )
{
    int iTemp;

    for (iTemp = 0; iTemp < _MAXCHAR+1; iTemp++)
        left[iTemp] = right[iTemp] = 0;
    for (iTemp = 0; iTemp < TWICEMAX+1; iTemp++)
        up[TWICEMAX+1] = freq[TWICEMAX+1] = 0;

    input_bit_count = 0;
    input_bit_buffer = 0;
    output_bit_count = 0;
    output_bit_buffer = 0;
    bytes_in = 0;
    bytes_out = 0;
    dictfile = 0;
    binary = 0;

    for (iTemp = 0; iTemp < COPYRANGES; iTemp++)
        copymin[COPYRANGES] = copymax[COPYRANGES] = 0;

    maxdistance = maxsize = 0;
    distance = 0; insert = MINCOPY;
}

void AddDir(FILE *fpGUM, char *pszDirName)
{
    char szKey[80];
    char szEncryptedName[MAX_FILENAME_LEN];
    char *pcFrom, *pcTo;
    int iTemp;

    for (iTemp = 0; iTemp < MAX_FILENAME_LEN; iTemp++)
        szEncryptedName[iTemp] = 0;

    /* encrypt the filename */
    pcFrom = pszDirName;
    pcTo = szEncryptedName;

    printf("Adding directory: %s\n", pszDirName);

    while (*pcFrom)
    {
        *pcTo = *pcFrom ^ CODE1;
        pcFrom++;
        pcTo++;
    }
    *pcTo = 0;
    //printf("Encrypted name = %s\n", szEncryptedName);

    /* make key using filename */
    sprintf(szKey, "%s%x%x", szEncryptedName, szEncryptedName[0], szEncryptedName[1]);
    //printf("key = '%s%x%x'\n", szEncryptedName, szEncryptedName[0], szEncryptedName[1]);

    pcTo = szKey;
    while (*pcTo)
    {
        *pcTo ^= CODE2;
        pcTo++;
    }

    // write dir name to file as is
    fwrite(szEncryptedName, sizeof(szEncryptedName), 1, fpGUM);
}

#ifndef __MSDOS__
unsigned _dos_getftime(int handle, unsigned *datep, unsigned *timep)
{
	struct stat file_stats;
	struct tm *file_datetime;
	struct tm file_dt;

	if (fstat(handle, &file_stats) != 0)
	{
		return EBADF;
		fputs ("fstat failed", stderr);
		fflush (stderr);
		exit (1);
	}

	file_datetime = localtime(&file_stats.st_mtime);
	if (!file_datetime)
	{
		fputs ("localtime failed", stderr);
		fflush (stderr);
		exit (1);
	}
	memcpy (&file_dt, file_datetime, sizeof(struct tm));

	*datep = 0;
	*datep = ((file_dt.tm_mday) & 0x1f);
	*datep |= ((file_dt.tm_mon + 1) & 0x0f) << 5;
	*datep |= ((file_dt.tm_year - 80) & 0x7f) << 9;

	*timep = 0;
	*timep = (((file_dt.tm_sec + 2) / 2) & 0x1f);
	*timep |= ((file_dt.tm_min) & 0x3f) << 5;
	*timep |= ((file_dt.tm_hour) & 0x1f) << 11;

	return 0;
}
#endif /* !__MSDOS__ */
