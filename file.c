#include <errno.h>
#include <math.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "class.h"
#include "instr.h"
#include "util.h"

#define MAGIC           0xCAFEBABE

struct FreeStack {
	struct FreeStack *next;
	void *p;
};

enum ErrorTag {
	ERR_NONE = 0,
	ERR_OPEN,
	ERR_READ,
	ERR_ALLOC,
	ERR_CODE,
	ERR_CONSTANT,
	ERR_DESCRIPTOR,
	ERR_EOF,
	ERR_INDEX,
	ERR_KIND,
	ERR_MAGIC,
	ERR_TAG,
};

/* jmp variables */
static jmp_buf jmpenv;
static struct FreeStack *freep = NULL;

/* file variables */
static FILE *filep;

/* error variables */
static int saverrno;
static enum ErrorTag errtag = ERR_NONE;
static char *errstr[] = {
	[ERR_NONE] = NULL,
	[ERR_OPEN] = NULL,
	[ERR_READ] = NULL,
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

/* add pointer to stack of pointers to be freed when an error occurs */
static void
pushfreestack(void *p)
{
	struct FreeStack *f;

	if ((f = malloc(sizeof *f)) == NULL)
		err(1, "malloc");
	f->p = p;
	f->next = freep;
	freep = f;
}

/* free head of stack of pointers to be freed when an error occurs */
static void
popfreestack(void)
{
	struct FreeStack *f;

	f = freep;
	freep = f->next;
	free(f);
}

/* free stack of pointers to be freed when an error occurs */
static void
freestack(void)
{
	struct FreeStack *f;

	while (freep) {
		f = freep;
		freep = f->next;
		free(f->p);
		free(f);
	}
}

/* call malloc; add returned pointer to stack of pointers to be freed when error occurs */
static void *
fmalloc(size_t size)
{
	void *p;

	if ((p = malloc(size)) == NULL) {
		errtag = ERR_ALLOC;
		longjmp(jmpenv, 1);
	}
	pushfreestack(p);
	return p;
}

/* call calloc; add returned pointer to stack of pointers to be freed when error occurs */
static void *
fcalloc(size_t nmemb, size_t size)
{
	void *p;

	if ((p = calloc(nmemb, size)) == NULL) {
		errtag = ERR_ALLOC;
		longjmp(jmpenv, 1);
	}
	pushfreestack(p);
	return p;
}

/* get attribute tag from string */
static AttributeTag
getattributetag(char *tagstr)
{
	static struct {
		AttributeTag t;
		char *s;
	} tags[] = {
		{ConstantValue,      "ConstantValue"},
		{Code,               "Code"},
		{Deprecated,         "Deprecated"},
		{Exceptions,         "Exceptions"},
		{InnerClasses,       "InnerClasses"},
		{SourceFile,         "SourceFile"},
		{Synthetic,          "Synthetic"},
		{LineNumberTable,    "LineNumberTable"},
		{LocalVariableTable, "LocalVariableTable"},
		{UnknownAttribute,   NULL},
	};
	int i;

	for (i = 0; tags[i].s; i++)
		if (strcmp(tagstr, tags[i].s) == 0)
			break;
	return tags[i].t;
}

/* check if string is valid field type, return pointer to next character: */
static int
isdescriptor(char *s)
{
	if (*s == '(') {
		s++;
		while (*s && *s != ')') {
			if (*s == 'L') {
				while (*s != '\0' && *s != ';')
					s++;
				if (*s == '\0')
					return 0;
			} else if (!strchr("BCDFIJSZ[", *s)) {
				return 0;
			}
			s++;
		}
		if (*s != ')')
			return 0;
		s++;
	}
	do {
		if (*s == 'L') {
			while (*s != '\0' && *s != ';')
				s++;
			if (*s == '\0')
				return 0;
		} else if (!strchr("BCDFIJSVZ[", *s)) {
			return 0;
		}
	} while (*s++ == '[');
	return 1;
}

/* check if kind of method handle is valid */
static void
checkkind(U1 kind)
{
	if (kind <= REF_none || kind >= REF_last) {
		errtag = ERR_KIND;
		longjmp(jmpenv, 1);
	}
}

/* check if index is valid and points to a given tag in the constant pool */
static void
checkindex(CP *cp, U2 count, ConstantTag tag, U2 index)
{
	if (index < 1 || index >= count) {
		errtag = ERR_INDEX;
		longjmp(jmpenv, 1);
	}
	if (tag && cp[index].tag != tag) {
		errtag = ERR_CONSTANT;
		longjmp(jmpenv, 1);
	}
}

/* check if index is points to a valid descriptor in the constant pool */
static void
checkdescriptor(CP *cp, U2 count, U2 index)
{
	if (index < 1 || index >= count) {
		errtag = ERR_INDEX;
		longjmp(jmpenv, 1);
	}
	if (cp[index].tag != CONSTANT_Utf8) {
		errtag = ERR_CONSTANT;
		longjmp(jmpenv, 1);
	}
	if (!isdescriptor(cp[index].info.utf8_info.bytes)) {
		errtag = ERR_DESCRIPTOR;
		longjmp(jmpenv, 1);
	}
}

/* check if code follows the JVM code constraints */
static void
checkcode(U1 *code, U4 count)
{
	int32_t j, npairs, high, low;
	U1 opcode;
	U4 i;
	U4 a, b, c, d;

	for (i = 0; i < count; i++) {
		opcode = code[i];
		if (opcode >= CodeLast)
			goto error;
		switch (instrtab[code[i]].noperands) {
		case OP_WIDE:
			if (++i >= count)
				goto error;
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
			default:
				goto error;
				break;
			}
		case OP_LOOKUPSWITCH:
			i++;
			while (i % 4)
				i++;
			i += 4;
			if (i + 4 > count)
				goto error;
			a = code[i++];
			b = code[i++];
			c = code[i++];
			d = code[i];
			npairs = (a << 24) | (b << 16) | (c << 8) | d;
			if (npairs < 0)
				goto error;
			i += 8 * npairs;
			i--;
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
			if (low > high)
				goto error;
			for (j = 0; j < high - low + 1; j++)
				i += 4;
			i--;
			break;
		default:
			for (j = 0; i < count && j < instrtab[opcode].noperands; j++)
				i++;
			break;
		}
	}
	if (i != count)
		goto error;
	return;
error:
	printf("%x\n", code[i]);
	errtag = ERR_CODE;
	longjmp(jmpenv, 1);
}

