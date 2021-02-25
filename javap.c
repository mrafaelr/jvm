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

/* get attribute with given tag in list of attributes */
static Attribute *
getattr(Attribute *attrs, U2 count, AttributeTag tag)
{
	U2 i;

	for (i = 0; i < count; i++)
		if (attrs[i].tag == tag)
			return &attrs[i];
	return NULL;
}

/* check whether constant pool index is valid */
static int
validcpindex(ClassFile *class, U2 index)
{
	if (index < 1 || index >= class->constant_pool_count)
		return 0;
	return 1;
}

/* print source file name */
static int
printsource(ClassFile *class)
{
	Attribute *attr;

	if ((attr = getattr(class->attributes, class->attributes_count, SourceFile)) == NULL)
		return 0;
	if (!validcpindex(class, attr->info.sourcefile.sourcefile_index))
		return -1;
	printf("Compiled from \"%s\"\n", "asda");
	return 0;
}

/* print class header */
static void
printclass(ClassFile *class)
{
	(void)class;
}

/* print class contents */
static void
javap(ClassFile *class)
{
	printsource(class);
	printclass(class);
	printf("}\n");
}

/* javap: disassemble jclass files */
int
main(int argc, char *argv[])
{
	ClassFile *class;
	int exitval = 0;

	setprogname(argv[0]);
	while (--argc > 0 && (*++argv)[0] == '-') {
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
