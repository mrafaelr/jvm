#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "class.h"
#include "frame.h"
#include "native.h"

static struct {
	char *name;
	JavaClass jclass;
} jclasstab[] = {
	{"java/lang/System",    LANG_SYSTEM},
	{"java/io/PrintStream", IO_PRINTSTREAM},
	{NULL,                  NONE_CLASS},
};

void
natprintln(Frame *frame)
{
	Value vfp, vstr;

	vstr = frame_stackpop(frame);
	vfp = frame_stackpop(frame);
	fprintf((FILE *)vfp.v, "%s\n", (char *)vstr.v);
}

JavaClass
native_javaclass(char *classname)
{
	size_t i;

	for (i = 0; jclasstab[i].name; i++)
		if (strcmp(classname, jclasstab[i].name) == 0)
			break;
	return jclasstab[i].jclass;
}

void *
native_javaobj(JavaClass jclass, char *objname, char *objtype)
{
	switch (jclass) {
	default:
		break;
	case LANG_SYSTEM:
		if (strcmp(objtype, "Ljava/io/PrintStream;") == 0) {
			if (strcmp(objname, "out") == 0) {
				return stdout;
			} else if (strcmp(objname, "err") == 0) {
				return stderr;
			} else if (strcmp(objname, "in") == 0) {
				return stdin;
			}
		}
		break;
	}
	return NULL;
}

int
native_javamethod(Frame *frame, JavaClass jclass, char *name, char *type)
{
	switch (jclass) {
	default:
		break;
	case IO_PRINTSTREAM:
		if (strcmp(type, "(Ljava/lang/String;)V") == 0) {
			if (strcmp(name, "println") == 0) {
				natprintln(frame);
				return 0;
			}
		}
	}
	return -1;
}
