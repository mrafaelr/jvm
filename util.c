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

/* call malloc checking for error */
void *
emalloc(size_t size)
{
	void *p;

	if ((p = malloc(size)) == NULL)
		err(1, "malloc");
	return p;
}

/* call calloc checking for error */
void *
ecalloc(size_t nmemb, size_t size)
{
	void *p;

	if ((p = calloc(nmemb, size)) == NULL)
		err(1, "malloc");
	return p;
}

/* print format error message with error string and exit */
void
err(int eval, const char *fmt, ...)
{
	va_list ap;

	if (progname)
		(void)fprintf(stderr, "%s: ", progname);
	va_start(ap, fmt);
	if (fmt) {
		(void)vfprintf(stderr, fmt, ap);
		(void)fprintf(stderr, ": ");
	}
	va_end(ap);
	(void)fprintf(stderr, "%s\n", strerror(errno));
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

	if (progname)
		(void)fprintf(stderr, "%s: ", progname);
	va_start(ap, fmt);
	if (fmt) {
		(void)vfprintf(stderr, fmt, ap);
		(void)fprintf(stderr, ": ");
	}
	va_end(ap);
	(void)fprintf(stderr, "%s\n", strerror(errno));
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