/* read count bytes into buf; longjmp to class_read on error */
static void
read(void *buf, U4 count)
{
	if (fread(buf, 1, count, filep) != count) {
		if (feof(filep)) {
			errtag = ERR_EOF;
		} else {
			saverrno = errno;
			errtag = ERR_READ;
		}
		longjmp(jmpenv, 1);
	}
}

/* read unsigned integer U4 and return it */
static U4
readu(U2 count)
{
	U4 u = 0;
	U1 b[4];

	read(b, count);
	switch (count) {
	case 4:
		u = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
		break;
	case 2:
		u = (b[0] << 8) | b[1];
		break;
	default:
		u = b[0];
		break;
	}
	return u;

}

/* read string of length count into buf; insert a nul at the end of it */
static char *
reads(U2 count)
{
	char *s;

	s = fmalloc(count + 1);
	read(s, count);
	s[count] = '\0';
	popfreestack();
	return s;
}

/* read index to constant pool and check whether it is a valid index to a given tag */
static U2
readindex(int canbezero, ClassFile *class, ConstantTag tag)
{
	U2 u;
	U1 b[2];

	read(b, 2);
	u = (b[0] << 8) | b[1];
	if (!canbezero || u)
		checkindex(class->constant_pool, class->constant_pool_count, tag, u);
	return u;
}

