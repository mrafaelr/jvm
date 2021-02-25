#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

static char *progname;

/* set program name */
void
setprogname(char *s)
{
	progname = s;
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
