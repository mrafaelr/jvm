#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "class.h"
#include "code.h"
#include "load.h"

/* show usage */
static void
usage(void)
{
	(void)fprintf(stderr, "usage: java [-cp classpath] class\n");
	exit(EXIT_FAILURE);
}

/* launches a java application */
int
main(int argc, char *argv[])
{
	ClassFile *class;
	char *cpath = NULL;
	int i;

	setprogname(argv[0]);
	cpath = getenv("CLASSPATH");
	for (i = 1; i < argc && argv[i][0] == '-'; i++) {
		if (strcmp(argv[i], "-cp") == 0) {
			if (++i >= argc)
				usage();
			cpath = argv[i];
		}
	}
	if (i >= argc)
		usage();
	if (cpath == NULL)
		cpath = ".";
	load_setpath(cpath);
	atexit(load_free);
	class = load_class(argv[i]);
	code_call(class, "main", "([Ljava/lang/String;)V", ACC_PUBLIC | ACC_STATIC);
	return 0;
}
