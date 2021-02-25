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

	if (count == 0)
		return NULL;
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

/* read interface indices, return pointer to interfaces array */
static U2 *
readinterfaces(U2 count)
{
	U2 *p;
	U2 i;

	if (count == 0)
		return NULL;
	p = ecalloc(count, sizeof *p);
	for (i = 0; i < count; i++)
		p[i] = readu(2);
	return p;
}

static Attribute *
readattributes(U2 count)
{
	if (count == 0)
		return NULL;
	return NULL;
}

/* read fields, reaturn pointer to fields array */
static Field *
readfields(U2 count)
{
	Field *p;
	U2 i;

	if (count == 0)
		return NULL;
	p = ecalloc(count, sizeof *p);
	for (i = 0; i < count; i++) {
		p[i].access_flags = readu(2);
		p[i].name_index = readu(2);
		p[i].descriptor_index = readu(2);
		p[i].attributes_count = readu(2);
		p[i].attributes = readattributes(p[i].attributes_count);
	}
	return p;
}

/* read methods, reaturn pointer to methods array */
static Method *
readmethods(U2 count)
{
	Method *p;
	U2 i;

	if (count == 0)
		return NULL;
	p = ecalloc(count, sizeof *p);
	for (i = 0; i < count; i++) {
		p[i].access_flags = readu(2);
		p[i].name_index = readu(2);
		p[i].descriptor_index = readu(2);
		p[i].attributes_count = readu(2);
		p[i].attributes = readattributes(p[i].attributes_count);
	}
	return p;
}

/* free attribute */
static void
attributefree(Attribute *attr, U2 count)
{
	(void)count;
	free(attr);
}

/* free class structure */
void
class_free(ClassFile *class)
{
	U2 i;

	if (class == NULL)
		return;
	for (i = 1; i < class->constant_pool_count; i++)
		if (class->constant_pool[i].tag == CONSTANT_Utf8)
			free(class->constant_pool[i].info.utf8_info.bytes);
	free(class->constant_pool);
	free(class->interfaces);
	for (i = 0; i < class->fields_count; i++)
		attributefree(class->fields[i].attributes, class->fields[i].attributes_count);
	free(class->fields);
	for (i = 0; i < class->methods_count; i++)
		attributefree(class->methods[i].attributes, class->methods[i].attributes_count);
	free(class->methods);
	attributefree(class->attributes, class->attributes_count);
	free(class);
}

/* read class file */
ClassFile *
class_read(char *s)
{
	ClassFile *class = NULL;

	filename = s;
	if ((filep = fopen(filename, "rb")) == NULL) {
		saverrno = errno;
		errtag = ERR_OPEN;
		goto error;
	}
	if (setjmp(jmpenv))
		goto error;
	if ((errval = readu(4)) != MAGIC) {
		errtag = ERR_MAGIC;
		goto error;
	}
	class = ecalloc(1, sizeof *class);
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
	class->fields = readfields(class->fields_count);
	class->methods_count = readu(2);
	class->methods = readmethods(class->methods_count);
	class->attributes_count = readu(2);
	class->attributes = readattributes(class->attributes_count);
	fclose(filep);
	return class;
error:
	if (filep != NULL) {
		fclose(filep);
		filep = NULL;
	}
	class_free(class);
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
