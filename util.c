#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

static char *progname;

int optind = 1;
int optopt;

/* set program name */
void
setprogname(char *s)
{
	progname = s;
}

/* get maximum */
int
max(int x, int y)
{
	return x > y ? x : y;
}

/* get minimum */
int
min(int x, int y)
{
	return x < y ? x : y;
}

/* get options, we do not support ':' on options */
int
getopt(int argc, char * const *argv, const char *options)
{
	static int optpos = 1;

	if (!optind) {                  /* reset */
		optind = 1;
		optpos = 1;
	}
	if (optind >= argc || !argv[optind] || argv[optind][0] != '-') {
		return -1;
	}
	if (strcmp(argv[optind], "--") == 0) {
		optind++;
		return -1;
	}
	optopt = argv[optind][optpos];
	if (strchr(options, argv[optind][optpos]) == NULL) {
		warnx("unknown option -- %c", argv[optind][optpos]);
		return '?';
	}
	if (!argv[optind][++optpos]) {
		optind++;
		optpos = 1;
	}
	return optopt;
}

/* get int32_t from uint32_t */
int32_t
getint(uint32_t bytes)
{
	return *(int32_t *)(&bytes);
}

/* get float from uint32_t */
float
getfloat(uint32_t bytes)
{
	return *(float *)(&bytes);
}

/* get int64_t from uint32_t */
int64_t
getlong(uint32_t high_bytes, uint32_t low_bytes)
{
	uint64_t l;

	l = ((uint64_t)high_bytes << 32) | (uint64_t)low_bytes;
	return *(int64_t *)(&l);
}

/* get double from uint32_t */
double
getdouble(uint32_t high_bytes, uint32_t low_bytes)
{
	uint64_t l;

	l = ((uint64_t)high_bytes << 32) | (uint64_t)low_bytes;
	return *(double *)(&l);
}

/* print format error message with error string and exit */
void
err(int eval, const char *fmt, ...)
{
	va_list ap;
	int saverrno;

	saverrno = errno;
	if (progname)
		(void)fprintf(stderr, "%s: ", progname);
	va_start(ap, fmt);
	if (fmt) {
		(void)vfprintf(stderr, fmt, ap);
		(void)fprintf(stderr, ": ");
	}
	va_end(ap);
	(void)fprintf(stderr, "%s\n", strerror(saverrno));
	exit(eval);
}

/* print format error message and exit */
void
errx(int eval, const char *fmt, ...)
{
	va_list ap;

	if (progname)
		(void)fprintf(stderr, "%s: ", progname);
	va_start(ap, fmt);
	if (fmt)
		(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	exit(eval);
}

/* print format error message with error string */
void
warn(const char *fmt, ...)
{
	va_list ap;
	int saverrno;

	saverrno = errno;
	if (progname)
		(void)fprintf(stderr, "%s: ", progname);
	va_start(ap, fmt);
	if (fmt) {
		(void)vfprintf(stderr, fmt, ap);
		(void)fprintf(stderr, ": ");
	}
	va_end(ap);
	(void)fprintf(stderr, "%s\n", strerror(saverrno));
}

/* print format error message */
void
warnx(const char *fmt, ...)
{
	va_list ap;

	if (progname)
		(void)fprintf(stderr, "%s: ", progname);
	va_start(ap, fmt);
	if (fmt)
		(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
}