/* read descriptor index to constant pool and check whether it is a valid */
static U2
readdescriptor(ClassFile *class)
{
	U2 u;
	U1 b[2];

	read(b, 2);
	u = (b[0] << 8) | b[1];
	checkdescriptor(class->constant_pool, class->constant_pool_count, u);
	return u;
}

/* read constant pool, return pointer to constant pool array */
static CP *
readcp(U2 count)
{
	CP *cp;
	U2 i;

	if (count == 0)
		return NULL;
	cp = fcalloc(count, sizeof *cp);
	for (i = 1; i < count; i++) {
		cp[i].tag = readu(1);
		switch (cp[i].tag) {
		case CONSTANT_Utf8:
			cp[i].info.utf8_info.length = readu(2);
			cp[i].info.utf8_info.bytes = reads(cp[i].info.utf8_info.length);
			break;
		case CONSTANT_Integer:
			cp[i].info.integer_info.bytes = readu(4);
			break;
		case CONSTANT_Float:
			cp[i].info.float_info.bytes = readu(4);
			break;
		case CONSTANT_Long:
			cp[i].info.long_info.high_bytes = readu(4);
			cp[i].info.long_info.low_bytes = readu(4);
			i++;
			break;
		case CONSTANT_Double:
			cp[i].info.double_info.high_bytes = readu(4);
			cp[i].info.double_info.low_bytes = readu(4);
			i++;
			break;
		case CONSTANT_Class:
			cp[i].info.class_info.name_index = readu(2);
			break;
		case CONSTANT_String:
			cp[i].info.string_info.string_index = readu(2);
			break;
		case CONSTANT_Fieldref:
			cp[i].info.fieldref_info.class_index = readu(2);
			cp[i].info.fieldref_info.name_and_type_index = readu(2);
			break;
		case CONSTANT_Methodref:
			cp[i].info.methodref_info.class_index = readu(2);
			cp[i].info.methodref_info.name_and_type_index = readu(2);
			break;
		case CONSTANT_InterfaceMethodref:
			cp[i].info.interfacemethodref_info.class_index = readu(2);
			cp[i].info.interfacemethodref_info.name_and_type_index = readu(2);
			break;
		case CONSTANT_NameAndType:
			cp[i].info.nameandtype_info.name_index = readu(2);
			cp[i].info.nameandtype_info.descriptor_index = readu(2);
			break;
		case CONSTANT_MethodHandle:
			cp[i].info.methodhandle_info.reference_kind = readu(1);
			cp[i].info.methodhandle_info.reference_index = readu(2);
			break;
		case CONSTANT_MethodType:
			cp[i].info.methodtype_info.descriptor_index = readu(2);
			break;
		case CONSTANT_InvokeDynamic:
			cp[i].info.invokedynamic_info.bootstrap_method_attr_index = readu(2);
			cp[i].info.invokedynamic_info.name_and_type_index = readu(2);
			break;
		default:
			errtag = ERR_TAG;
			longjmp(jmpenv, 1);
			break;
		}
	}
	popfreestack();
	for (i = 1; i < count; i++) {
		switch (cp[i].tag) {
		case CONSTANT_Utf8:
		case CONSTANT_Integer:
		case CONSTANT_Float:
		case CONSTANT_Long:
		case CONSTANT_Double:
			break;
		case CONSTANT_Class:
			break;
		case CONSTANT_String:
			checkindex(cp, count, CONSTANT_Utf8, cp[i].info.string_info.string_index);
			break;
		case CONSTANT_Fieldref:
			checkindex(cp, count, CONSTANT_Class, cp[i].info.fieldref_info.class_index);
			checkindex(cp, count, CONSTANT_NameAndType, cp[i].info.fieldref_info.name_and_type_index);
			break;
		case CONSTANT_Methodref:
			checkindex(cp, count, CONSTANT_Class, cp[i].info.methodref_info.class_index);
			checkindex(cp, count, CONSTANT_NameAndType, cp[i].info.methodref_info.name_and_type_index);
			break;
		case CONSTANT_InterfaceMethodref:
			checkindex(cp, count, CONSTANT_Class, cp[i].info.interfacemethodref_info.class_index);
			checkindex(cp, count, CONSTANT_NameAndType, cp[i].info.interfacemethodref_info.name_and_type_index);
			break;
		case CONSTANT_NameAndType:
			checkindex(cp, count, CONSTANT_Utf8, cp[i].info.nameandtype_info.name_index);
			checkdescriptor(cp, count, cp[i].info.nameandtype_info.descriptor_index);
			break;
		case CONSTANT_MethodHandle:
			checkkind(cp[i].info.methodhandle_info.reference_kind);
			switch (cp[i].info.methodhandle_info.reference_kind) {
			case REF_getField:
			case REF_getStatic:
			case REF_putField:
			case REF_putStatic:
				checkindex(cp, count, CONSTANT_Fieldref, cp[i].info.methodhandle_info.reference_index);
				break;
			case REF_invokeVirtual:
			case REF_newInvokeSpecial:
				checkindex(cp, count, CONSTANT_Methodref, cp[i].info.methodhandle_info.reference_index);
				break;
			case REF_invokeStatic:
			case REF_invokeSpecial:
				/* TODO check based on ClassFile version */
				break;
			case REF_invokeInterface:
				checkindex(cp, count, CONSTANT_InterfaceMethodref, cp[i].info.methodhandle_info.reference_index);
				break;
			}
			break;
		case CONSTANT_MethodType:
			checkdescriptor(cp, count, cp[i].info.methodtype_info.descriptor_index);
			break;
		case CONSTANT_InvokeDynamic:
			checkindex(cp, count, CONSTANT_NameAndType, cp[i].info.invokedynamic_info.name_and_type_index);
			break;
		default:
			break;
		}
	}
	return cp;
}

