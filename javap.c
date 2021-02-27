#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "class.h"
#include "instr.h"
#include "file.h"
#include "util.h"

static char *cptags[] = {
	[CONSTANT_Untagged]           = "",
	[CONSTANT_Utf8]               = "Utf8",
	[CONSTANT_Integer]            = "Integer",
	[CONSTANT_Float]              = "Float",
	[CONSTANT_Long]               = "Long",
	[CONSTANT_Double]             = "Double",
	[CONSTANT_Class]              = "Class",
	[CONSTANT_String]             = "String",
	[CONSTANT_Fieldref]           = "Fieldref",
	[CONSTANT_Methodref]          = "Methodref",
	[CONSTANT_InterfaceMethodref] = "InterfaceMethodref",
	[CONSTANT_NameAndType]        = "NameAndType",
	[CONSTANT_MethodHandle]       = "MethodHandle",
	[CONSTANT_MethodType]         = "MethodType",
	[CONSTANT_InvokeDynamic]      = "InvokeDynamic",
};

/* flags */
static int cflag = 0;
static int pflag = 0;
static int sflag = 0;
static int verbose = 0;

/* show usage */
static void
usage(void)
{
	(void)fprintf(stderr, "usage: javap [-c|-p|-verbose] classfile...\n");
	exit(EXIT_FAILURE);
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

/* check if super class is java.lang.Object */
static int
istopclass(ClassFile *class)
{
	char *s;

	s = class->constant_pool[class->constant_pool[class->super_class].info.class_info.name_index].info.utf8_info.bytes;
	return strcmp(s, "java/lang/Object") == 0;
}

/* print access flags */
static void
printflags(U2 flags, int type)
{
	static struct {
		char *s;
		U2 flag;
		int type;
	} flagstab[] = {
		{"ACC_PUBLIC",       0x0001, TYPE_CLASS | TYPE_FIELD | TYPE_METHOD | TYPE_INNER },
		{"ACC_PRIVATE",      0x0002,              TYPE_FIELD | TYPE_METHOD | TYPE_INNER },
		{"ACC_PROTECTED",    0x0004,              TYPE_FIELD | TYPE_METHOD | TYPE_INNER },
		{"ACC_STATIC",       0x0008,              TYPE_FIELD | TYPE_METHOD | TYPE_INNER },
		{"ACC_FINAL",        0x0010, TYPE_CLASS | TYPE_FIELD | TYPE_METHOD | TYPE_INNER },
		{"ACC_SUPER",        0x0020, TYPE_CLASS                                         },
		{"ACC_SYNCHRONIZED", 0x0020,                           TYPE_METHOD              },
		{"ACC_VOLATILE",     0x0040,              TYPE_FIELD                            },
		{"ACC_BRIDGE",       0x0040,                           TYPE_METHOD              },
		{"ACC_TRANSIENT",    0x0080,              TYPE_FIELD                            },
		{"ACC_VARARGS",      0x0080,                           TYPE_METHOD              },
		{"ACC_NATIVE",       0x0100,                           TYPE_METHOD              },
		{"ACC_INTERFACE",    0x0200, TYPE_CLASS                            | TYPE_INNER },
		{"ACC_ABSTRACT",     0x0400, TYPE_CLASS              | TYPE_METHOD | TYPE_INNER },
		{"ACC_STRICT",       0x0800,                           TYPE_METHOD              },
		{"ACC_SYNTHETIC",    0x1000, TYPE_CLASS | TYPE_FIELD | TYPE_METHOD | TYPE_INNER },
		{"ACC_ANNOTATION",   0x2000, TYPE_CLASS                            | TYPE_INNER },
		{"ACC_ENUM",         0x4000, TYPE_CLASS | TYPE_FIELD               | TYPE_INNER }
	};
	int p = 0;      /* whether a flag was printed */
	size_t i;

	printf("flags: (0x%04X) ", flags);
	for (i = 0; i < LEN(flagstab); i++) {
		if (flags & flagstab[i].flag && flagstab[i].type & type) {
			if (p)
				printf(", ");
			printf("%s", flagstab[i].s);
			p = 1;
		}
	}
	putchar('\n');
}

/* print content of constant pool */
static void
printcp(ClassFile *class)
{
	CP *cp;
	U2 count, i;
	int d, n;

	printf("Constant pool:\n");
	count = class->constant_pool_count;
	cp = class->constant_pool;
	for (i = 1; i < count; i++) {
		d = 0;
		n = i;
		do {
			d++;
		} while (n /= 10);
		d = (d < 5) ? 5 - d : 0;
		n = d;
		while (n--)
			putchar(' ');
		n = printf("#%d = %s", i, cptags[cp[i].tag]);
		n = (n > 0) ? 27 - (n + d) : 0;
		do {
			putchar(' ');
		} while (n-- > 0);
		switch (cp[i].tag) {
		case CONSTANT_Utf8:
			printf("%s", cp[i].info.utf8_info.bytes);
			break;
		case CONSTANT_Integer:
			printf("%ld", (long)getint(cp[i].info.integer_info.bytes));
			break;
		case CONSTANT_Float:
			printf("%gd", getfloat(cp[i].info.integer_info.bytes));
			break;
		case CONSTANT_Long:
			printf("%lld", getlong(cp[i].info.long_info.high_bytes, cp[i].info.long_info.low_bytes));
			i++;
			break;
		case CONSTANT_Double:
			printf("%gd", getdouble(cp[i].info.long_info.high_bytes, cp[i].info.long_info.low_bytes));
			i++;
			break;
		case CONSTANT_Class:
			n = printf("#%u", cp[i].info.class_info.name_index);
			n = (n > 0 && n < 14) ? 14 - n : 0;
			while (n--)
				putchar(' ');
			printf("// %s", getutf8(class, cp[i].info.class_info.name_index));
			break;
		case CONSTANT_String:
			n = printf("#%u", cp[i].info.string_info.string_index);
			n = (n > 0 && n < 14) ? 14 - n : 0;
			while (n--)
				putchar(' ');
			printf("// %s", getutf8(class, cp[i].info.string_info.string_index));
			break;
		case CONSTANT_Fieldref:
			printf("#%u", cp[i].info.fieldref_info.class_index);
			printf(".#%u", cp[i].info.fieldref_info.name_and_type_index);
			break;
		case CONSTANT_Methodref:
			printf("#%u", cp[i].info.methodref_info.class_index);
			printf(".#%u", cp[i].info.methodref_info.name_and_type_index);
			break;
		case CONSTANT_InterfaceMethodref:
			printf("#%u", cp[i].info.interfacemethodref_info.class_index);
			printf(".#%u", cp[i].info.interfacemethodref_info.name_and_type_index);
			break;
		case CONSTANT_NameAndType:
			printf("#%u", cp[i].info.nameandtype_info.name_index);
			printf(":#%u", cp[i].info.nameandtype_info.descriptor_index);
			break;
		case CONSTANT_MethodHandle:
			printf("%u", cp[i].info.methodhandle_info.reference_kind);
			printf(":#%u", cp[i].info.methodhandle_info.reference_index);
			break;
		case CONSTANT_MethodType:
			printf("#%u", cp[i].info.methodtype_info.descriptor_index);
			break;
		case CONSTANT_InvokeDynamic:
			printf("#%u", cp[i].info.invokedynamic_info.bootstrap_method_attr_index);
			printf(":#%u", cp[i].info.invokedynamic_info.name_and_type_index);
			break;
		}
		putchar('\n');
	}
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

/* print metadata about class */
static void
printmeta(ClassFile *class)
{
	int n;

	printf("  minor version: %u\n", class->minor_version);
	printf("  major version: %u\n", class->major_version);
	printf("  ");
	printflags(class->access_flags, TYPE_CLASS);

	n = printf("  this class: #%u", class->this_class);
	n = (n > 0 && n < 42) ? 42 - n : 0;
	while (n--)
		putchar(' ');
	printf("// ");
	printclass(class, class->this_class);
	printf("\n");

	n = printf("  super class: #%u", class->super_class);
	n = (n > 0 && n < 42) ? 42 - n : 0;
	while (n--)
		putchar(' ');
	printf("// ");
	printclass(class, class->super_class);
	printf("\n");

	printf("  interfaces: %u, fields: %u, methods: %u, attributes: %u\n",
	       class->interfaces_count, class->fields_count, class->methods_count, class->attributes_count);
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
	U2 i;

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
	if (class->super_class && !istopclass(class)) {
		printf(" extends ");
		printclass(class, class->super_class);
		;
	}
	if (class->interfaces_count > 0)
		printf(" implements ");
	for (i = 0; i < class->interfaces_count; i++) {
		if (i > 0)
			printf(", ");
		printclass(class, class->interfaces[i]);
	}
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
		if (*s == ';')
			s++;
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

/* print descriptor and name; return number of parameters */
static U2
printdeclaration(char *descriptor, char *name, int init)
{
	U2 nargs = 0;
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
			if (nargs)
				printf(", ");
			s = printtype(s);
			nargs++;
		}
		putchar(')');
	}
	return nargs;
}

/* print constant value of a field */
static void
printconstant(ClassFile *class, Field *field)
{
	U2 index, i;
	char *s, buf[3];

	if (!(field->access_flags & ACC_STATIC))
		return;
	index = 0;
	for (i = 0; i < field->attributes_count; i++) {
		if (field->attributes[i].tag == ConstantValue) {
			index = field->attributes[i].info.constantvalue.constantvalue_index;
			break;
		}
	}
	if (index == 0)
		return;
	s = getutf8(class, field->descriptor_index);
	printf("    ConstantValue: ");
	switch (*s) {
	case 'B':
		buf[0] = class->constant_pool[index].info.integer_info.bytes;
		buf[1] = class->constant_pool[index].info.integer_info.bytes >> 8;
		buf[2] = '\0';
		printf("byte %s", buf);
		break;
	case 'Z':
		printf("boolean %ld", (long)getint(class->constant_pool[index].info.integer_info.bytes));
		break;
	case 'C':
		printf("char %ld", (long)getint(class->constant_pool[index].info.integer_info.bytes));
		break;
	case 'I':
		printf("int %ld", (long)getint(class->constant_pool[index].info.integer_info.bytes));
		break;
	case 'S':
		printf("short %ld", (long)getint(class->constant_pool[index].info.integer_info.bytes));
		break;
	case 'J':
		printf("long %lld", getlong(class->constant_pool[index].info.long_info.high_bytes,
		                            class->constant_pool[index].info.long_info.low_bytes));
		break;
	case 'F':
		printf("float %gd", getfloat(class->constant_pool[index].info.float_info.bytes));
		break;
	case 'D':
		printf("double %gd", getdouble(class->constant_pool[index].info.double_info.high_bytes,
		                               class->constant_pool[index].info.double_info.low_bytes));
		break;
	}
	putchar('\n');
}

/* print field information */
static void
printfield(ClassFile *class, Field *field)
{
	if (!pflag && field->access_flags & ACC_PRIVATE)
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
	printdeclaration(getutf8(class, field->descriptor_index), getutf8(class, field->name_index), 0);
	printf(";\n");
	if (sflag)
		printf("    descriptor: %s\n", getutf8(class, field->descriptor_index));
	if (verbose) {
		printf("    ");
		printflags(field->access_flags, TYPE_FIELD);
		printconstant(class, field);
		putchar('\n');
	}
}

/* print line numbers */
static void
printlinenumbers(LineNumber *ln, U2 count)
{
	U2 i;

	printf("      LineNumberTable:\n");
	for (i = 0; i < count; i++) {
		printf("        line %u: %u\n", ln[i].line_number, ln[i].start_pc);
	}
}

/* print local variables */
static void
printlocalvars(ClassFile *class, LocalVariable *lv, U2 count)
{
	U2 i;

	printf("      LocalVariableTable:\n");
	printf("        Start  Length  Slot  Name   Signature\n");
	for (i = 0; i < count; i++) {
		printf("      %7u %7u %5u %5s   %s\n", lv[i].start_pc, lv[i].length, lv[i].index,
		                                       getutf8(class, lv[i].name_index),
		                                       getutf8(class, lv[i].descriptor_index));
	}
}

/* print code of method */
static void
printcode(ClassFile *class, Code_attribute *codeattr, U2 nargs)
{
	int32_t j, npairs, high, low;
	U1 *code;
	U1 opcode;
	U4 count;
	U4 a, b, c, d;
	U4 i;

	code = codeattr->code;
	count = codeattr->code_length;
	printf("    Code:\n");
	if (verbose) {
		printf("      stack=%u, locals=%u, args_size=%u", codeattr->max_stack, codeattr->max_locals, nargs);
		putchar('\n');
	}
	for (i = 0; i < count; i++) {
		opcode = code[i];
		if (verbose)
			printf("  ");
		printf("%8u: %s\n", i, instrtab[opcode].name);
		switch (instrtab[code[i]].noperands) {
		case OP_WIDE:
			switch (code[i]) {
			case ILOAD:
			case FLOAD:
			case ALOAD:
			case LLOAD:
			case DLOAD:
			case ISTORE:
			case FSTORE:
			case ASTORE:
			case LSTORE:
			case DSTORE:
			case RET:
				i += 2;
				break;
			case IINC:
				i += 4;
				break;
			}
		case OP_LOOKUPSWITCH:
			i++;
			while (i % 4)
				i++;
			printf("->%u\n", i);
			i += 4;
			a = code[i++];
			b = code[i++];
			c = code[i++];
			d = code[i];
			npairs = (a << 24) | (b << 16) | (c << 8) | d;
			i += 8 * npairs;
			break;
		case OP_TABLESWITCH:
			i++;
			while (i % 4)
				i++;
			i += 4;
			a = code[i++];
			b = code[i++];
			c = code[i++];
			d = code[i++];
			low = (a << 24) | (b << 16) | (c << 8) | d;
			a = code[i++];
			b = code[i++];
			c = code[i++];
			d = code[i];
			high = (a << 24) | (b << 16) | (c << 8) | d;
			for (j = 0; j < high - low + 1; j++)
				i += 4;
			break;
		default:
			for (j = 0; i < count && j < instrtab[opcode].noperands; j++)
				i++;
			break;
		}
	}
	if (verbose) {
		for (i = 0; i < codeattr->attributes_count; i++) {
			if (codeattr->attributes[i].tag == LineNumberTable) {
				printlinenumbers(codeattr->attributes[i].info.linenumbertable.line_number_table,
				                 codeattr->attributes[i].info.linenumbertable.line_number_table_length);
			}
		}
		for (i = 0; i < codeattr->attributes_count; i++) {
			if (codeattr->attributes[i].tag == LocalVariableTable) {
				printlocalvars(class,
				               codeattr->attributes[i].info.localvariabletable.local_variable_table,
				               codeattr->attributes[i].info.localvariabletable.local_variable_table_length);
			}
		}
	}
}

/* print method information */
static void
printmethod(ClassFile *class, Method *method)
{
	char *name;
	int init = 0;
	U2 nargs, i;

	if (!pflag && method->access_flags & ACC_PRIVATE)
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
	nargs = printdeclaration(getutf8(class, method->descriptor_index), name, init);
	if (!(method->access_flags & ACC_STATIC))
		nargs++;
	printf(";\n");
	if (sflag)
		printf("    descriptor: %s\n", getutf8(class, method->descriptor_index));
	if (verbose) {
		printf("    ");
		printflags(method->access_flags, TYPE_METHOD);
	}
	if (cflag) {
		for (i = 0; i < method->attributes_count; i++) {
			if (method->attributes[i].tag == Code) {
				printcode(class, &method->attributes[i].info.code, nargs);
				break;
			}
		}

	}
}

/* print class contents */
static void
javap(ClassFile *class)
{
	U2 i;

	printsource(class);
	printheader(class);
	if (verbose) {
		printf("\n");
		printmeta(class);
		printcp(class);
		printf("{\n");
	} else {
		printf(" {\n");
	}
	for (i = 0; i < class->fields_count; i++) {
		printfield(class, &class->fields[i]);
	}
	for (i = 0; i < class->methods_count; i++) {
		if (i && (sflag || cflag))
			putchar('\n');
		printmethod(class, &class->methods[i]);
	}
	printf("}\n");
}

/* javap: disassemble jclass files */
int
main(int argc, char *argv[])
{
	ClassFile *class;
	int exitval = EXIT_SUCCESS;

	setprogname(argv[0]);
	while (--argc > 0 && (*++argv)[0] == '-') {
		if (strcmp(*argv, "-c") == 0) {
			cflag = 1;
		} else if (strcmp(*argv, "-p") == 0) {
			pflag = 1;
		} else if (strcmp(*argv, "-s") == 0) {
			sflag = 1;
		} else if (strcmp(*argv, "-verbose") == 0) {
			verbose = 1;
			cflag = 1;
			sflag = 1;
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
			exitval = EXIT_FAILURE;
		}
		argv++;
	}
	return exitval;
}
