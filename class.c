#include <errno.h>
#include <math.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "class.h"
#include "util.h"

#define MAGIC           0xCAFEBABE

static jmp_buf jmpenv;
static FILE *filep;
static char *filename;

/* error variables */
static int saverrno;
static ErrorTag errtag = ERR_NONE;
static U4 errval;

/* get attribute tag from string */
AttributeTag
getattributetag(char *tagstr)
{
	static struct {
		AttributeTag t;
		char *s;
	} tags[] = {
		{ConstantValue,      "ConstantValue"},
		{Code,               "Code"},
		{Depcreated,         "Depcreated"},
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

/* read count bytes into buf; longjmp to class_read on error */
static void
read(void *buf, U2 count)
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

/* read string of length count into buf; insert a nul at the end of it */
static void
reads(char *buf, U2 count)
{
	read(buf, count);
	buf[count] = '\0';
}

/* read unsigned integer U4 and return it */
static U4
readu(U2 count)
{
	U4 u = 0;
	unsigned char c[4];

	read(c, count);
	switch (count) {
	case 4:
		u = (c[0] << 24) | (c[1] << 16) | (c[2] << 8) | c[3];
		break;
	case 2:
		u = (c[0] << 8) | c[1];
		break;
	default:
		u = c[0];
		break;
	}
	return u;

}

/* read constant pool, return pointer to constant pool array */
static CP *
readcp(U2 count)
{
	CP *cp;
	U2 i;

	cp = ecalloc(count, sizeof *cp);
	for (i = 1; i < count; i++) {
		cp[i].tag = readu(1);
		switch (cp[i].tag) {
		case CONSTANT_Utf8:
			cp[i].info.utf8_info.length = readu(2);
			cp[i].info.utf8_info.bytes = emalloc(cp[i].info.utf8_info.length + 1);
			reads(cp[i].info.utf8_info.bytes, cp[i].info.utf8_info.length);
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
			break;
		case CONSTANT_Double:
			cp[i].info.double_info.high_bytes = readu(4);
			cp[i].info.double_info.low_bytes = readu(4);
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
			cp[i].tag = CONSTANT_Untagged;
			break;
		}
	}
	return cp;
}

/* free class structure */
void
class_free(ClassFile *class)
{
	U2 i;

	for (i = 1; i < class->constant_pool_count; i++) {
		switch (class->constant_pool[i].tag) {
		case CONSTANT_Utf8:
			free(class->constant_pool[i].info.utf8_info.bytes);
			break;
		case CONSTANT_Integer:
			break;
		case CONSTANT_Float:
			break;
		case CONSTANT_Long:
			break;
		case CONSTANT_Double:
			break;
		case CONSTANT_Class:
			break;
		case CONSTANT_String:
			break;
		case CONSTANT_Fieldref:
			break;
		case CONSTANT_Methodref:
			break;
		case CONSTANT_InterfaceMethodref:
			break;
		case CONSTANT_NameAndType:
			break;
		case CONSTANT_MethodHandle:
			break;
		case CONSTANT_MethodType:
			break;
		case CONSTANT_InvokeDynamic:
			break;
		default:
			break;
		}

	}
	free(class->constant_pool);
	free(class);
}

/* read class file */
ClassFile *
class_read(char *s)
{
	U4 u4;
	ClassFile *class = NULL;

	/* open file */
	filep = NULL;
	filename = s;
	if ((filep = fopen(filename, "rb")) == NULL) {
		saverrno = errno;
		errtag = ERR_OPEN;
		goto error;
	}

	/* set jump point */
	if (setjmp(jmpenv))
		goto error;

	/* check magic number */
	u4 = readu(4);
	if (u4 != MAGIC) {
		errtag = ERR_MAGIC;
		errval = u4;
		goto error;
	}

	/* allocate ClassFile structure */
	class = ecalloc(1, sizeof *class);
	
	/* get minor and major version */
	class->minor_version = readu(2);
	class->major_version = readu(2);

	/* read constant pool */
	class->constant_pool_count = readu(2);
	class->constant_pool = readcp(class->constant_pool_count);

	/* close filep and return ClassFile structure */
	fclose(filep);
	return class;

error:
	if (filep != NULL) {
		fclose(filep);
		filep = NULL;
	}
	if (class != NULL) {
		class_free(class);
	}
	return NULL;
}

/* return error string */
char *
class_geterr(void)
{
	static char buf[128];

	switch (errtag) {
	case ERR_NONE:
		return NULL;
	case ERR_OPEN:
	case ERR_READ:
		return strerror(saverrno);
	case ERR_EOF:
		return "unexpected end of file";
	case ERR_MAGIC:
		snprintf(buf, sizeof buf, "invalid magic number \"%0llx\"", (unsigned long long)errval);
		break;
	case ERR_CONSTANT:
		snprintf(buf, sizeof buf, "unknown constant tag \"%0llu\"", (unsigned long long)errval);
		break;
	}
	return buf;
}
