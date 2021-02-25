#ifndef _UTIL_H_
#define _UTIL_H_

void setprogname(char *s);
void err(int eval, const char *fmt, ...);
void errx(int eval, const char *fmt, ...);
void warn(const char *fmt, ...);
void warnx(const char *fmt, ...);

#endif /* _UTIL_H_ */
