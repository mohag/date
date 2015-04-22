/* Minimal POSIX date command implementation

   Note: The behaviour when the input time for setting the time is during a DST transition depends on
   libc's mktime function.

 */

/****************************************************************************
 * <Relative path to the file>
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

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

//#define DATE_ALLOW_NEG_EPOCH 
//#define DATE_ABSOLUTE_MINIMAL
#define DATE_DYNAMIC_OUTSIZE /* Dyanmically resize output buffer */
#define DATE_INITIAL_SIZE 30 /* Enough for a default output */
#define DATE_DYN_GROWTH DATE_INITIAL_SIZE /* Amount to grow by */
#ifdef DATE_DYNAMIC_OUTSIZE
#  define DATE_MAX_OUTSIZE 1000
#else
#  define DATE_MAX_OUTSIZE 100
#endif
/* Replacements for nice to have functions */
#ifdef DATE_ABSOLUTE_MINIMAL
#   define usage() 
#   define error(code,msg,errno) code
#endif

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

#ifndef DATE_ABSOLUTE_MINIMAL
static void usage(void) {
	fprintf(stderr,"Usage:\n");
	fprintf(stderr,"    date [-u] [+format]\n");
	fprintf(stderr,"    date [-u] mmddhhmm[[cc]yy][.ss]\n");
	fprintf(stderr,"    date -h\n\n");
	fprintf(stderr,"Use the given format to display the date or set the date/time.\n");
}

static int error(const int code,const char * message,const int errno_input) {
	fprintf(stderr,"Error: %s",message);
	if (errno_input) {
		fprintf(stderr,": %s\n",strerror(errno_input));
	} else {
		fprintf(stderr,"\n\n");
		/* If we don't have another error, the user was wrong */
		usage();
	}
	return code;
}
#endif

static int settime(char * timestring) {
	struct tm time;
	char * dot;
	struct timespec timetoset;
	short i;
	size_t len;

	dot = NULL;

	timetoset.tv_nsec = 0;
	/* Loop to check that at lesat the general format is correct */
	for (i=0; i < strlen(timestring); i++) {
		if (! isdigit(timestring[i])) {
			if (timestring[i] == '.' && dot == NULL) {
				dot = timestring + i;
			} else {
				return error(4,"invalid character in time string",0);
			}
		}
	}
	if (dot != NULL) {
		*dot++ = '\0'; /* Split off seconds string */
		if (strlen(dot) != 2) {
			return error(5,"syntax error in seconds",0);
		}
		time.tm_sec = atoi(dot);
		if (time.tm_sec > 60) /* This allows for a single leap second as per C11 */
			return error(6,"seconds out of range",0);
	} else {
		time.tm_sec = 0;
	}

	len = strlen(timestring);
	switch(len) {
		case 12:
			time.tm_year = (timestring[8]-'0')*1000+(timestring[9]-'0')*100+(timestring[10]-'0')*10+(timestring[11]-'0');
			time.tm_year -= 1900;
			/* fallthrough */
		case 10:
			if (len == 10) { /* This should be skipped by the previous one, but a break is overkill */
				time.tm_year = (timestring[8]-'0')*10+(timestring[9]-'0');
				if (time.tm_year < 69) /* Two digit handled as per POSIX */
					time.tm_year += 100;
			}
			/* fallthrough */
		case 8:
			time.tm_mon = (timestring[0]-'0')*10+(timestring[1]-'0');
				if (--(time.tm_mon) > 11) /* tm months are zero based */
					return error(9,"invalid month",0);
			time.tm_mday = (timestring[2]-'0')*10+(timestring[3]-'0');
				if (time.tm_mday > 31 || time.tm_mday < 1) 
					return error(10,"invalid day of month",0);
			time.tm_hour = (timestring[4]-'0')*10+(timestring[5]-'0');
				if (time.tm_hour > 23) 
					return error(11,"invalid hour",0);
			time.tm_min = (timestring[6]-'0')*10+(timestring[7]-'0');
				if (time.tm_min > 59) 
					return error(12,"invalid min",0);
		break;
		default:
			return error(7,"invalid time length",0);
	}

	/* Optional extended year check */
	#ifndef DATE_ALLOW_NEG_EPOCH
	if (time.tm_year < 70) 
		return error(8,"negative epoch time",0);
	#endif

	time.tm_isdst = -1;
	time.tm_wday = 0; // 0-6
	time.tm_yday = 0; // 0-365
	timetoset.tv_sec = mktime(&time);
	if (clock_settime(CLOCK_REALTIME,&timetoset) == 0) {
		return 0;
	} else {
		return error(13,"unable to set time",errno);
	}
}

static int showtime(const char * format) {
#ifdef DATE_DYNAMIC_OUTSIZE
	char * buffer;
	size_t buffsize;
#else
	char buffer[DATE_MAX_OUTSIZE];
#endif
	struct tm * nowtm;
	time_t now;
	size_t result;

	now = time(NULL); /* This uses std C time instead of a POSIX function */
	nowtm = localtime(&now);
#ifndef DATE_DYNAMIC_OUTSIZE
#  define buffsize DATE_MAX_OUTSIZE
#else
	buffsize = DATE_INITIAL_SIZE;
	buffer = NULL;
	do {
		buffer = realloc(buffer,buffsize*sizeof(char));
		if (buffer == NULL) {
			return error(15,"unable to allocate buffer!",0);
		}
#endif

		result = strftime(buffer,buffsize,format,nowtm);
#ifdef DATE_DYNAMIC_OUTSIZE
		buffsize += DATE_DYN_GROWTH;
	} while (result == 0 && buffer[0] != 0 && buffsize <= DATE_MAX_OUTSIZE);
#endif
	/* This might incorrectly succeed on error if the system's strftime sets the first character to \0 on failure (The result is unspecified in the standards, so it is possible) */
	if(result == 0 && buffer[0] != 0) { 
		return error(4,"time formatting failed",0);
	}
	if (fprintf(stdout,"%s\n",buffer) > 0 ) { /* Will at least print \n */
#ifdef DATE_DYNAMIC_OUTSIZE
		free(buffer);
		return 0;
	} else {
		free(buffer);
#else
		return 0;
	} else {
#endif
		return error(3,"unable to print result",0);
	}
}

int main(int argc, char **argv) {
	int index;
	char *string = NULL;
	int option;

	while ((option = getopt (argc, argv, "uh")) != -1) {
		switch (option) {
			case 'u':
				if ( setenv("TZ","UTC0",1) != 0) {
					return error(2," Unable to set GMT mode",0);
				} else {
					tzset(); /* Apply changed timezone */
				}
			break;
			case 'h':
				usage();
				return 0;
			break;
			default:
				return error(14,"invalid command line",0);
			break;
		}
	}
	for (index = optind; index < argc; index++) {
		if (string != NULL) {
			return error(1,"too many arguments",0);
		}
		string = argv[index];
	}
	if (string == NULL) { /* No time to set or output format */
		string = "+%a %b %e %H:%M:%S %Z %Y"; /* From POSIX */
	}
	if (string[0] == '+') {
		return showtime(string+1); /* Skip plus */
	} else {
		return settime(string);
	}
}