/* read interface indices, return pointer to interfaces array */
static U2 *
readinterfaces(U2 count)
{
	U2 *p;
	U2 i;

	if (count == 0)
		return NULL;
	p = fcalloc(count, sizeof *p);
	for (i = 0; i < count; i++)
		p[i] = readu(2);
	popfreestack();
	return p;
}

/* read code instructions, return point to instruction array */
static U1 *
readcode(U4 count)
{
	U1 *code;
	U4 i;

	if (count == 0)
		return NULL;
	code = fmalloc(count);
	for (i = 0; i < count; i++)
		code[i] = readu(1);
	checkcode(code, count);
	popfreestack();
	return code;
}

/* read indices to constant pool, return point to index array */
static U2 *
readindices(U2 count)
{
	U2 *indices;
	U2 i;

	if (count == 0)
		return NULL;
	indices = fcalloc(count, sizeof *indices);
	for (i = 0; i < count; i++)
		indices[i] = readu(2);
	popfreestack();
	return indices;
}

/* read exception table, return point to exception array */
static Exception *
readexceptions(U2 count)
{
	Exception *p;
	U2 i;

	if (count == 0)
		return NULL;
	p = fcalloc(count, sizeof *p);
	for (i = 0; i < count; i++) {
		p[i].start_pc = readu(2);
		p[i].end_pc = readu(2);
		p[i].handler_pc = readu(2);
		p[i].catch_type = readu(2);
	}
	popfreestack();
	return p;
}

/* read inner class table, return point to class array */
static InnerClass *
readclasses(ClassFile *class, U2 count)
{
	InnerClass *p;
	U2 i;

	if (count == 0)
		return NULL;
	p = fcalloc(count, sizeof *p);
	for (i = 0; i < count; i++) {
		p[i].inner_class_info_index = readindex(0, class, CONSTANT_Class);
		p[i].outer_class_info_index = readindex(1, class, CONSTANT_Class);
		p[i].inner_name_index = readindex(1, class, CONSTANT_Utf8);
		p[i].inner_class_access_flags = readu(2);
	}
	popfreestack();
	return p;
}

