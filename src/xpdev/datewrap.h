#ifndef _DATEWRAP_H_
#define _DATEWRAP_H_


#ifdef __unix__
#include <sys/time.h>

struct date {
  short da_year;
  char  da_day;
  char  da_mon;
};

struct time {
  unsigned char ti_min;
  unsigned char ti_hour;
  unsigned char ti_hund;
  unsigned char ti_sec;
};

void getdate(struct date *nyd);
void gettime(struct time *nyt);
#else
#include <dos.h>
#endif


#endif
