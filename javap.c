#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "class.h"
#include "util.h"
#include "op.h"
#include "file.h"

/* names */
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
static char *errstr[] = {
	[ERR_NONE] = NULL,
	[ERR_READ] = "could not read file",
	[ERR_ALLOC] = "could not allocate memory",
	[ERR_CODE] = "code does not follow jvm code constraints",
	[ERR_CONSTANT] = "reference to entry of wrong type on constant pool",
	[ERR_DESCRIPTOR] = "invalid descriptor string",
	[ERR_EOF] = "unexpected end of file",
	[ERR_INDEX] = "index to constant pool out of bounds",
	[ERR_KIND] = "invalid method handle reference kind",
	[ERR_MAGIC] = "invalid magic number",
	[ERR_TAG] = "unknown constant pool tag",
};

/* flags */
static int cflag = 0;
static int lflag = 0;
static int pflag = 0;
static int sflag = 0;
static int verbose = 0;

/* show usage */
static void
usage(void)
{
	(void)fprintf(stderr, "usage: javap [-clpsv] classfile...\n");
	exit(EXIT_FAILURE);
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

	s = getclassname(class, index);
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

	if ((attr = getattr(class->attributes, class->attributes_count, SourceFile)) == NULL)
		return;
	printf("Compiled from \"%s\"\n", getutf8(class, attr->info.sourcefile.sourcefile_index));
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
	printf("    ConstantValue: ");
	switch (class->constant_pool[index].tag) {
	case CONSTANT_Integer:
		printf("int %ld", (long)getint(class->constant_pool[index].info.integer_info.bytes));
		break;
	case CONSTANT_Long:
		printf("long %lld", getlong(class->constant_pool[index].info.long_info.high_bytes,
		                            class->constant_pool[index].info.long_info.low_bytes));
		break;
	case CONSTANT_Float:
		printf("float %gd", getfloat(class->constant_pool[index].info.float_info.bytes));
		break;
	case CONSTANT_Double:
		printf("double %gd", getdouble(class->constant_pool[index].info.double_info.high_bytes,
		                               class->constant_pool[index].info.double_info.low_bytes));
		break;
	case CONSTANT_String:
		printf("String %s", getutf8(class, class->constant_pool[index].info.string_info.string_index));
		break;
	}
	putchar('\n');
}

/* print field information */
static void
printfield(ClassFile *class, U2 count)
{
	Field *field;

	field = &class->fields[count];
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
	}
	if (lflag || cflag) {
		putchar('\n');
	}
}

/* print line numbers */
static void
printlinenumbers(LineNumberTable_attribute *lnattr)
{
	LineNumber *ln;
	U2 count, i;

	printf("      LineNumberTable:\n");
	count = lnattr->line_number_table_length;
	ln = lnattr->line_number_table;
	for (i = 0; i < count; i++) {
		printf("        line %u: %u\n", ln[i].line_number, ln[i].start_pc);
	}
}

/* print local variables */
static void
printlocalvars(ClassFile *class, LocalVariableTable_attribute *lvattr)
{
	LocalVariable *lv;
	U2 count, i;

	count = lvattr->local_variable_table_length;
	lv = lvattr->local_variable_table;
	if (count == 0)
		return;
	printf("      LocalVariableTable:\n");
	printf("        Start  Length  Slot  Name   Signature\n");
	for (i = 0; i < count; i++) {
		printf("      %7u %7u %5u %5s   %s\n", lv[i].start_pc, lv[i].length, lv[i].index,
		       getutf8(class, lv[i].name_index), getutf8(class, lv[i].descriptor_index));
	}
}

/* print code of method */
static void
printcode(Code_attribute *codeattr, U2 nargs)
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
		printf("%8u: %s\n", i, op_getname(opcode));
		switch (op_getnoperands(code[i])) {
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
			for (j = 0; i < count && j < op_getnoperands(opcode); j++)
				i++;
			break;
		}
	}
}

/* print method information */
static void
printmethod(ClassFile *class, U2 count)
{
	char *name;
	int init = 0;
	U2 nargs;
	Attribute *cattr;       /* Code_attribute */
	Attribute *lnattr;      /* LineNumberTable_attribute */
	Attribute *lvattr;      /* LocalVariableTable_attribute */
	Method *method;

	method = &class->methods[count];
	if (!pflag && method->access_flags & ACC_PRIVATE)
		return;
	if (count && (lflag || sflag || cflag))
		putchar('\n');
	name = getutf8(class, method->name_index);
	if (strcmp(name, "<init>") == 0) {
		name = getclassname(class, class->this_class);
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
	cattr = getattr(method->attributes, method->attributes_count, Code);
	if (cattr != NULL) {
		lnattr = getattr(cattr->info.code.attributes, cattr->info.code.attributes_count, LineNumberTable);
		lvattr = getattr(cattr->info.code.attributes, cattr->info.code.attributes_count, LocalVariableTable);
		if (cflag) {
			printcode(&cattr->info.code, nargs);
		}
		if (lflag && lnattr != NULL) {
			printlinenumbers(&lnattr->info.linenumbertable);
		}
		if (lflag && lvattr != NULL) {
			printlocalvars(class, &lvattr->info.localvariabletable);
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
	for (i = 0; i < class->fields_count; i++)
		printfield(class, i);
	for (i = 0; i < class->methods_count; i++)
		printmethod(class, i);
	printf("}\n");
}

/* javap: disassemble jclass files */
int
main(int argc, char *argv[])
{
	ClassFile *class;
	FILE *fp;
	int exitval = EXIT_SUCCESS;
	int status;
	int ch;

	setprogname(argv[0]);
	while ((ch = getopt(argc, argv, "clpsv")) != -1) {
		switch (ch) {
		case 'c':
			cflag = 1;
			break;
		case 'l':
			lflag = 1;
			break;
		case 'p':
			pflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		case 'v':
			verbose = 1;
			cflag = 1;
			lflag = 1;
			sflag = 1;
			break;
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc == 0)
		usage();
	class = emalloc(sizeof *class);
	for (; argc--; argv++) {
		if ((fp = fopen(*argv, "rb")) == NULL) {
			warn("%s", *argv);
			exitval = EXIT_FAILURE;
			continue;
		}
		if ((status = file_read(fp, class)) != 0) {
			warnx("%s: %s", *argv, errstr[status]);
			exitval = EXIT_FAILURE;
		} else {
			javap(class);
			file_free(class);
		}
		fclose(fp);
	}
	return exitval;
}
