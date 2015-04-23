
/****************************************************************************
 * date.c
 * Minimal POSIX-based date command
 *
 *   Copyright (C) 2015 Gert van den Berg. All rights reserved.
 *   Author: Gert van den Berg posix-date@mohag.net
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <malloc.h>
#include <stdbool.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

//#define DATE_ALLOW_NEG_EPOCH 
//#define DATE_ABSOLUTE_MINIMAL
#define DATE_DYNAMIC_OUTSIZE    /* Dyanmically resize output buffer */
#define DATE_INITIAL_SIZE 30    /* Enough for a default output */
#define DATE_DYN_GROWTH DATE_INITIAL_SIZE       /* Amount to grow by */
#ifdef DATE_DYNAMIC_OUTSIZE
#  define DATE_MAX_OUTSIZE 1000
#else
#  define DATE_MAX_OUTSIZE 100
#endif

/* Replacements for nice to have functions */
#ifdef DATE_ABSOLUTE_MINIMAL
#  define usage()
#  define error(code,msg,errno) code
#endif

/* free is not used without dynamic allocation, so disabling it is better than another #ifdef... */
#ifndef DATE_DYNAMIC_OUTSIZE
#  define free(a)
#endif

/* Errors */
#define ETOOMANYARGS 1
#define EUTCMODE 2
#define ERESPRINT 3
#define ENONDIGIT 4
#define ETIMEFMT 5
#define ESEC 6
#define ETIMELEN 7
#define ENEGEPOCH 8
#define EMON 9
#define EDMON 10
#define EHOUR 11
#define EMIN 12
#define ESETTIME 13
#define ECMDLINE 14
#define EBUFFALLOC 15
#define ESHOWHELP 100           /* no an error */

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

#ifndef DATE_ABSOLUTE_MINIMAL
static void usage(void)
{
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "    date [-u] [+format]\n");
  fprintf(stderr, "    date [-u] mmddhhmm[[cc]yy][.ss]\n");
  fprintf(stderr, "    date -h\n\n");
  fprintf(stderr,
          "Use the given format to display the date or set the date/time.\n");
}

static char *errormsg(const unsigned char code)
{
  switch (code)
    {
    case ETOOMANYARGS:
      return "too many arguments";
      break;
    case EUTCMODE:
      return "unable to set UTC mode";
      break;
    case ERESPRINT:
      return "unable to print result";
      break;
    case ENONDIGIT:
      return "invalid character in time string";
      break;
    case ETIMEFMT:
      return "time formatting failed";
      break;
    case ESEC:
      return "invalid seconds value";
      break;
    case ETIMELEN:
      return "invalid time length";
      break;
    case ENEGEPOCH:
      return "negative epoch time";
      break;
    case EMON:
      return "invalid month";
      break;
    case EDMON:
      return "invalid day of month";
      break;
    case EHOUR:
      return "invalid hour";
      break;
    case EMIN:
      return "invalid min";
      break;
    case ESETTIME:
      return "unable to set time";
      break;
    case ECMDLINE:
      return "invalid command line";
      break;
    case EBUFFALLOC:
      return "unable to allocate buffer!";
      break;
    case ESHOWHELP:            /* Should not be hit, but here in case... */
      return "help requested";
      break;
    default:
      return "unknown error";
      break;
    }
}

static int error(const unsigned char code, const int errno_input)
{
  char *message;

  message = errormsg(code);

  fprintf(stderr, "Error: %s", message);
  if (errno_input)
    {
      fprintf(stderr, ": %s\n", strerror(errno_input));
    }
  else
    {
      fprintf(stderr, "\n\n");

      /* If we don't have errno, the user was wrong */

      usage();
    }
  return (int)code;
}
#endif

