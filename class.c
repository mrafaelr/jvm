#include <stddef.h>
#include "class.h"

static char *errstr[] = {
	[ERR_NONE] = NULL,
	[ERR_OPEN] = NULL,
	[ERR_READ] = NULL,
	[ERR_EOF] = "unexpected end of file",
	[ERR_CONSTANT] = "reference to entry of wrong type on constant pool",
	[ERR_INDEX] = "index to constant pool out of bounds",
	[ERR_MAGIC] = "invalid magic number",
	[ERR_ALLOC] = "could not allocate memory",
};

/* return error string of current error */
char *
class_strerr(enum ErrorTag errtag)
{
	return errstr[errtag];
}
