#include <datewrap.h>

#ifdef __unix__
#include <time.h>
void getdate(struct date *nyd)  {
  time_t tim;
  struct tm *dte;

  tim=time(NULL);
  dte=localtime(&tim);
  nyd->da_year=dte->tm_year+1900;
  nyd->da_day=dte->tm_mday;
  nyd->da_mon=dte->tm_mon+1;
}

void gettime(struct time *nyt)  {
  struct timeval tim;
  struct tm *dte;

  gettimeofday(&tim,NULL);
  dte=localtime(&tim.tv_sec);
  nyt->ti_min=dte->tm_min;
  nyt->ti_hour=dte->tm_hour;
  nyt->ti_sec=dte->tm_sec;
  nyt->ti_hund=tim.tv_usec/10000;
}
#endif

