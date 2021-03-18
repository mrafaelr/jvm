#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "class.h"

/* get attribute with given tag in list of attributes */
Attribute *
getattr(Attribute *attrs, U2 count, AttributeTag tag)
{
	U2 i;

	for (i = 0; i < count; i++)
		if (attrs[i].tag == tag)
			return &attrs[i];
	return NULL;
}

/* get string from constant pool */
char *
getutf8(ClassFile *class, U2 index)
{
	return class->constant_pool[index].info.utf8_info.bytes;
}

/* get string from constant pool */
char *
getclassname(ClassFile *class, U2 index)
{
	return getutf8(class, class->constant_pool[index].info.class_info.name_index);
}

/* get method matching name and descriptor from class */
Method *
getmethod(ClassFile *class, char *name, char *descr)
{
	U2 i;

	for (i = 0; i < class->methods_count; i++)
		if (strcmp(name, getutf8(class, class->methods[i].name_index)) == 0 &&
		    strcmp(descr, getutf8(class, class->methods[i].descriptor_index)) == 0)
			return &class->methods[i];
	return NULL;
}

/* check if super class is java.lang.Object */
int
istopclass(ClassFile *class)
{
	return strcmp(getclassname(class, class->super_class), "java/lang/Object") == 0;
}