/* read line number table, return point to LineNumber array */
static LineNumber *
readlinenumber(U2 count)
{
	LineNumber *p;
	U2 i;

	if (count == 0)
		return NULL;
	p = fcalloc(count, sizeof *p);
	for (i = 0; i < count; i++) {
		p[i].start_pc = readu(2);
		p[i].line_number = readu(2);
	}
	popfreestack();
	return p;
}

/* read local variable table, return point to LocalVariable array */
static LocalVariable *
readlocalvariable(ClassFile *class, U2 count)
{
	LocalVariable *p;
	U2 i;

	if (count == 0)
		return NULL;
	p = fcalloc(count, sizeof *p);
	for (i = 0; i < count; i++) {
		p[i].start_pc = readu(2);
		p[i].length = readu(2);
		p[i].name_index = readindex(0, class, CONSTANT_Utf8);
		p[i].descriptor_index = readdescriptor(class);
		p[i].index = readu(2);
	}
	popfreestack();
	return p;
}

/* read attribute list, longjmp to class_read on error */
static Attribute *
readattributes(ClassFile *class, U2 count)
{
	Attribute *p;
	U4 length;
	U2 index;
	U2 i;
	U1 b;

	if (count == 0)
		return NULL;
	p = fcalloc(count, sizeof *p);
	for (i = 0; i < count; i++) {
		index = readindex(0, class, CONSTANT_Utf8);
		length = readu(4);
		p[i].tag = getattributetag(class->constant_pool[index].info.utf8_info.bytes);
		switch (p[i].tag) {
		case ConstantValue:
			p[i].info.constantvalue.constantvalue_index = readindex(0, class, 0);
			break;
		case Code:
			p[i].info.code.max_stack = readu(2);
			p[i].info.code.max_locals = readu(2);
			p[i].info.code.code_length = readu(4);
			p[i].info.code.code = readcode(p[i].info.code.code_length);
			p[i].info.code.exception_table_length = readu(2);
			p[i].info.code.exception_table = readexceptions(p[i].info.code.exception_table_length);
			p[i].info.code.attributes_count = readu(2);
			p[i].info.code.attributes = readattributes(class, p[i].info.code.attributes_count);
			break;
		case Deprecated:
			break;
		case Exceptions:
			p[i].info.exceptions.number_of_exceptions = readu(2);
			p[i].info.exceptions.exception_index_table = readindices(p[i].info.exceptions.number_of_exceptions);
			break;
		case InnerClasses:
			p[i].info.innerclasses.number_of_classes = readu(2);
			p[i].info.innerclasses.classes = readclasses(class, p[i].info.innerclasses.number_of_classes);
			break;
		case SourceFile:
			p[i].info.sourcefile.sourcefile_index = readindex(0, class, CONSTANT_Utf8);
			break;
		case Synthetic:
			break;
		case LineNumberTable:
			p[i].info.linenumbertable.line_number_table_length = readu(2);
			p[i].info.linenumbertable.line_number_table = readlinenumber(p[i].info.linenumbertable.line_number_table_length);
			break;
		case LocalVariableTable:
			p[i].info.localvariabletable.local_variable_table_length = readu(2);
			p[i].info.localvariabletable.local_variable_table = readlocalvariable(class, p[i].info.localvariabletable.local_variable_table_length);
			break;
		case UnknownAttribute:
			while (length-- > 0)
				read(&b, 1);
			break;
		}
	}
	popfreestack();
	return p;
}

/* read fields, reaturn pointer to fields array */
static Field *
readfields(ClassFile *class, U2 count)
{
	Field *p;
	U2 i;

	if (count == 0)
		return NULL;
	p = fcalloc(count, sizeof *p);
	for (i = 0; i < count; i++) {
		p[i].access_flags = readu(2);
		p[i].name_index = readindex(0, class, CONSTANT_Utf8);
		p[i].descriptor_index = readdescriptor(class);
		p[i].attributes_count = readu(2);
		p[i].attributes = readattributes(class, p[i].attributes_count);
	}
	popfreestack();
	return p;
}