#define ATOI2(str,pos) ((str[pos]-'0')*10 + (str[pos+1] - '0'))
static int settime(char *timestring)
{
  struct tm time;
  char *dot;
  struct timespec timetoset;
  unsigned char i;
  size_t len;

  dot = NULL;
  timetoset.tv_nsec = 0;
  len = strlen(timestring);

  /* Loop to check that at least the general format is correct. Also handles
   * detecting seconds */
  for (i = 0; i < len; i++)
    {
      if (!isdigit(timestring[i]))
        {
          if (timestring[i] == '.' && dot == NULL)
            {
              dot = timestring + i;
            }
          else
            {
              return error(ENONDIGIT, 0);
            }
        }
    }

  /* Actually parse seconds */
  if (dot != NULL)
    {
      *dot++ = '\0';            /* Split off seconds string */
      if (strlen(dot) != 2)
        {
          return error(ESEC, 0);
        }
      time.tm_sec = ATOI2(dot, 0);

      /* Range check. This allows for a single leap second as per C11 */
      if (time.tm_sec > 60)
        return error(ESEC, 0);
    }
  else
    {
      time.tm_sec = 0;
    }

  /* POSIX defined date string parsing */
  len = strlen(timestring);     /* need to recalculate due to dot */
  switch (len)
    {
    case 12:
      time.tm_year =
        (ATOI2(timestring, 8) * 100 + ATOI2(timestring, 10)) - 1900;
      /* fallthrough */
    case 10:
      /* This should be skipped by the previous case, but a break is overkill */
      if (len == 10)
        {
          time.tm_year = ATOI2(timestring, 8);
          if (time.tm_year < 69)        /* Two digit handled as per POSIX */
            time.tm_year += 100;
        }
      /* fallthrough */
    case 8:
      time.tm_mon = ATOI2(timestring, 0);
      if (--(time.tm_mon) > 11) /* tm months are zero based */
        return error(EMON, 0);
      time.tm_mday = ATOI2(timestring, 2);
      if (time.tm_mday > 31 || time.tm_mday < 1)
        return error(EDMON, 0);
      time.tm_hour = ATOI2(timestring, 4);
      if (time.tm_hour > 23)
        return error(EHOUR, 0);
      time.tm_min = ATOI2(timestring, 6);
      if (time.tm_min > 59)
        return error(EMIN, 0);
      break;
    default:
      return error(ETIMELEN, 0);
    }

  /* Optional extended year check */
#ifndef DATE_ALLOW_NEG_EPOCH
  if (time.tm_year < 70)
    return error(ENEGEPOCH, 0);
#endif

  time.tm_isdst = -1;
  time.tm_wday = 0;             // 0-6
  time.tm_yday = 0;             // 0-365
  printf("%d-%d-%d %d:%d:%d\n", time.tm_year + 1900, time.tm_mon + 1,
         time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);
  timetoset.tv_sec = mktime(&time);
  if (clock_settime(CLOCK_REALTIME, &timetoset) == 0)
    {
      return 0;
    }
  else
    {
      return error(ESETTIME, errno);
    }
}

static int showtime(const char *format)
{
#ifdef DATE_DYNAMIC_OUTSIZE
  char *buffer;
  size_t buffsize;
#else
  char buffer[DATE_MAX_OUTSIZE];
#endif
  struct tm *nowtm;
  time_t now;
  size_t result;

  now = time(NULL);             /* This uses std C time instead of a POSIX
                                 * function */
  nowtm = localtime(&now);
#ifndef DATE_DYNAMIC_OUTSIZE
  /* Keep buffsize usable */
#  define buffsize DATE_MAX_OUTSIZE
#else
  buffsize = DATE_INITIAL_SIZE;
  buffer = NULL;
  do
    {
      buffer = realloc(buffer, buffsize * sizeof(char));
      if (buffer == NULL)
        {
          return error(EBUFFALLOC, 0);
        }
#endif

      result = strftime(buffer, buffsize, format, nowtm);
#ifdef DATE_DYNAMIC_OUTSIZE
      buffsize += DATE_DYN_GROWTH;
    }
  while (result == 0 && buffer[0] != 0 && buffsize <= DATE_MAX_OUTSIZE);
#endif

  /* This might incorrectly succeed on error if the system's strftime sets the
   * first character to \0 on failure (The result is unspecified in the
   * standards, so it is possible) */
  if (result == 0 && buffer[0] != 0)
    {
      return error(ETIMEFMT, 0);
    }
  if (fprintf(stdout, "%s\n", buffer) > 0)      /* Will at least print \n */
    {
      free(buffer);             /* See macros */
      return 0;
    }
  else
    {
      free(buffer);
      return error(ERESPRINT, 0);
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, char **argv)
{
  int index;
  char *string;
  int option;
  unsigned char err;

  err = 0;
  string = NULL;

  while ((option = getopt(argc, argv, "uh")) != -1)
    {
      switch (option)
        {
        case 'u':
          {
            if (setenv("TZ", "UTC0", 1) != 0)
              {
                err = EUTCMODE;
              }
            else
              {
                tzset();        /* Apply changed timezone */
              }
          }
          break;
        case 'h':
          {
            err = ESHOWHELP;
          }
          break;
        default:
          {
            err = ECMDLINE;
          }
          break;
        }
    }
  for (index = optind; index < argc; index++)
    {
      if (string != NULL)       /* This will trigger if we loop through here
                                 * more than once... */
        {
          err = ETOOMANYARGS;
        }
      string = argv[index];
    }

  /* Check getopt loop result. NuttX seems to have problems if the loop is
   * interrupted... */
  switch (err)
    {
    case ETOOMANYARGS:
      {
        return error(ETOOMANYARGS, 0);
      }
      break;
    case EUTCMODE:
      {
        return error(EUTCMODE, 0);
      }
      break;
    case ECMDLINE:
      {
        return error(ECMDLINE, 0);
      }
      break;
    case ESHOWHELP:
      {
        usage();
        return 0;
      }
      break;
    default:
      /* no error */
      break;
    }

  /* No time to set or output format */
  if (string == NULL)
    {
      string = "+%a %b %e %H:%M:%S %Z %Y";      /* From POSIX */
    }
  if (string[0] == '+')
    {
      return showtime(string + 1);      /* Skip plus */
    }
  else
    {
      return settime(string);
    }
}
