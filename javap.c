#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "class.h"
#include "util.h"

static int cflag = 0;
static int pflag = 0;
static int verbose = 0;

/* show usage */
static void
usage(void)
{
	(void)fprintf(stderr, "usage: javap [-c|-p|-verbose] classfile...\n");
	exit(1);
}

/* print class contents */
static void
javap(ClassFile *class)
{
	(void)class;
}

/* javap: disassemble jclass files */
int
main(int argc, char *argv[])
{
	ClassFile *class;
	int exitval = 0;

	setprogname(argv[0]);
	while (--argc && (*++argv)[0] == '-') {
		if (strcmp(*argv, "-c") == 0) {
			cflag = 1;
		} else if (strcmp(*argv, "-p") == 0) {
			pflag = 1;
		} else if (strcmp(*argv, "-verbose") == 0) {
			verbose = 1;
		} else {
			usage();
		}
	}
	if (argc == 0)
		usage();
	while (argc--) {
		if ((class = class_read(*argv)) != NULL) {
			javap(class);
			class_free(class);
		} else {
			warnx("%s: %s", *argv, class_geterr());
			exitval = 1;
		}
		argv++;
	}
	return exitval;
}