/* read methods, reaturn pointer to methods array */
static Method *
readmethods(ClassFile *class, U2 count)
{
	Method *p;
	U2 i;

	if (count == 0)
		return NULL;
	p = fcalloc(count, sizeof *p);
	for (i = 0; i < count; i++) {
		p[i].access_flags = readu(2);
		p[i].name_index = readindex(0, class, CONSTANT_Utf8);
		p[i].descriptor_index = readdescriptor(class);
		p[i].attributes_count = readu(2);
		p[i].attributes = readattributes(class, p[i].attributes_count);
	}
	popfreestack();
	return p;
}

/* free attribute */
static void
attributefree(Attribute *attr, U2 count)
{
	U2 i;

	if (attr == NULL)
		return;
	for (i = 0; i < count; i++) {
		switch (attr[i].tag) {
		case UnknownAttribute:
		case ConstantValue:
		case Deprecated:
		case SourceFile:
		case Synthetic:
			break;
		case Code:
			free(attr[i].info.code.code);
			free(attr[i].info.code.exception_table);
			attributefree(attr[i].info.code.attributes, attr[i].info.code.attributes_count);
			break;
		case Exceptions:
			free(attr[i].info.exceptions.exception_index_table);
			break;
		case InnerClasses:
			free(attr[i].info.innerclasses.classes);
			break;
		case LineNumberTable:
			free(attr[i].info.linenumbertable.line_number_table);
			break;
		case LocalVariableTable:
			free(attr[i].info.localvariabletable.local_variable_table);
			break;
		}
	}
	free(attr);
}

/* free class structure */
void
file_free(ClassFile *class)
{
	U2 i;

	if (class == NULL)
		return;
	if (class->constant_pool)
		for (i = 1; i < class->constant_pool_count; i++)
			if (class->constant_pool[i].tag == CONSTANT_Utf8)
				free(class->constant_pool[i].info.utf8_info.bytes);
	free(class->constant_pool);
	free(class->interfaces);
	if (class->fields)
		for (i = 0; i < class->fields_count; i++)
			attributefree(class->fields[i].attributes, class->fields[i].attributes_count);
	free(class->fields);
	if (class->methods)
		for (i = 0; i < class->methods_count; i++)
			attributefree(class->methods[i].attributes, class->methods[i].attributes_count);
	free(class->methods);
	attributefree(class->attributes, class->attributes_count);
	free(class);
}

/* read class file */
ClassFile *
file_read(char *filename)
{
	ClassFile *class = NULL;
	U4 u;

	if ((filep = fopen(filename, "rb")) == NULL) {
		saverrno = errno;
		errtag = ERR_OPEN;
		goto error;
	}
	if (setjmp(jmpenv))
		goto error;
	if ((u = readu(4)) != MAGIC) {
		errtag = ERR_MAGIC;
		goto error;
	}
	if ((class = calloc(1, sizeof *class)) == NULL) {
		errtag = ERR_ALLOC;
		goto error;
	}
	class->minor_version = readu(2);
	class->major_version = readu(2);
	class->constant_pool_count = readu(2);
	class->constant_pool = readcp(class->constant_pool_count);
	class->access_flags = readu(2);
	class->this_class = readu(2);
	class->super_class = readu(2);
	class->interfaces_count = readu(2);
	class->interfaces = readinterfaces(class->interfaces_count);
	class->fields_count = readu(2);
	class->fields = readfields(class, class->fields_count);
	class->methods_count = readu(2);
	class->methods = readmethods(class, class->methods_count);
	class->attributes_count = readu(2);
	class->attributes = readattributes(class, class->attributes_count);
	fclose(filep);
	return class;
error:
	if (filep != NULL) {
		fclose(filep);
		filep = NULL;
	}
	freestack();
	file_free(class);
	if (errtag == ERR_OPEN || errtag == ERR_READ)
		warnx("%s: %s", filename, strerror(saverrno));
	else
		warnx("%s: %s", filename, errstr[errtag]);
	return NULL;
}
