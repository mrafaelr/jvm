#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "class.h"
#include "file.h"
#include "util.h"

/* flags */
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

/* get string from constant pool */
static char *
getutf8(ClassFile *class, U2 index)
{
	return class->constant_pool[index].info.utf8_info.bytes;
}

/* print class name */
static void
printclass(ClassFile *class, U2 index)
{
	char *s;

	s = class->constant_pool[class->constant_pool[index].info.class_info.name_index].info.utf8_info.bytes;
	while (*s) {
		if (*s == '/')
			putchar('.');
		else
			putchar(*s);
		s++;
	}
}

/* print source file name */
static void
printsource(ClassFile *class)
{
	Attribute *attr;
	U2 i;

	if ((attr = getattr(class->attributes, class->attributes_count, SourceFile)) == NULL)
		return;
	i = attr->info.sourcefile.sourcefile_index;
	printf("Compiled from \"%s\"\n", getutf8(class, i));
}

/* print class header */
static void
printheader(ClassFile *class)
{
	if (class->access_flags & ACC_PUBLIC)
		printf("public ");
	if (class->access_flags & ACC_INTERFACE) {
		if (class->access_flags & ACC_STRICT)
			printf("strict ");
		printf("interface ");
	} else if (class->access_flags & ACC_ENUM) {
		if (class->access_flags & ACC_STRICT)
			printf("strict ");
		printf("enum ");
	} else {
		if (class->access_flags & ACC_ABSTRACT)
			printf("abstract ");
		else if (class->access_flags & ACC_FINAL)
			printf("final ");
		if (class->access_flags & ACC_STRICT)
			printf("strict ");
		printf("class ");
	}
	printclass(class, class->this_class);
	if (class->super_class) {
		printf(" extends ");
		printclass(class, class->super_class);
		;
	}
	printf(" {\n");
}

/* print type, return next type in descriptor */
static char *
printtype(char *type)
{
	char *s;

	s = type + 1;
	switch (*type) {
	case 'B':
		printf("byte");
		break;
	case 'C':
		printf("char");
		break;
	case 'D':
		printf("double");
		break;
	case 'F':
		printf("float");
		break;
	case 'I':
		printf("int");
		break;
	case 'J':
		printf("long");
		break;
	case 'L':
		while (*s && *s != ';') {
			if (*s == '/')
				putchar('.');
			else
				putchar(*s);
			s++;
		}
		break;
	case 'S':
		printf("short");
		break;
	case 'V':
		printf("void");
		break;
	case 'Z':
		printf("boolean");
		break;
	case '[':
		s = printtype(s);
		printf("[]");
		break;
	}
	return s;
}

/* print descriptor and name */
static void
printdescriptor(char *descriptor, char *name, int init)
{
	char *s;

	s = strrchr(descriptor, ')');
	if (s == NULL) {
		printtype(descriptor);
		printf(" %s", name);
	} else {
		if (!init) {
			printtype(s+1);
			putchar(' ');
		}
		printf("%s(", name);
		s = descriptor + 1;
		while (*s && *s != ')') {
			s = printtype(s);
		}
		putchar(')');
	}
}

/* print field information */
static void
printfield(ClassFile *class, Field *field)
{
	if (!pflag && field->access_flags & ACC_PROTECTED)
		return;
	printf("  ");
	if (field->access_flags & ACC_PRIVATE)
		printf("private ");
	else if (field->access_flags & ACC_PROTECTED)
		printf("protected ");
	else if (field->access_flags & ACC_PUBLIC)
		printf("public ");
	if (field->access_flags & ACC_STATIC)
		printf("static ");
	if (field->access_flags & ACC_FINAL)
		printf("final ");
	if (field->access_flags & ACC_TRANSIENT)
		printf("transient ");
	if (field->access_flags & ACC_VOLATILE)
		printf("volatile ");
	printdescriptor(getutf8(class, field->descriptor_index), getutf8(class, field->name_index), 0);
	printf(";\n");
}

/* print method information */
static void
printmethod(ClassFile *class, Method *method)
{
	char *name;
	int init = 0;

	if (!pflag && method->access_flags & ACC_PROTECTED)
		return;
	name = getutf8(class, method->name_index);
	if (strcmp(name, "<init>") == 0) {
		name = getutf8(class, class->constant_pool[class->this_class].info.class_info.name_index);
		init = 1;
	}
	printf("  ");
	if (method->access_flags & ACC_PRIVATE)
		printf("private ");
	else if (method->access_flags & ACC_PROTECTED)
		printf("protected ");
	else if (method->access_flags & ACC_PUBLIC)
		printf("public ");
	if (method->access_flags & ACC_ABSTRACT)
		printf("abstract ");
	if (method->access_flags & ACC_STATIC)
		printf("static ");
	if (method->access_flags & ACC_FINAL)
		printf("final ");
	if (method->access_flags & ACC_SYNCHRONIZED)
		printf("synchronized ");
	if (method->access_flags & ACC_NATIVE)
		printf("native ");
	if (method->access_flags & ACC_STRICT)
		printf("strict ");
	printdescriptor(getutf8(class, method->descriptor_index), name, init);
	printf(";\n");
}

/* print class contents */
static void
javap(ClassFile *class)
{
	U2 i;

	printsource(class);
	printheader(class);
	for (i = 0; i < class->fields_count; i++)
		printfield(class, &class->fields[i]);
	for (i = 0; i < class->methods_count; i++)
		printmethod(class, &class->methods[i]);
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
			cflag = 1;
			pflag = 1;
		} else {
			usage();
		}
	}
	if (argc == 0)
		usage();
	while (argc--) {
		if ((class = file_read(*argv)) != NULL) {
			javap(class);
			file_free(class);
		} else {
			warnx("%s: %s", *argv, file_geterr());
			exitval = 1;
		}
		argv++;
	}
	return exitval;
}
