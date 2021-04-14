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
natprintln(Frame *frame, char *type)
{
	Value vfp, v;

	v = frame_stackpop(frame);
	vfp = frame_stackpop(frame);
	if (strcmp(type, "(Ljava/lang/String;)V") == 0)
		fprintf((FILE *)vfp.v, "%s\n", (char *)v.v);
	else if (strcmp(type, "(B)V") == 0)
		fprintf((FILE *)vfp.v, "%d\n", v.i);
	else if (strcmp(type, "(C)V") == 0)
		fprintf((FILE *)vfp.v, "%c\n", v.i);
	else if (strcmp(type, "(D)V") == 0)
		fprintf((FILE *)vfp.v, "%.16g\n", v.d);
	else if (strcmp(type, "(F)V") == 0)
		fprintf((FILE *)vfp.v, "%.16g\n", v.f);
	else if (strcmp(type, "(I)V") == 0)
		fprintf((FILE *)vfp.v, "%d\n", v.i);
	else if (strcmp(type, "(J)V") == 0)
		fprintf((FILE *)vfp.v, "%lld\n", v.l);
	else if (strcmp(type, "(S)V") == 0)
		fprintf((FILE *)vfp.v, "%d\n", v.i);
	else if (strcmp(type, "(Z)V") == 0)
		fprintf((FILE *)vfp.v, "%d\n", v.i);
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
		if (strcmp(name, "println") == 0) {
			natprintln(frame, type);
			return 0;
		}
	}
	return -1;
}
