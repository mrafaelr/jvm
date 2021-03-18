#include <stdint.h>
#include <stdlib.h>
#include "util.h"
#include "class.h"
#include "load.h"
#include "frame.h"

/* call method */
void
code_call(ClassFile *class, char *name, char *descr, U2 access)
{
	Attribute *cattr;       /* Code_attribute */
	Code_attribute *code;
	Method *method;
	Frame *frame;
	U2 i;

	if ((method = class_getmethod(class, name, descr)) == NULL || !(method->access_flags & access))
		errx(EXIT_FAILURE, "could not find method %s in class %s", name, class_getclassname(class, class->this_class));
	if ((cattr = class_getattr(method->attributes, method->attributes_count, Code)) == NULL)
		err(EXIT_FAILURE, "could not find code for method %s", name);
	code = &cattr->info.code;
	if ((frame = frame_push(code, class)) == NULL)
		err(EXIT_FAILURE, "out of memory");
	for (i = 0; i < code->code_length; i++)
		printf("%04x\n", code->code[i]);
	frame_pop();
}
