/*
**  COMMAFMT.C
**
**  Public domain by Bob Stout
**
**  Notes:  1. Use static buffer to eliminate error checks on buffer overflow
**             and reduce code size.
**          2. By making the numeric argument a long and prototyping it before
**             use, passed numeric arguments will be implicitly cast to longs
**             thereby avoiding int overflow.
**          3. Use the thousands grouping and thousands separator from the
**             ANSI locale to make this more robust.
*/

/* #define TEST */

#include <string.h>
#ifdef TEST
 #include <stdio.h>
 #include <stdlib.h>
#endif

#define NUL '\0'


size_t commafmt(char   *buf,            /* Buffer for formatted string  */
                int     bufsize,        /* Size of buffer               */
                long    N)              /* Number to convert            */
{
      int len = 1, posn = 1, sign = 1;
      char *ptr = buf + bufsize - 1;

      if (2 > bufsize)
      {
ABORT:      *buf = NUL;
            return 0;
      }

      *ptr-- = NUL;
      --bufsize;
      if (0L > N)
      {
            sign = -1;
            N = -N;
      }

      for ( ; len <= bufsize; ++len, ++posn)
      {
            *ptr-- = (char)((N % 10L) + '0');
            if (0L == (N /= 10L))
                  break;
            if (0 == (posn % 3))
            {
                  *ptr-- = ',';
                  ++len;
            }
            if (len >= bufsize)
                  goto ABORT;
      }

      if (0 > sign)
      {
            if (0 == bufsize)
                  goto ABORT;
            *ptr-- = '-';
            ++len;
      }

      strcpy(buf, ++ptr);
      return (size_t)len;
}

#ifdef TEST

main(int argc, char *argv[])
{
      size_t len;
      char buf[20];
      long N;

      N = strtol(argv[1], NULL, 10);
      len = commafmt(buf, 20, N);
	  printf("%s converts to %s and returned %d\n", argv[1], buf, len);

	  (void)argc;

      return EXIT_SUCCESS;
}

#endif
